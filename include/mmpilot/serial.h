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


namespace mmpilot {

class SerialPort {
public:
	SerialPort() = default;

	~SerialPort() {
		close();
	}

	SerialPort(const SerialPort&) = delete;
	SerialPort& operator=(const SerialPort&) = delete;

	void open(const std::string& path, int baud);

	void close();

	bool isOpen() const {
		return _fd >= 0;
	}

	// Best-effort write all bytes (non-blocking fd; we spin until done or error)
	void writeAll(const uint8_t* data, size_t n);

	// Non-blocking read: returns bytes read (0 if none)
	size_t readSome(uint8_t* out, size_t cap);

private:
	int _fd = -1;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_SERIAL_H_ */
