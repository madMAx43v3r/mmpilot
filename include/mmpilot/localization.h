/*
 * localization.h
 *
 *  Created on: Mar 7, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_LOCALIZATION_H_
#define INCLUDE_MMPILOT_LOCALIZATION_H_

#include <mmpilot/multi_affine.h>
#include <mmpilot/transform.h>
#include <mmpilot/map.h>
#include <mmpilot/pose.h>
#include <mmpilot/wgs84.h>
#include <mmpilot/random.h>

#include <optional>


namespace mmpilot {

class Localization {
public:
	int num_search = 8;					// search samples per update
	int lock_delay = 20;				// number of frames

	float init_sigma_xy = 10;			// [m]
	float init_sigma_yaw = 5;			// [deg]
	float init_sigma_scale = 0.25;		// log(scale)

	float lock_sigma_pos = 0.02;		// sigma_xy / AGL
	float lock_sigma_yaw = 1;			// [deg]
	float lock_sigma_scale = 0.01;

	float max_error = 300;				// R_norm
	float min_overlap = 0.2;

	float f_camera = 0;					// camera focal length [px]

	std::shared_ptr<Map> map;				// [R]
	std::shared_ptr<GL_Tex2D> tex_map;		// [R, w]

	MultiAffine affine;

	Affine::Params A;
	Affine::Params A_init;

	float sigma_xy = 5;					// [m]
	float sigma_yaw = deg2rad(5.f);		// [rad]
	float sigma_scale = 0.01;			// [log(scale)]

	int state = 0;						// 0 = no init, 1 = search, 2 = locked
	int lock_count = 0;

	bool debug = false;
	bool valid = false;


	PoseEN get_pose() const
	{
		PoseEN out;
		out.pos = A.translation() * map->scale;
		out.yaw = A.yaw();
		out.alt = A.scale() * map->scale * f_camera;
		return out;
	}

	PoseGPS get_gps() const
	{
		const auto pose = get_pose();

		PoseGPS out;
		// TODO
		out.yaw = pose.yaw;
		out.alt = pose.alt;
		return out;
	}

	void set_fov_deg(int width, int height, float fov_diag)
	{
		// rectilinear baseline
		const auto diag = Vec2f(width, height).norm() / 2;
		f_camera = diag / std::tan(deg2rad(fov_diag) / 2);
	}

	void init_gps(double lat_deg, double lon_deg)
	{
		const auto pos = map->gps_to_px(lat_deg, lon_deg);
		A_init.p(0) = pos.x();
		A_init.p(1) = pos.y();

		if(state <= 0) {
			state = 1;
		}
		if(state == 1) {
			std::cout << "Localization: Initialized with GPS: lat = " << lat_deg << ", lon = " << lon_deg
						<< ", pos = (" << pos.x() << ", " << pos.y() << ") px" << std::endl;
		}
	}

	void init_yaw(float yaw_deg)
	{
		A_init.p(2) = deg2rad(yaw_deg);
	}

	void init_alt(float alt_m)
	{
		A_init.p(3) = alt_m / (map->scale * f_camera);
	}

	void init(int width, int height)
	{
		if(have_init) {
			throw std::logic_error("already initialized");
		}
		if(width != height) {
			throw std::logic_error("width != height");
		}
		if(!map) {
			throw std::logic_error("have no map");
		}
		if(!map->format == 1) {
			throw std::logic_error("invalid map format");
		}
		affine.debug = debug;
		affine.init(width, height, map->size, map->size);

		tex_map = map->upload_ex();

		affine.exec_ref(tex_map);

		have_init = true;
	}

	void advance(const Affine::Params& odom)
	{
		A.add(odom);
	}

	void exec(std::shared_ptr<GL_Tex2D> img)
	{
		if(!have_init) {
			init(img->width, img->height);
		}
		if(state < 1) {
			return;		// need init first
		}

		std::optional<Affine::Params> A_0;
		if(valid) {
			A_0 = A;
		}

		if(state >= 2) {
			const auto A_new = affine.exec_img(img, A);
			if(A_new.valid()) {
				A = A_new;
				A_init.p(2) = A_new.p(2);	// save yaw
				A_init.p(3) = A_new.p(3);	// save scale
				have_yaw = true;
				valid = true;
				std::cout << "Localization: R = " << A.R_norm << ", overlap = " << A.overlap << std::endl;
			} else {
				state = 1;
				valid = false;
				lock_count = 0;
				std::cout << "Localization: lost lock!" << std::endl;
			}
		}

		if(state == 1) {
			std::vector<Affine::Params> A_try;
			for(int i = 0; i < num_search; ++i) {
				auto A_i = A_init;
				A_i.p(0) += rand_gaussian<double>(0, init_sigma_xy) / map->scale;
				A_i.p(1) += rand_gaussian<double>(0, init_sigma_xy) / map->scale;
				if(have_yaw) {
					A_i.p(2) += rand_gaussian<double>(0, deg2rad(init_sigma_yaw));
				} else {
					A_i.p(2)  = rand_range<double>(0, 2 * M_PI);
				}
				A_i.p(3)  = std::exp(rand_gaussian<double>(std::log(A_i.scale()), init_sigma_scale));
				A_try.push_back(A_i);
			}
			A_try.push_back(A_init);

			if(valid) {
				A_try.push_back(A);
			}
			std::vector<Affine::Params> A_res;

			for(const auto& A_i : A_try)
			{
				const auto A_new = affine.exec_img(img, A_i);

				if(A_new.valid() && A_new.R_norm < max_error && A_new.overlap > min_overlap) {
					A_res.push_back(A_new);
				}
			}
			std::sort(A_res.begin(), A_res.end(),
				[](const Affine::Params& L, const Affine::Params& R) {
					return L.R_norm < R.R_norm;
				});

			if(A_res.size() > 0) {
				A = A_res[0];
				valid = true;
				std::cout << "Localization: Search: R = " << A.R_norm << ", N = " << A_res.size() << std::endl;
			}
			else {
				valid = false;
				lock_count = 0;
				std::cout << "Localization: no solution found" << std::endl;
			}
		}

		if(valid && A_0) {
			const float dpos = (A.translation() - A_0->translation()).norm();
			const float dyaw = angle_norm_pi(A.yaw() - A_0->yaw());
			const float dscale = std::log(A.scale()) - std::log(A_0->scale());

			const float gain = 0.1;
			sigma_xy = std::sqrt(std::pow(sigma_xy, 2.f) * (1 - gain) + std::pow(dpos, 2.f) * gain);
			sigma_yaw = std::sqrt(std::pow(sigma_yaw, 2.f) * (1 - gain) + std::pow(dyaw, 2.f) * gain);
			sigma_scale = std::sqrt(std::pow(sigma_scale, 2.f) * (1 - gain) + std::pow(dscale, 2.f) * gain);

			if(state == 1) {
				const auto sigma_pos = sigma_xy / get_pose().alt;
				if(sigma_pos < lock_sigma_pos && sigma_yaw < deg2rad(lock_sigma_yaw) && sigma_scale < lock_sigma_scale) {
					lock_count++;
				}
			}
			std::cout << "Localization: Health: sigma_xy = " << sigma_xy
					<< " m, sigma_yaw = " << rad2deg(sigma_yaw) << " deg, sigma_scale = " << sigma_scale << std::endl;
		} else {
			lock_count = 0;
		}

		if(state == 1 && lock_count > lock_delay) {
			state = 2;
			std::cout << "Localization: acquired lock" << std::endl;
		}
	}


private:
	bool have_init = false;
	bool have_yaw = false;

};



} // mmpilot

#endif /* INCLUDE_MMPILOT_LOCALIZATION_H_ */
