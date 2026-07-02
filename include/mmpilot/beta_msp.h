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
#include <optional>
#include <functional>


namespace mmpilot {

// MSPv2 framing (INAV/Betaflight-derived):
//   '$' 'X' <type> <flags> <func lo> <func hi> <len lo> <len hi> <payload...> <crc8>
// Where <type> is '<' request, '>' response, '!' error. :contentReference[oaicite:2]{index=2}
// CRC is CRC-8/DVB-S2 (poly 0xD5), computed over bytes starting at <flags> through end of payload. :contentReference[oaicite:3]{index=3}

class MSP2 {
public:
	struct Frame {
		char type = 0;          // '>' response, '!' error (we only parse incoming)
		uint8_t flags = 0;
		uint16_t func = 0;
		int delay_us = 0;		// reponse time
		std::vector<uint8_t> payload;
	};

	static constexpr uint16_t MSP_ATTITUDE = 108;
	static constexpr uint16_t MSP_ALTITUDE = 109;
	static constexpr uint16_t MSP_RAW_IMU  = 102;
	static constexpr uint16_t MSP_RC = 105;
	static constexpr uint16_t MSP_RAW_GPS = 106;
	static constexpr uint16_t MSP_SET_RAW_RC = 200;

	// MSP_ATTITUDE (108 / 0x006C): "2 angles 1 heading"
	// Payload is typically 3x int16: roll, pitch, yaw/heading in decidegrees (0.1°) in MW/BF lineage.
	// (Betaflight uses the same message ID; if you see swapped roll/pitch, just swap in the unpack.)
	class Attitude : public Sample {
	public:
		int16_t roll = 0;		// raw angle (0.1 deg)
		int16_t pitch = 0;		// raw angle (0.1 deg)
		int16_t yaw = 0;		// raw angle (0.1 deg or 1.0 deg)

		void write(Recorder& out) const override {
			out.write_u32(MAGIC);
			out.write_i16(roll);
			out.write_i16(pitch);
			out.write_i16(yaw);
		}

		static std::shared_ptr<Sample> read(Player& in) {
			const auto magic = in.read_u32();
			if(magic != MAGIC) {
				throw std::runtime_error("Attitude: invalid magic");
			}
			auto out = std::make_shared<Attitude>();
			out->roll = in.read_i16();
			out->pitch = in.read_i16();
			out->yaw = in.read_i16();
			return out;
		}
	private:
		static constexpr uint32_t MAGIC = 0x9d292216;
	};

	// MSP_RAW_IMU (102 / 0x0066): acc[3], gyro[3], mag[3] as int16 LE (18 bytes total)
	class RawImu : public Sample {
	public:
		std::array<int16_t, 3> acc = {0, 0, 0};    // raw accel ((8|16)g/4096 default)
		std::array<int16_t, 3> gyro = {0, 0, 0};   // raw gyro rate (1/16.4 deg/s at 2000 dps)
		std::array<int16_t, 3> mag = {0, 0, 0};    // raw mag (0 if not present)

		void write(Recorder& out) const override {
			out.write_u32(MAGIC);
			for(int i = 0; i < 3; ++i) out.write_i16(acc[i]);
			for(int i = 0; i < 3; ++i) out.write_i16(gyro[i]);
			for(int i = 0; i < 3; ++i) out.write_i16(mag[i]);
		}

		static std::shared_ptr<Sample> read(Player& in) {
			const auto magic = in.read_u32();
			if(magic != MAGIC) {
				throw std::runtime_error("RawImu: invalid magic");
			}
			auto out = std::make_shared<RawImu>();
			for(int i = 0; i < 3; ++i) out->acc[i] = in.read_i16();
			for(int i = 0; i < 3; ++i) out->gyro[i] = in.read_i16();
			for(int i = 0; i < 3; ++i) out->mag[i] = in.read_i16();
			return out;
		}
	private:
		static constexpr uint32_t MAGIC = 0x40f0ef3d;
	};

	class RcPacket : public Sample {
	public:
		std::vector<uint16_t> ch;

		uint16_t roll() const     { return ch.size() > 0 ? ch[0] : 0; }
		uint16_t pitch() const    { return ch.size() > 1 ? ch[1] : 0; }
		uint16_t yaw() const      { return ch.size() > 2 ? ch[2] : 0; }
		uint16_t throttle() const { return ch.size() > 3 ? ch[3] : 0; }
		uint16_t aux(size_t i) const { return ch.size() > 4 + i ? ch[4 + i] : 0; }
		size_t num_aux() const    	 { return ch.size() > 4 ? ch.size() - 4 : 0; }

		void write(Recorder& out) const override {
			out.write_u32(MAGIC);
			out.write_u16(ch.size());
			for(auto v : ch) {
				out.write_u16(v);
			}
		}

		static std::shared_ptr<Sample> read(Player& in) {
			const auto magic = in.read_u32();
			if(magic != MAGIC) {
				throw std::runtime_error("RcPacket: invalid magic");
			}
			auto out = std::make_shared<RcPacket>();
			out->ch.resize(in.read_u16());
			for(auto& v : out->ch) {
				v = in.read_u16();
			}
			return out;
		}
	private:
		static constexpr uint32_t MAGIC = 0x8c208142;
	};

	class RawGPS : public Sample {
	public:
		uint8_t  fix_type = 0;
		uint8_t  num_sats = 0;
		int32_t  lat = 0;		// degrees * 1e7
		int32_t  lon = 0;		// degrees * 1e7
		int32_t  alt = 0;		// m
		uint16_t speed = 0;		// cm/s
		uint16_t course = 0;	// deg * 10

		double get_lat() const { return lat * 1e-7; }		// deg
		double get_lon() const { return lon * 1e-7; }		// deg
		double get_alt() const { return alt; }				// m
		double get_speed() const { return speed * 1e-2; }		// m/s
		double get_heading() const { return course * 1e-1; }	// deg

		void write(Recorder& out) const override {
			out.write_u32(MAGIC);
			out.write_u16(0);
			out.write_u16(fix_type);
			out.write_u16(num_sats);
			out.write_i32(lat);
			out.write_i32(lon);
			out.write_i32(alt);
			out.write_u16(speed);
			out.write_u16(course);
		}

		static std::shared_ptr<Sample> read(Player& in) {
			const auto magic = in.read_u32();
			if(magic != MAGIC) {
				throw std::runtime_error("RawGPS: invalid magic");
			}
			const auto version = in.read_u16();
			if(version != 0) {
				throw std::logic_error("RawGPS: invalid version");
			}
			auto out = std::make_shared<RawGPS>();
			out->fix_type = in.read_u16();
			out->num_sats = in.read_u16();
			out->lat = in.read_i32();
			out->lon = in.read_i32();
			out->alt = in.read_i32();
			out->speed = in.read_u16();
			out->course = in.read_u16();
			return out;
		}
	private:
		static constexpr uint32_t MAGIC = 0x5c2332bd;
	};

	class Altitude : public Sample {
	public:
		int32_t alt_cm;					// [cm]
		int16_t vario_cms;				// [cm/s]

		float get_alt() const {
			return alt_cm / 100.f;		// [m]
		}

		void write(Recorder& out) const override {
			out.write_u32(MAGIC);
			out.write_i32(alt_cm);
			out.write_i16(vario_cms);
		}

		static std::shared_ptr<Sample> read(Player& in) {
			const auto magic = in.read_u32();
			if(magic != MAGIC) {
				throw std::runtime_error("Altitude: invalid magic");
			}
			auto out = std::make_shared<Altitude>();
			out->alt_cm = in.read_i32();
			out->vario_cms = in.read_i16();
			return out;
		}
	private:
		static constexpr uint32_t MAGIC = 0x1a4fdac1;
	};


	std::chrono::milliseconds timeout = std::chrono::milliseconds(500);
	std::chrono::milliseconds interval = std::chrono::milliseconds(20);		// update rate for run()

	std::function<void(const RawImu&)> on_raw_imu;
	std::function<void(const Attitude&)> on_attitude;
	std::function<void(const Altitude&)> on_altitude;
	std::function<void(const RcPacket&)> on_rc;
	std::function<void(const RawGPS&)> on_gps;


	MSP2(const std::string& path, int baud = 115200)
	{
		_serial.open(path, baud);
	}

	~MSP2()
	{
		shutdown();
		if(thread.joinable()) {
			thread.join();
		}
	}

	MSP2(const MSP2&) = delete;
	MSP2& operator=(const MSP2&) = delete;

	void send_raw_rc(const std::array<uint16_t, 8>& ch)
	{
		std::vector<uint8_t> payload;
		payload.reserve(16);

		for(uint16_t v : ch) {
			// clamp to normal RC range
			v = std::min(std::max(int(v), 1000), 2000);
			payload.push_back(v & 0xFF);
			payload.push_back(v >> 8);
		}
		send_request(MSP_SET_RAW_RC, payload);
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
		std::map<uint16_t, std::chrono::steady_clock::time_point> pending;
		std::map<uint16_t, std::chrono::steady_clock::time_point> last_send;

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

			const auto check_send = [&](const uint16_t func, const size_t max_pending, std::chrono::milliseconds min_interval = {})
			{
				if(!pending.count(func) || now - pending[func] > timeout / 2) {
					const auto delta = now - last_send[func];
					if(!last_send.count(func) || delta > std::max(interval, min_interval)) {
						if(pending.size() <= max_pending || delta > timeout / 2) {
							send_request(func);
							pending[func] = now;
							last_send[func] = now;
						}
					}
				}
			};

			if(on_raw_imu) {
				check_send(MSP_RAW_IMU, 3);
			}
			if(on_rc) {
				check_send(MSP_RC, 1, std::chrono::milliseconds(100));
			}
			if(on_altitude) {
				check_send(MSP_ALTITUDE, 1, std::chrono::milliseconds(100));
			}
			if(on_gps) {
				check_send(MSP_RAW_GPS, 1, std::chrono::milliseconds(200));
			}
			if(on_attitude) {
				check_send(MSP_ATTITUDE, 1, std::chrono::milliseconds(500));
			}

			for(auto& f : poll()) {
				const auto now = std::chrono::steady_clock::now();
				if(f.type == '>') {
					switch(f.func) {
						case MSP_RAW_IMU: 	if(on_raw_imu) on_raw_imu(parse_raw_imu(f)); break;
						case MSP_ATTITUDE: 	if(on_attitude) on_attitude(parse_attitude(f)); break;
						case MSP_ALTITUDE: 	if(on_altitude) on_altitude(parse_altitude(f)); break;
						case MSP_RC: 		if(on_rc) on_rc(parse_rc(f)); break;
						case MSP_RAW_GPS: 	if(on_gps) on_gps(parse_raw_gps(f)); break;
					}
					pending.erase(f.func);
					last_reply = now;
				}
				// If you want to surface errors: (f.type == '!' && f.func == func)
			}
			std::this_thread::sleep_until(now + std::chrono::microseconds(500));
		}
	}

	void start() {
		std::lock_guard<std::mutex> lock(mutex);

		thread = std::thread([&]() {
			while(!is_shutdown()) {
				try {
					run();
				} catch(std::exception& ex) {
					std::cout << "ERROR: MSP2: " << ex.what() << std::endl;
				}
			}
		});
	}

	void shutdown() {
		std::lock_guard<std::mutex> lock(mutex);
		do_run = false;
	}

	bool is_shutdown() {
		std::lock_guard<std::mutex> lock(mutex);
		return !do_run;
	}

private:
	std::mutex mutex;
	std::mutex send_mutex;
	std::thread thread;
	bool do_run = true;

	SerialPort _serial;
	std::deque<uint8_t> _rx;
	size_t _maxBuf = 64 * 1024;

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

	RcPacket parse_rc(const Frame& f)
	{
		if(f.payload.size() < 8) { // at least 4 channels
			throw std::runtime_error("MSP_RC payload too short");
		}
		if((f.payload.size() & 1) != 0) {
			throw std::runtime_error("MSP_RC payload odd length");
		}
		RcPacket rc;
		rc.ts = get_time_micros();
		const size_t n = f.payload.size() / 2;
		rc.ch.resize(n);
		for(size_t i = 0; i < n; ++i) {
			rc.ch[i] = read_u16_le(f.payload, 2 * i);
		}
		return rc;
	}

	RawGPS parse_raw_gps(const Frame& f)
	{
		// Betaflight RAW_GPS layout:
		// 1 + 1 + 4 + 4 + 2 + 2 + 2 = 16 bytes
		const auto& p = f.payload;
		if(p.size() < 16) {
			throw std::runtime_error("MSP_RAW_GPS payload too short");
		}
		RawGPS gps;
		gps.ts       = get_time_micros();
		gps.fix_type  = p[0];
		gps.num_sats  = p[1];
		gps.lat      = read_i32_le(p, 2);
		gps.lon      = read_i32_le(p, 6);
		gps.alt      = read_i16_le(p, 10);
		gps.speed    = read_u16_le(p, 12);
		gps.course   = read_u16_le(p, 14);
		return gps;
	}

	Altitude parse_altitude(const Frame& f)
	{
		const auto& p = f.payload;
		if(p.size() < 6) {
			throw std::runtime_error("MSP_ALTITUDE too short");
		}
		Altitude out;
		out.ts = get_time_micros();
		out.alt_cm = read_i32_le(p, 0);
		out.vario_cms = read_i16_le(p, 4);
		return out;
	}

	static int16_t read_i16_le(const std::vector<uint8_t>& p, size_t off)
	{
		if(off + 2 > p.size())
			throw std::runtime_error("payload too short");
		return int16_t(uint16_t(p[off]) | (uint16_t(p[off + 1]) << 8));
	}

	static uint16_t read_u16_le(const std::vector<uint8_t>& p, size_t off)
	{
		if(off + 2 > p.size())
			throw std::runtime_error("payload too short");
		return uint16_t(p[off]) | (uint16_t(p[off + 1]) << 8);
	}

	static int32_t read_i32_le(const std::vector<uint8_t>& p, size_t off)
	{
		if(off + 4 > p.size())
			throw std::runtime_error("payload too short");
		return int32_t(
				uint32_t(p[off]) | (uint32_t(p[off + 1]) << 8) | (uint32_t(p[off + 2]) << 16)
						| (uint32_t(p[off + 3]) << 24));
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
