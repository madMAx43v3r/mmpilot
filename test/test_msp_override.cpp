/*
 * test_msp_override.cpp
 *
 *  Created on: Jun 24, 2026
 *      Author: mad
 */

#include <mmpilot/beta_msp.h>
#include <thread>
#include <array>

using namespace mmpilot;

int main()
{
	MSP2 msp("/dev/ttyAMA0", 115200);

	std::array<uint16_t, 8> rc = {
		1500, // roll
		1500, // pitch
		1337, // throttle
		1500, // yaw
	};

	while(true) {
		msp.send_raw_rc(rc);
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}


