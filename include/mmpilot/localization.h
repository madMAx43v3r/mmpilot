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


namespace mmpilot {

class Localization {
public:
	int num_search = 10;				// search samples per update
	int lock_delay = 10;				// number of frames
	float init_sigma_xy = 100;			// [px]
	float init_sigma_scale = 0.1;		// log(scale)
	float unique_threshold = 1.5;
	float max_error = 200;				// R_norm

	float f_camera = 0;					// camera focal length [px]

	std::shared_ptr<Map> map;				// [R]
	std::shared_ptr<GL_Tex2D> map_tex;		// [R, w]

	MultiAffine affine;

	Affine::Params A;

	int lock_count = 0;
	bool is_locked = false;				// if localization has locked on (initialized)


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

	void init_gps(double lat_deg, double lon_deg)
	{
		if(is_locked) {
			return;
		}
		if(!origin) {
			origin = std::make_shared<WGS84<double>>(map->lat0, map->lon0, map->alt0);
		}
		const auto EN = origin->get_en(deg2rad(lat_deg), deg2rad(lon_deg));
		A.p(0) = EN.x() / map->scale;
		A.p(1) = EN.y() / map->scale;
	}

	void init_yaw(float yaw_rad)
	{
		if(is_locked) {
			return;
		}
		A.p(2) = yaw_rad;
	}

	void init_alt(float alt_m)
	{
		if(is_locked) {
			return;
		}
		A.p(3) = alt_m / (map->scale * f_camera);
	}

	void init(int width, int height)
	{
		if(have_init) {
			throw std::logic_error("already initialized");
		}
		if(!map) {
			throw std::logic_error("have no map");
		}
		if(!map->format == 1) {
			throw std::logic_error("invalid map format");
		}
		affine.init(width, height, map->width, map->height);

		map_tex = map->upload();

		affine.exec_ref(map_tex);

		have_init = true;
	}

	void exec(std::shared_ptr<GL_Tex2D> img)
	{
		if(!have_init) {
			init(img->width, img->height);
		}

		if(is_locked) {
			const auto A_new = affine.exec_img(img, A);
			if(A_new.converged) {
				A = A_new;
				std::cout << "Localization: R = " << A.R_norm << ", overlap = " << A.overlap << std::endl;
			} else {
				is_locked = false;
				std::cout << "Localization: lost lock!" << std::endl;
			}
		}

		if(!is_locked) {
			std::vector<Affine::Params> A_init;
			for(int i = 0; i < num_search; ++i) {
				auto A_i = A;
				A_i.p(0) += rand_gaussian<double>(0, init_sigma_xy);
				A_i.p(1) += rand_gaussian<double>(0, init_sigma_xy);
				A_i.p(2)  = rand_range<double>(0, 2 * M_PI);
				A_i.p(3)  = std::exp(std::log(A_i.scale()) + rand_gaussian<double>(0, init_sigma_scale));
				A_init.push_back(A_i);
			}
			std::vector<Affine::Params> A_list;

			for(const auto& A_i : A_init) {
				const auto A_new = affine.exec_img(img, A_i);
				if(A_new.converged && A_new.R_norm < max_error) {
					A_list.push_back(A_new);
				}
			}
			std::sort(A_list.begin(), A_list.end(),
				[](const Affine::Params& L, const Affine::Params& R) {
					return L.R_norm < R.R_norm;
				});

			if(A_list.size() >= 1) {
				A = A_list[0];
				std::cout << "Localization: Search: R = " << A.R_norm << ", N = " << A_list.size() << std::endl;

				if(A_list.size() == 1) {
					lock_count++;
					std::cout << "Localization: Search: lock " << lock_count << " with single result" << std::endl;
				} else{
					const auto ratio = A_list[1].R_norm / A.R_norm;
					if(ratio > unique_threshold) {
						lock_count++;
						std::cout << "Localization: Search: lock " << lock_count << " with ratio " << ratio << std::endl;
					} else {
						lock_count = 0;
						std::cout << "Localization: Search: lock reset due to ratio " << ratio << std::endl;
					}
				}
			}
			else {
				lock_count = 0;
			}

			if(lock_count > lock_delay) {
				is_locked = true;
				std::cout << "Localization: acquired lock" << std::endl;
			}
		}
	}


private:
	bool have_init = false;

	std::shared_ptr<WGS84<double>> origin;

};



} // mmpilot

#endif /* INCLUDE_MMPILOT_LOCALIZATION_H_ */
