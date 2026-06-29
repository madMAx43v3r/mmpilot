/*
 * stage.h
 *
 *  Created on: Jun 29, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_STAGE_H_
#define INCLUDE_MMPILOT_STAGE_H_

#include <mmpilot/value.h>

#include <string>
#include <memory>
#include <map>


namespace mmpilot {

class Stage {
public:
	const std::string stage_name;

	const Stage* prev_stage = nullptr;

	Stage(const std::string& name) : stage_name(name) {}

	virtual ~Stage() {}

	virtual void init() {};

	virtual void exec() {};

	template<typename T>
	const T* find_output(const std::string& name) const
	{
		auto it = output_map.find(name);
		if(it != output_map.end()) {
			return dynamic_cast<const T*>(it->second);
		}
		return nullptr;
	}

protected:
	template<typename T>
	const T* find_stage(const std::string& name) const
	{
		auto stage = prev_stage;
		while(stage) {
			if(stage->stage_name == name) {
				return dynamic_cast<const T*>(stage);
			}
			stage = stage->prev_stage;
		}
		return nullptr;
	}

	template<typename T>
	const T* get_stage(const std::string& name) const
	{
		if(auto stage = find_stage<T>(name)) {
			return stage;
		}
		throw std::runtime_error("missing stage: " + name);
	}

	template<typename T>
	const T* find_input(const std::string& name) const
	{
		auto stage = prev_stage;
		while(stage) {
			if(auto value = stage->find_output<T>(name)) {
				return value;
			}
			stage = stage->prev_stage;
		}
		return nullptr;
	}

	template<typename T>
	const T& get_input(const std::string& name) const
	{
		if(auto value = find_input<T>(name)) {
			return *value;
		}
		throw std::runtime_error("missing input: " + name);
	}

	void add_output(const std::string& name, const void* value) {
		output_map[name] = value;
	}

private:
	std::map<std::string, const void*> output_map;

};



} // mmpilot

#endif /* INCLUDE_MMPILOT_STAGE_H_ */
