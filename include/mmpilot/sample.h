/*
 * sample.h
 *
 *  Created on: Feb 8, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_SAMPLE_H_
#define INCLUDE_MMPILOT_SAMPLE_H_

#include <mmpilot/record.h>


namespace mmpilot {

template<typename T>
void write_sample(Recorder& out, const std::string& topic, const T& data)
{
	out.write_u32(0x3d171f57);
	out.write_u32(1);
	out.write(topic);

	data.write(out);
	out.flush();
}


} // mmpilot

#endif /* INCLUDE_MMPILOT_SAMPLE_H_ */
