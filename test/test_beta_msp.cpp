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

	MSP2Client msp(dev, baud);

	msp.interval = std::chrono::milliseconds(10);

	msp.on_attitude = [](const MSP2Client::Attitude& att) {
		std::cout << "att: ts=" << att.ts
				<< " roll=" << att.roll << " pitch=" << att.pitch << " yaw=" << att.yaw << std::endl;
	};

	msp.on_raw_imu = [](const MSP2Client::RawImu& imu) {
		std::cout << "imu: ts=" << imu.ts
				<< " gyro=[" << imu.gyro[0] << "," << imu.gyro[1] << "," << imu.gyro[2] << "]" << " acc=["
				<< imu.acc[0] << "," << imu.acc[1] << "," << imu.acc[2] << "]" << " mag=[" << imu.mag[0] << ","
				<< imu.mag[1] << "," << imu.mag[2] << "]" << std::endl;
	};

	msp.run();

} catch(const std::exception& e) {
	std::cerr << "Fatal: " << e.what() << "\n";
	return 1;
}

