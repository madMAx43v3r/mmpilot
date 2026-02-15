/*
 * beta_msp.h
 *
 *  Created on: Feb 15, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_BETA_MSP_H_
#define INCLUDE_MMPILOT_BETA_MSP_H_

#include <mmpilot/serial.h>
#include <mmpilot/replay.h>
#include <mmpilot/util.h>

#include <deque>
#include <mutex>
#include <thread>
#include <functional>


namespace mmpilot {

// MSPv2 framing (INAV/Betaflight-derived):
//   '$' 'X' <type> <flags> <func lo> <func hi> <len lo> <len hi> <payload...> <crc8>
// Where <type> is '<' request, '>' response, '!' error. :contentReference[oaicite:2]{index=2}
// CRC is CRC-8/DVB-S2 (poly 0xD5), computed over bytes starting at <flags> through end of payload. :contentReference[oaicite:3]{index=3}

class MSP2Client {
public:
	struct Frame {
		char type = 0;          // '>' response, '!' error (we only parse incoming)
		uint8_t flags = 0;
		uint16_t func = 0;
		int delay_us = 0;		// reponse time
		std::vector<uint8_t> payload;
	};

	static constexpr uint16_t MSP_ATTITUDE = 108; // 0x006C :contentReference[oaicite:8]{index=8}
	static constexpr uint16_t MSP_RAW_IMU  = 102; // 0x0066

	// MSP_ATTITUDE (108 / 0x006C): "2 angles 1 heading" :contentReference[oaicite:7]{index=7}
	// Payload is typically 3x int16: roll, pitch, yaw/heading in decidegrees (0.1°) in MW/BF lineage.
	// (Betaflight uses the same message ID; if you see swapped roll/pitch, just swap in the unpack.)
	class Attitude : public Sample {
	public:
		int16_t roll = 0;		// raw angle (0.1 deg)
		int16_t pitch = 0;		// raw angle (0.1 deg)
		int16_t yaw = 0;		// raw angle (0.1 deg or 1.0 deg)

		void write(Recorder& out) const {
			out.write_u32(MAGIC);
			out.write_i32(roll);
			out.write_i32(pitch);
			out.write_i32(yaw);
		}

		static std::shared_ptr<Sample> read(Player& in)
		{
			const auto magic = in.read_u32();
			if(magic != MAGIC) {
				throw std::runtime_error("Attitude: invalid magic");
			}
			auto out = std::make_shared<Attitude>();
			out->roll = in.read_i32();
			out->pitch = in.read_i32();
			out->yaw = in.read_i32();
			return out;
		}
	private:
		static constexpr uint32_t MAGIC = 0x9d292216;
	};

	// MSP_RAW_IMU (102 / 0x0066): acc[3], gyro[3], mag[3] as int16 LE (18 bytes total)
	class RawImu : public Sample {
	public:
		int16_t acc[3] = {0, 0, 0};    // raw accel ((8|16)g/4096 default)
		int16_t gyro[3] = {0, 0, 0};   // raw gyro rate (1/16.4 deg/s at 2000 dps)
		int16_t mag[3] = {0, 0, 0};    // raw mag (0 if not present)

		void write(Recorder& out) const {
			out.write_u32(MAGIC);
			for(int i = 0; i < 3; ++i) out.write_i32(acc[i]);
			for(int i = 0; i < 3; ++i) out.write_i32(gyro[i]);
			for(int i = 0; i < 3; ++i) out.write_i32(mag[i]);
		}

		static std::shared_ptr<Sample> read(Player& in)
		{
			const auto magic = in.read_u32();
			if(magic != MAGIC) {
				throw std::runtime_error("RawImu: invalid magic");
			}
			auto out = std::make_shared<RawImu>();
			for(int i = 0; i < 3; ++i) out->acc[i] = in.read_i32();
			for(int i = 0; i < 3; ++i) out->gyro[i] = in.read_i32();
			for(int i = 0; i < 3; ++i) out->mag[i] = in.read_i32();
			return out;
		}
	private:
		static constexpr uint32_t MAGIC = 0x40f0ef3d;
	};

	std::chrono::milliseconds timeout = std::chrono::milliseconds(500);
	std::chrono::milliseconds interval = std::chrono::milliseconds(10);		// update rate for run()

	std::function<void(const RawImu&)> on_raw_imu;
	std::function<void(const Attitude&)> on_attitude;


	MSP2Client(const std::string& path, int baud = 115200)
	{
		_serial.open(path, baud);
	}

	~MSP2Client()
	{
		shutdown();
		if(send_thread.joinable()) {
			send_thread.join();
		}
	}

	// Send an MSPv2 request (type '<'), flags usually 0. :contentReference[oaicite:5]{index=5}
	void send_request(const uint16_t func, const std::vector<uint8_t>& payload = {}, uint8_t flags = 0)
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

		std::lock_guard<std::mutex> lock(send_mutex);

		_serial.writeAll(pkt.data(), pkt.size());

		last_send[func] = std::chrono::steady_clock::now();
	}

	// Pump bytes from UART, parse as many frames as available.
	// Returns all newly decoded frames (responses/errors).
	std::vector<Frame> poll()
	{
		std::lock_guard<std::mutex> lock(mutex);

		read_serial();

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
				consume(start);
				if(_rx.size() < 9)
					break;
			}

			// Need '$' 'X' <type>
			if(_rx.size() < 3)
				break;
			if(_rx[0] != uint8_t('$')) {
				consume(1);
				continue;
			}
			if(_rx[1] != uint8_t('X')) {
				consume(1);
				continue;
			}
			char type = char(_rx[2]);
			if(type != '>' && type != '!') {
				// Not an incoming frame we care about; could be our own echo or noise. Drop '$' and rescan.
				consume(1);
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
			uint8_t crc = 0;
			for(size_t i = 3; i < 8 + size_t(len); ++i) {
				crc = crc8_dvb_s2_update(crc, _rx[i]);
			}
			const uint8_t want = crc;
			const uint8_t got = _rx[8 + size_t(len)];

			if(want != got) {
				// CRC fail -> drop one byte and rescan (strong resync).
				consume(1);
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
			consume(total);
		}

		return out;
	}

	std::optional<Frame> request(uint16_t func,
			const std::chrono::milliseconds timeout,
			const std::vector<uint8_t>& payload = {}, uint8_t flags = 0)
	{
		const auto begin = get_time_micros();

		send_request(func, payload, flags);

		const auto t0 = std::chrono::steady_clock::now();
		while(std::chrono::steady_clock::now() - t0 < timeout) {
			for(auto& f : poll()) {
				if(f.func == func && f.type == '>') {
					f.delay_us = get_time_micros() - begin;
//					std::cout << "delay = " << f.delay_us / 1e3 << " ms" << std::endl;
					return f;
				}
				// If you want to surface errors: (f.type == '!' && f.func == func)
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return std::nullopt;
	}

	void run()
	{
		auto last_reply = std::chrono::steady_clock::now();
		while(true) {
			{
				std::lock_guard<std::mutex> lock(mutex);
				if(!do_run) {
					break;
				}
			}
			const auto now = std::chrono::steady_clock::now();
			if(now - last_reply > timeout) {
				throw std::runtime_error("MSP2Client::run(): timeout");
			}

			if(on_raw_imu && now - get_last_send(MSP_RAW_IMU) > timeout / 2) {
				send_request(MSP_RAW_IMU);
			}
			if(on_attitude && now - get_last_send(MSP_ATTITUDE) > timeout / 2) {
				send_request(MSP_ATTITUDE);
			}

			for(auto& f : poll()) {
				const auto now = std::chrono::steady_clock::now();
				if(f.type == '>') {
					switch(f.func) {
						case MSP_RAW_IMU:
							if(on_raw_imu) {
								send_request(f.func);
								on_raw_imu(parse_raw_imu(f));
							}
							break;
						case MSP_ATTITUDE:
							if(on_attitude) {
								send_request(f.func);
								on_attitude(parse_attitude(f));
							}
							break;
					}
					last_reply = now;
				}
				// If you want to surface errors: (f.type == '!' && f.func == func)
			}
			std::this_thread::sleep_until(now + interval);
		}
	}

	void shutdown() {
		std::lock_guard<std::mutex> lock(mutex);
		do_run = false;
	}

	Attitude req_attitude()
	{
		if(auto f = request(MSP_ATTITUDE, timeout)) {
			return parse_attitude(*f);
		}
		throw std::runtime_error("MSP2Client: timeout");
	}

	RawImu req_raw_imu()
	{
		if(auto f = request(MSP_RAW_IMU, timeout)) {
			return parse_raw_imu(*f);
		}
		throw std::runtime_error("MSP2Client: timeout");
	}

private:
	std::mutex mutex;
	std::mutex send_mutex;
	std::thread send_thread;
	std::map<uint16_t, std::chrono::steady_clock::time_point> last_send;
	bool do_run = true;

	SerialPort _serial;
	std::deque<uint8_t> _rx;
	size_t _maxBuf = 64 * 1024;

	void send_loop()
	{
		std::vector<uint16_t> request;
		{
			std::lock_guard<std::mutex> lock(mutex);
			if(on_raw_imu) {
				request.push_back(MSP_RAW_IMU);
			}
			if(on_attitude) {
				request.push_back(MSP_ATTITUDE);
			}
		}
		size_t seq = 0;
		while(true) {
			{
				std::lock_guard<std::mutex> lock(mutex);
				if(!do_run) {
					break;
				}
			}
			for(auto func : request) {
				send_request(func);
			}
			std::this_thread::sleep_for(interval);

			if(seq++ > 100) {
				std::cout << "SEND STOP" << std::endl;
				break;
			}
		}
	}

	std::chrono::steady_clock::time_point get_last_send(uint16_t func)
	{
		std::lock_guard<std::mutex> lock(send_mutex);
		return last_send[func];
	}

	void read_serial()
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

	void consume(size_t n)
	{
		for(size_t i = 0; i < n && !_rx.empty(); ++i) {
			_rx.pop_front();
		}
	}

	Attitude parse_attitude(const Frame& f)
	{
		if(f.payload.size() < 6) {
			throw std::runtime_error("MSP_ATTITUDE payload too short");
		}
		Attitude att;
		att.ts = get_time_micros();
		att.roll = read_i16_le(f.payload, 0);
		att.pitch = read_i16_le(f.payload, 2);
		att.yaw = read_i16_le(f.payload, 4);
		return att;
	}

	RawImu parse_raw_imu(const Frame& f)
	{
		if(f.payload.size() < 18) {
			throw std::runtime_error("MSP_RAW_IMU payload too short");
		}
		RawImu imu;
		imu.ts = get_time_micros();

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
