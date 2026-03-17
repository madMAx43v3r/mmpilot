/*
 * localization.h
 *
 *  Created on: Mar 17, 2026
 *      Author: mad
 */

#ifndef TEST_LOCALIZATION_H_
#define TEST_LOCALIZATION_H_

#include <mmpilot/localization.h>

#include "pipeline2.h"


class LocalizationPipe : public Pipeline {
public:
	Localization loc;

	LocalizationPipe(const std::string& file_name)
	{
		Reader in(file_name);
		loc.map = Map::read(in);

		std::cout << "Localization: Map '" << file_name << "': "
				<< loc.map->size << " x " << loc.map->size << " x " << loc.map->format
				<< ", scale = " << loc.map->scale << " m/px" << std::endl;
	}

protected:
	void init(int width, int height) override
	{
		Pipeline::init(width, height);

		tex_map = loc.map->upload();

		loc.debug = true;

		loc.init(src_width, src_height);

		loc.set_fov_deg(src_width, src_height, FOV_cam);
	}

	void update() override
	{
		loc.advance(get_params());

		loc.exec(source);

		const auto pose = loc.get_pose();

		std::cout << "Localization: A: " << to_string(loc.A) << ", R_norm = " << loc.A.R_norm << ", state = " << loc.state << ", valid = " << loc.valid << std::endl;
		std::cout << "Localization: Pose: (" << pose.pos.x() << ", " << pose.pos.y() << ", " << pose.alt << ") m, " << rad2deg(pose.yaw) << " deg" << std::endl;

		rebase();

//		show(display, source);
//		show(display, loc.affine.stage[0]->solver.tex_debug);

		show(display, tex_map);

		if(loc.valid) {
			TexDisplay::Marker mark;
			mark.x = loc.A.p(0);
			mark.y = loc.A.p(1);
			mark.size = 10;
			mark.color = std::array<float, 4>{1, 0, 0, 0.5};
			display->set_marker("loc", mark);
		} else {
			display->clear_marker("loc");
		}
		{
			TexDisplay::Marker mark;
			mark.x = gps_pos.x();
			mark.y = gps_pos.y();
			mark.size = 10;
			mark.color = std::array<float, 4>{0, 0, 1, 0.5};
			display->set_marker("gps", mark);
		}
	}

	void on_sample(std::shared_ptr<Sample> sample) override
	{
		Pipeline::on_sample(sample);

		if(auto gps = std::dynamic_pointer_cast<MSP2Client::RawGPS>(sample))
		{
			const auto lat = gps->get_lat();
			const auto lon = gps->get_lon();

			loc.init_gps(lat, lon);

			gps_pos = loc.map->gps_to_px(lat, lon);
		}
	}

private:
	std::shared_ptr<GL_Tex2D> tex_map;

	Vec2f gps_pos = Vec2f::Zero();

};



#endif /* TEST_LOCALIZATION_H_ */
