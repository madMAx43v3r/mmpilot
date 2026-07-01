/*
 * value.h
 *
 *  Created on: Jun 29, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_VALUE_H_
#define INCLUDE_MMPILOT_VALUE_H_

#include <string>
#include <memory>
#include <cstdint>


namespace mmpilot {

class Value {
public:
	virtual ~Value() {}

	virtual std::string to_string() const {
		return "null";
	}
};

class Pointer : public Value {
public:
	std::shared_ptr<Value> value;

	template<typename T>
	std::shared_ptr<T> get() const {
		return std::dynamic_pointer_cast<T>(value);
	}

	std::shared_ptr<Value>& operator=(const std::shared_ptr<Value>& v) {
		value = v;
		return value;
	}

	operator bool() const {
		return bool(value);
	}

	template<typename T>
	T& operator->() const {
		if(!value) {
			throw std::runtime_error("nullptr exception");
		}
		if(auto v = get<T>()) {
			return *v;
		}
		throw std::runtime_error("Pointer: type mismatch");
	}

	std::string to_string() const override {
		if(value) {
			return value->to_string();
		}
		return "null";
	}
};

class ConstPointer : public Value {
public:
	std::shared_ptr<const Value> value;

	template<typename T>
	std::shared_ptr<const T> get() const {
		return std::dynamic_pointer_cast<const T>(value);
	}

	std::shared_ptr<const Value>& operator=(const std::shared_ptr<const Value>& v) {
		value = v;
		return value;
	}

	operator bool() const {
		return bool(value);
	}

	template<typename T>
	const T& operator->() const {
		if(!value) {
			throw std::runtime_error("nullptr exception");
		}
		if(auto v = get<T>()) {
			return *v;
		}
		throw std::runtime_error("ConstPointer: type mismatch");
	}

	std::string to_string() const override {
		if(value) {
			return value->to_string();
		}
		return "null";
	}
};

class String : public Value {
public:
	std::string value;

	String() = default;
	String(const std::string& v) : value(v) {}

	std::string& operator=(const std::string& v) {
		value = v;
		return value;
	}

	std::string to_string() const override {
		return value;
	}
};

template<typename T>
class Integral : public Value {
public:
	T value = T();

	Integral() = default;
	Integral(const T& v) : value(v) {}

	operator T() const {
		return value;
	}

	T& operator=(const T& v) {
		value = v;
		return value;
	}

	std::string to_string() const override {
		return std::to_string(value);
	}
};

using Integer = Integral<int>;
using Integer64 = Integral<int64_t>;
using Bool    = Integral<bool>;
using Float   = Integral<float>;
using Double  = Integral<double>;



} // mmpilot

#endif /* INCLUDE_MMPILOT_VALUE_H_ */
