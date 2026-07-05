/*
 * model_stage.h
 *
 *  Created on: Jul 5, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_MODEL_STAGE_H_
#define INCLUDE_MMPILOT_MODEL_STAGE_H_

#include <mmpilot/stage.h>
#include <mmpilot/gyro.h>
#include <mmpilot/model.h>
#include <mmpilot/control.h>
#include <mmpilot/pipeline.h>


namespace mmpilot {

class ModelStage : public Stage {
public:
	Model model;

	Velocity out;

	int64_t step_size = 10 * 1000;		// [usec]

	float AGL_min = 1;					// [m]


	ModelStage() : Stage("model") {}

	void init() override
	{
		pipe = find_stage<Pipeline>("root");

		model.reset();
	}

	void exec() override
	{
		integrate();

		const float dt = get_input<Float>("dt");
		const float AGL = get_input<Float>("AGL");
		const float cam_fpx = get_input<Float>("cam_fpx");

		const Gyro::State gyro = get_input<Gyro::State>("gyro");
		const ImageVelocity affine = get_input<ImageVelocity>("affine_vel");

		const float factor = AGL / cam_fpx;

		// convert to [m/s]
		const Vec3f affine_vel = Vec3f(
				-1 * affine.xy.x() * factor,
				 1 * affine.xy.y() * factor,
				 1 * affine.z * AGL
		);

		if(AGL > AGL_min) {
			model.update(affine_vel, dt);
		}
		out.xy.x() = model.velocity.x();
		out.xy.y() = model.velocity.y();
		out.z      = model.velocity.z();
		out.yaw_rate = gyro.rates.z();

		std::cout << "Velocity: xy = " << out.xy.transpose() << " m/s, z = " << out.z << " m/s" << std::endl;

		std::cout << "ModelBias: " << model.accel_bias.transpose() << " m/s^2" << std::endl;
		std::cout << "ModelError: " << model.error.transpose() << " m/s" << std::endl;
	}

	void integrate()
	{
		const int64_t ts_now  = get_input<Integer64>("ts");
		const int64_t ts_last = get_input<Integer64>("last_ts");

		if(ts_last && pipe) {
			// integrate
			for(auto ts = ts_last; ts < ts_now;)
			{
				const auto ts_end = std::min(ts + step_size, ts_now);
				const auto ts_mid = (ts + ts_end) / 2;

				const auto gyro = pipe->gyro_api.lookup(ts_mid);

				const float dt = (ts_end - ts) * 1e-6;
				if(dt > 0) {
					model.step(gyro, dt);
				}
				ts = ts_end;
			}
		}
	}

private:
	const Pipeline* pipe = nullptr;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_MODEL_STAGE_H_ */
