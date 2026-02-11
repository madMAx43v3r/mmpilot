/*
 * binary.h
 *
 *  Created on: Feb 11, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_BINARY_H_
#define INCLUDE_MMPILOT_BINARY_H_

#include <mmpilot/record.h>
#include <mmpilot/replay.h>

#include <vector>
#include <cstdint>


namespace mmpilot {

class Binary : public Sample {
public:
	std::vector<uint8_t> data;

	void write(Recorder& out) const
	{
		out.write_u32(MAGIC);
		out.write(data.data(), data.size());
	}

	void read(Player& in)
	{
		const auto magic = in.read_u32();
		if(magic != MAGIC) {
			throw std::runtime_error("Binary: invalid magic");
		}
		const auto size = in.read_binary_size();
		data.resize(size);
		in.read(data.data(), size);
	}

	static constexpr uint32_t MAGIC = 0x7a2d5c8f;
};


} // mmpilot

#endif /* INCLUDE_MMPILOT_BINARY_H_ */
