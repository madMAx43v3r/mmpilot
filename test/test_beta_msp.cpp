/*
 * test_beta_msp.cpp
 *
 *  Created on: Feb 15, 2026
 *      Author: mad
 */

#include <mmpilot/beta_msp.h>

#include <iostream>

using namespace mmpilot;


// ------------------------- Example usage -------------------------

int main(int argc, char** argv) try
{
	if(argc < 2) {
		std::cerr << "Usage: " << argv[0] << " /dev/ttyACM0 [baud]" << std::endl;
		return 2;
	}
	std::string dev = argv[1];
	int baud = (argc >= 3) ? std::stoi(argv[2]) : 115200;

	mmpilot::MspV2Client cli(dev, baud);

	// Poll at ~50 Hz
	while(true) {
		try {
			auto att = cli.request_attitude();
			std::cout << "att: roll=" << att.roll << " pitch=" << att.pitch << " yaw=" << att.yaw << std::endl;
		}
		catch(std::exception& ex) {
			std::cerr << "Error: " << ex.what() << "\n";
		}
		try {
			auto imu = cli.request_raw_imu();
			std::cout << "imu: gyro=[" << imu.gyro[0] << "," << imu.gyro[1] << "," << imu.gyro[2] << "]" << " acc=["
					<< imu.acc[0] << "," << imu.acc[1] << "," << imu.acc[2] << "]" << " mag=[" << imu.mag[0] << ","
					<< imu.mag[1] << "," << imu.mag[2] << "]" << std::endl;
		}
		catch(std::exception& ex) {
			std::cerr << "Error: " << ex.what() << "\n";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
} catch(const std::exception& e) {
	std::cerr << "Fatal: " << e.what() << "\n";
	return 1;
}

