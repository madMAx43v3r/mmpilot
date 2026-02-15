/*
 * beta_msp.h
 *
 *  Created on: Feb 15, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_BETA_MSP_H_
#define INCLUDE_MMPILOT_BETA_MSP_H_

#include <mmpilot/serial.h>

#include <deque>


namespace mmpilot {

// MSPv2 framing (INAV/Betaflight-derived):
//   '$' 'X' <type> <flags> <func lo> <func hi> <len lo> <len hi> <payload...> <crc8>
// Where <type> is '<' request, '>' response, '!' error. :contentReference[oaicite:2]{index=2}
// CRC is CRC-8/DVB-S2 (poly 0xD5), computed over bytes starting at <flags> through end of payload. :contentReference[oaicite:3]{index=3}

class MspV2Client {
public:
	struct Frame {
		char type = 0;          // '>' response, '!' error (we only parse incoming)
		uint8_t flags = 0;
		uint16_t func = 0;
		std::vector<uint8_t> payload;
	};

	static constexpr uint16_t MSP_ATTITUDE = 108; // 0x006C :contentReference[oaicite:8]{index=8}
	static constexpr uint16_t MSP_RAW_IMU  = 102; // 0x0066

	// MSP_ATTITUDE (108 / 0x006C): "2 angles 1 heading" :contentReference[oaicite:7]{index=7}
	// Payload is typically 3x int16: roll, pitch, yaw/heading in decidegrees (0.1°) in MW/BF lineage.
	// (Betaflight uses the same message ID; if you see swapped roll/pitch, just swap in the unpack.)
	struct Attitude {
		int16_t roll = 0;		// raw angle (0.1 deg)
		int16_t pitch = 0;		// raw angle (0.1 deg)
		int16_t yaw = 0;		// raw angle (0.1 deg or 1.0 deg)
	};

	// MSP_RAW_IMU (102 / 0x0066): acc[3], gyro[3], mag[3] as int16 LE (18 bytes total)
	struct RawImu {
		int16_t acc[3] = {0, 0, 0};    // raw accel ((8|16)g/4096 default)
		int16_t gyro[3] = {0, 0, 0};   // raw gyro rate (1/16.4 deg/s at 2000 dps)
		int16_t mag[3] = {0, 0, 0};    // raw mag (0 if not present)
	};

	MspV2Client(const std::string& path, int baud = 115200)
	{
		_serial.open(path, baud);
	}

	// Send an MSPv2 request (type '<'), flags usually 0. :contentReference[oaicite:5]{index=5}
	void send_request(uint16_t func, const std::vector<uint8_t>& payload = {}, uint8_t flags = 0)
	{
		std::vector<uint8_t> pkt;
		pkt.reserve(3 + 1 + 2 + 2 + payload.size() + 1);

		pkt.push_back('$');
		pkt.push_back('X');
		pkt.push_back('<');

		// Header fields (part of CRC region starts at flags)
		pkt.push_back(flags);
		pkt.push_back(uint8_t(func & 0xFF));
		pkt.push_back(uint8_t((func >> 8) & 0xFF));

		uint16_t len = uint16_t(payload.size());
		pkt.push_back(uint8_t(len & 0xFF));
		pkt.push_back(uint8_t((len >> 8) & 0xFF));

		pkt.insert(pkt.end(), payload.begin(), payload.end());

		// CRC is over bytes starting at flags through end payload. :contentReference[oaicite:6]{index=6}
		const uint8_t* crc_begin = pkt.data() + 3;          // flags
		size_t crc_len = pkt.size() - 3;                    // flags..payload
		pkt.push_back(crc8_dvb_s2(crc_begin, crc_len));

		_serial.writeAll(pkt.data(), pkt.size());
	}

	// Pump bytes from UART, parse as many frames as available.
	// Returns all newly decoded frames (responses/errors).
	std::vector<Frame> poll()
	{
		readIntoBuffer();
		std::vector<Frame> out;

		// Robust resync strategy:
		// - We keep a byte deque.
		// - We scan for '$','X', then ensure we have the rest.
		// - On any mismatch / CRC fail, we drop one byte and rescan.
		while(true) {
			// Need at least the fixed header + crc: 3 + 1+2+2 + 1 = 9 bytes
			if(_rx.size() < 9)
				break;

			// Find '$'
			size_t start = 0;
			while(start < _rx.size() && _rx[start] != uint8_t('$'))
				start++;
			if(start > 0) {
				popFront(start);
				if(_rx.size() < 9)
					break;
			}

			// Need '$' 'X' <type>
			if(_rx.size() < 3)
				break;
			if(_rx[0] != uint8_t('$')) {
				popFront(1);
				continue;
			}
			if(_rx[1] != uint8_t('X')) {
				popFront(1);
				continue;
			}
			char type = char(_rx[2]);
			if(type != '>' && type != '!') {
				// Not an incoming frame we care about; could be our own echo or noise. Drop '$' and rescan.
				popFront(1);
				continue;
			}

			// Need header fields: flags + func(2) + len(2)
			if(_rx.size() < 8)
				break;
			uint8_t flags = _rx[3];
			uint16_t func = uint16_t(_rx[4]) | (uint16_t(_rx[5]) << 8);
			uint16_t len = uint16_t(_rx[6]) | (uint16_t(_rx[7]) << 8);

			// Full frame size = 3 + 1+2+2 + len + 1
			size_t total = 8 + size_t(len) + 1;
			if(_rx.size() < total)
				break;

			// Compute CRC over flags..payload (starts at index 3, length = 5 + len)
			std::vector<uint8_t> crc_region;
			crc_region.reserve(5 + len);
			for(size_t i = 3; i < 8 + size_t(len); ++i)
				crc_region.push_back(_rx[i]);
			uint8_t want = crc8_dvb_s2(crc_region.data(), crc_region.size());
			uint8_t got = _rx[8 + size_t(len)];

			if(want != got) {
				// CRC fail -> drop one byte and rescan (strong resync).
				popFront(1);
				continue;
			}

			Frame f;
			f.type = type;
			f.flags = flags;
			f.func = func;
			f.payload.resize(len);
			for(size_t i = 0; i < len; ++i)
				f.payload[i] = _rx[8 + i];

			out.push_back(std::move(f));
			popFront(total);
		}

		return out;
	}

	// Blocking helper: send request, then wait for matching response func with timeout.
	std::optional<Frame> request(uint16_t func,
			std::chrono::milliseconds timeout, const std::vector<uint8_t>& payload = {}, uint8_t flags = 0)
	{
		send_request(func, payload, flags);

		const auto t0 = std::chrono::steady_clock::now();
		while(std::chrono::steady_clock::now() - t0 < timeout) {
			for(auto& f : poll()) {
				if(f.func == func && f.type == '>') {
					return f;
				}
				// If you want to surface errors: (f.type == '!' && f.func == func)
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return std::nullopt;
	}

	Attitude request_attitude(int timeout_ms = 20)
	{
		if(auto f = request(MSP_ATTITUDE, std::chrono::milliseconds(timeout_ms))) {
			return parse_attitude(*f);
		}
		throw std::runtime_error("MspV2Client: timeout");
	}

	RawImu request_raw_imu(int timeout_ms = 20)
	{
		if(auto f = request(MSP_RAW_IMU, std::chrono::milliseconds(timeout_ms))) {
			return parse_raw_imu(*f);
		}
		throw std::runtime_error("MspV2Client: timeout");
	}

private:
	SerialPort _serial;
	std::deque<uint8_t> _rx;
	size_t _maxBuf = 64 * 1024;

	void readIntoBuffer()
	{
		uint8_t tmp[1024];
		for(;;) {
			size_t n = _serial.readSome(tmp, sizeof(tmp));
			if(n == 0)
				break;
			for(size_t i = 0; i < n; ++i) {
				_rx.push_back(tmp[i]);
				// Prevent unbounded growth if link is noisy
				if(_rx.size() > _maxBuf)
					_rx.pop_front();
			}
		}
	}

	void popFront(size_t n)
	{
		for(size_t i = 0; i < n && !_rx.empty(); ++i)
			_rx.pop_front();
	}

	Attitude parse_attitude(const Frame& f)
	{
		if(f.payload.size() < 6) {
			throw std::runtime_error("MSP_ATTITUDE payload too short");
		}
		Attitude a;
		a.roll = read_i16_le(f.payload, 0);
		a.pitch = read_i16_le(f.payload, 2);
		a.yaw = read_i16_le(f.payload, 4);
		return a;
	}

	RawImu parse_raw_imu(const Frame& f)
	{
		if(f.payload.size() < 18) {
			throw std::runtime_error("MSP_RAW_IMU payload too short");
		}
		RawImu imu;
		size_t off = 0;
		for(int i = 0; i < 3; ++i, off += 2)
			imu.acc[i] = read_i16_le(f.payload, off);
		for(int i = 0; i < 3; ++i, off += 2)
			imu.gyro[i] = read_i16_le(f.payload, off);
		for(int i = 0; i < 3; ++i, off += 2)
			imu.mag[i] = read_i16_le(f.payload, off);
		return imu;
	}

	static int16_t read_i16_le(const std::vector<uint8_t>& p, size_t off)
	{
		if(off + 2 > p.size())
			throw std::runtime_error("payload too short");
		return int16_t(uint16_t(p[off]) | (uint16_t(p[off + 1]) << 8));
	}

	static uint8_t crc8_dvb_s2_update(uint8_t crc, uint8_t a)
	{
		crc ^= a;
		for(int i = 0; i < 8; ++i) {
			if(crc & 0x80)
				crc = uint8_t((crc << 1) ^ 0xD5);
			else
				crc = uint8_t(crc << 1);
		}
		return crc;
	}

	static uint8_t crc8_dvb_s2(const uint8_t* data, size_t n)
	{
		uint8_t crc = 0;
		for(size_t i = 0; i < n; ++i)
			crc = crc8_dvb_s2_update(crc, data[i]);
		return crc;
	}

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_BETA_MSP_H_ */
