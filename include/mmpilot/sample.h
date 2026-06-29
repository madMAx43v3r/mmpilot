/*
 * sample.h
 *
 *  Created on: Feb 8, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_SAMPLE_H_
#define INCLUDE_MMPILOT_SAMPLE_H_

#include <mmpilot/record.h>
#include <mmpilot/thread.h>
#include <mmpilot/value.h>
#include <mmpilot/util.h>

#include <string>
#include <functional>


namespace mmpilot {

class Sample : public Value {
public:
	int64_t ts;				// [us]
	std::string topic;

	virtual ~Sample() {}

	std::string to_string() const override {
		return "Sample(" + topic + ")";
	}
};


template<typename T>
void write_sample(Recorder& out, const std::string& topic, const T& data)
{
	out.write_u32(0x3d171f57);
	out.write_u32(0);
	out.write_i64(get_time_micros());
	out.write(topic);

	data.write(out);
	out.flush();
}

template<typename T>
auto dispatch(const std::function<void(std::shared_ptr<T>)>& handler)
{
	return [handler](const std::shared_ptr<Sample>& sample)
	{
		if(auto out = std::dynamic_pointer_cast<T>(sample)) {
			handler(out);
		}
	};
}


} // mmpilot

#endif /* INCLUDE_MMPILOT_SAMPLE_H_ */
