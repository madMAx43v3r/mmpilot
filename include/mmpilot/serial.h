/*
 * serial.h
 *
 *  Created on: Feb 15, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_SERIAL_H_
#define INCLUDE_MMPILOT_SERIAL_H_

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <optional>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>


namespace mmpilot {

class SerialPort {
public:
	SerialPort() = default;

	~SerialPort() {
		close();
	}

	SerialPort(const SerialPort&) = delete;
	SerialPort& operator=(const SerialPort&) = delete;

	void open(const std::string& path, int baud)
	{
		close();

		_fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
		if(_fd < 0) {
			throw std::runtime_error("open(" + path + "): " + std::string(strerror(errno)));
		}

		termios tio {};
		if(tcgetattr(_fd, &tio) != 0) {
			throw std::runtime_error("tcgetattr: " + std::string(strerror(errno)));
		}

		cfmakeraw(&tio);
		tio.c_cflag |= (CLOCAL | CREAD);
		tio.c_cflag &= ~CRTSCTS;     // no HW flow control
		tio.c_cflag &= ~CSTOPB;      // 1 stop
		tio.c_cflag &= ~PARENB;      // no parity
		tio.c_cflag &= ~CSIZE;
		tio.c_cflag |= CS8;          // 8 data

		// Non-blocking reads, but keep a small VMIN/VTIME for sane behavior if you later switch modes
		tio.c_cc[VMIN] = 0;
		tio.c_cc[VTIME] = 0;

		speed_t sp = baudToSpeed(baud);
		if(cfsetispeed(&tio, sp) != 0 || cfsetospeed(&tio, sp) != 0) {
			throw std::runtime_error("cfsetispeed/cfsetospeed: " + std::string(strerror(errno)));
		}

		if(tcsetattr(_fd, TCSANOW, &tio) != 0) {
			throw std::runtime_error("tcsetattr: " + std::string(strerror(errno)));
		}

		// Flush any stale bytes
		tcflush(_fd, TCIOFLUSH);
	}

	void close()
	{
		if(_fd >= 0) {
			::close(_fd);
			_fd = -1;
		}
	}

	bool isOpen() const {
		return _fd >= 0;
	}

	// Best-effort write all bytes (non-blocking fd; we spin until done or error)
	void writeAll(const uint8_t* data, size_t n)
	{
		size_t off = 0;
		while(off < n) {
			ssize_t rc = ::write(_fd, data + off, n - off);
			if(rc > 0) {
				off += size_t(rc);
				continue;
			}
			if(rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
				// yield a bit
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}
			throw std::runtime_error("SerialPort write: " + std::string(strerror(errno)));
		}
	}

	// Non-blocking read: returns bytes read (0 if none)
	size_t readSome(uint8_t* out, size_t cap)
	{
		ssize_t rc = ::read(_fd, out, cap);
		if(rc > 0)
			return size_t(rc);
		if(rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return 0;
		if(rc < 0)
			throw std::runtime_error("SerialPort read: " + std::string(strerror(errno)));
		return 0; // rc == 0 (EOF) shouldn't happen for tty, treat as 0
	}

private:
	int _fd = -1;

	static speed_t baudToSpeed(int baud)
	{
		switch(baud) {
			case 9600:
				return B9600;
			case 19200:
				return B19200;
			case 38400:
				return B38400;
			case 57600:
				return B57600;
			case 115200:
				return B115200;
#ifdef B230400
			case 230400:
				return B230400;
#endif
#ifdef B460800
			case 460800:
				return B460800;
#endif
#ifdef B921600
			case 921600:
				return B921600;
#endif
			default:
				throw std::runtime_error("Unsupported baud: " + std::to_string(baud));
		}
	}

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_SERIAL_H_ */
