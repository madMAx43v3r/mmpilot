/*
 * agl_stage.h
 *
 *  Created on: Jun 30, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_AGL_STAGE_H_
#define INCLUDE_MMPILOT_AGL_STAGE_H_

#include <mmpilot/stage.h>
#include <mmpilot/control.h>
#include <mmpilot/beta_msp.h>
#include <mmpilot/math.h>
#include <mmpilot/gps.h>
#include <mmpilot/affine.h>


namespace mmpilot {

/*
 * Computes altitude over ground (AGL) via GPS and Barometer.
 * Localization will override AGL when available in a later stage.
 */

class AGL_Stage : public Stage {
public:
	float gain = 0.1;				// update gain

	float min_gps_speed = 1;		// [m/s]
	float min_affine_vel = 10;		// [pix/s]


	AGL_Stage() : Stage("AGL") {}

	Float AGL_out = 0;				// altitude over ground [m]

	String AGL_source {"NONE"};		// GPS_CAM | BARO | CAM | GPS | NONE


	void init() override
	{
		add_output("AGL", &AGL_out);
		add_output("AGL_source", &AGL_source);
	}

	void exec() override
	{
		const auto gps   = get_input<ConstPointer>("gps").get<GPS::State>();
		const auto baro  = get_input<ConstPointer>("msp_alt").get<MSP2::Altitude>();
		const auto delta = get_input<Affine::Params>("affine");
		const auto vel   = get_input<ImageVelocity>("affine_vel");

		if(gps  && gps->fix_type >= 1
				&& gps->speed_ms > min_gps_speed
				&& vel.xy.norm() > min_affine_vel)
		{
			const float cam_fpx = get_input<Float>("cam_fpx");

			// only this case is a true absolute AGL measurement
			const float alt_m = cam_fpx * gps->speed_ms / vel.xy.norm();

			AGL_out = exp_gain<float>(AGL_out, alt_m, gain);
			AGL_source = "GPS_CAM";

			if(baro) {
				baro_ref = baro->get_alt();		// keep baro reference
			}
			if(gps->fix_type >= 2) {
				gps_ref = gps->alt_m;			// keep GPS reference
			}
			base_ref = AGL_out;
			return;
		}

		// barometer fallback (better than GPS fallback)
		if(baro) {
			AGL_out = base_ref + (baro->get_alt() - baro_ref);
			AGL_source = "BARO";
			return;
		}

		// affine fallback
		if(delta.valid()) {
			AGL_out *= delta.scale();
			AGL_source = "CAM";
			return;
		}

		// GPS fallback
		if(gps && gps->fix_type >= 2) {
			AGL_out = base_ref + (gps->alt_m - gps_ref);
			AGL_source = "GPS";
			return;
		}

		// keep last known value as a last resort
		AGL_source = "NONE";
	}

private:
	float base_ref = 0;		// [m]
	float baro_ref = 0;		// [m]
	float gps_ref = 0;		// [m]

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_AGL_STAGE_H_ */
