/*
 * test_controller.cpp
 *
 *  Created on: Jul 4, 2026
 *      Author: mad
 */

#include <mmpilot/control.h>


#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>


class Mass1D {
public:
	float mass = 1.0f;
	float drag = 0.2f;

	float x = 0;
	float v = 0;

	void reset(float x_ = 0, float v_ = 0)
	{
		x = x_;
		v = v_;
	}

	void update(float force, float dt)
	{
		const float a = (force - drag * v) / mass;

		v += a * dt;
		x += v * dt;
	}
};

int main()
{
	const float dt = 0.2;
	const float t_end = 20;
	const float target = 1;

	mmpilot::ControlVar ctrl;
	ctrl.gain = 1;
	ctrl.damping = 2;
	ctrl.target_time = 3;
	ctrl.set_limit(-10, 10, 2);
	ctrl.reset(0);

	Mass1D plant;
	plant.mass = 2;
	plant.drag = 0.1;
	plant.reset(0, 0);

	std::ofstream file("step_response.csv");
	file << "t, a, x, vel" << std::endl;

	for(float t = 0; t <= t_end; t += dt)
	{
//		const float err = target - plant.x;

		const float u = ctrl.update(target, plant.x, dt);

		plant.update(u, dt);

		file << std::fixed << std::setprecision(6)
				<< t << ", " << u << ", " << plant.x << ", " << plant.v << std::endl;
	}

	std::cout << "wrote step_response.csv" << std::endl;
	return 0;
}


