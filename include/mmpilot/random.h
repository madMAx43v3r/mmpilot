/*
 * random.h
 *
 *  Created on: Mar 10, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_RANDOM_H_
#define INCLUDE_MMPILOT_RANDOM_H_

#include <random>
#include <type_traits>


namespace mmpilot {

// thread-local RNG
inline std::mt19937_64& global_rng()
{
	static thread_local std::mt19937_64 rng(std::random_device {}());
	return rng;
}

// uniform distribution [low, high]
template<typename T>
T rand_range(const T low, const T high)
{
	static_assert(std::is_floating_point<T>::value, "rand_range requires floating point type");

	std::uniform_real_distribution<T> dist(low, high);
	return dist(global_rng());
}

// gaussian distribution around mean
template<typename T>
T rand_gaussian(const T mean, const T sigma)
{
	static_assert(std::is_floating_point<T>::value, "rand_gaussian requires floating point type");

	std::normal_distribution<T> dist(mean, sigma);
	return dist(global_rng());
}



} // mmpilot

#endif /* INCLUDE_MMPILOT_RANDOM_H_ */
