/*
 * virtual_cam_stage.h
 *
 *  Created on: Jun 29, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_VIRTUAL_CAMERA_STAGE_H_
#define INCLUDE_MMPILOT_VIRTUAL_CAMERA_STAGE_H_

#include <mmpilot/stage.h>
#include <mmpilot/pipeline.h>
#include <mmpilot/virtual_cam.h>
#include <mmpilot/weight.h>


namespace mmpilot {

class VirtualCamStage : public Stage {
public:
	Integer width = 1024;			// output size
	Integer height = 1024;			// output size

	float FOV_in = 200;				// fisheye deg (diagonal)
	float FOV_cam = 120;			// virtual deg (diagonal)
	float FOV_circle = 1;			// for FOV_in

	int cam_model = 3;				// (pinhole, equi-distant, equi-solid, stereo-graphic)

	Vec2f K_param = Vec2f::Zero();	// distortion params (K2, K4)

	Vec3f RPY_cam = Vec3f::Zero();	// relative to body frame [deg]


	VirtualCamStage() : Stage("virtual_cam") {}


	VirtualCam virtual_cam;
	WeightRadius weight_radius;

	Mat3f R_BC;			// camera to body
	Mat3f R_WB;			// body to world
	Mat3f R_EB;			// camera to extrinsic

	ConstPointer output;		// GL_Tex2D

	Float cam_fpx = 0;			// focal length [pix]
	Float cam_yaw = 0;			// yaw between virtual camera and body [deg]

	std::shared_ptr<const GL_Tex2D> input;

private:
	void init() override
	{
		input = get_input<ConstPointer>("image").get<GL_Tex2D>();

		cam_fpx = Vec2f(width, height).norm() / (2 * tan(deg2rad(FOV_cam) / 2));

		// shuffle matrix to make hand calibration easier
		// defaults to camera looking down, XY aligned to body frame
		R_EB <<  0,  1,  0,
				-1,  0,  0,
				 0,  0,  1;

//		R_BC = rpy_to_rot_zyx_deg(RPY_cam) * R_EB;	// TODO
		R_BC = rpy_to_rot_zyx_deg(RPY_cam);

		virtual_cam.width = width;
		virtual_cam.height = height;
		virtual_cam.FOV_in = FOV_in;
		virtual_cam.FOV_cam = FOV_cam;
		virtual_cam.FOV_circle = FOV_circle;
		virtual_cam.K2 = K_param.x();
		virtual_cam.K4 = K_param.y();
		virtual_cam.model = cam_model;
		virtual_cam.init(GL_RG16F, GL_RG, GL_HALF_FLOAT);

		weight_radius.init(GL_RG, width, height);

		add_output("width", &width);
		add_output("height", &height);
		add_output("image", &output);
		add_output("cam_fpx", &cam_fpx);
		add_output("cam_yaw", &cam_yaw);
	}

	void exec() override
	{
		const auto& gyro = get_input<Gyro::State>("gyro");

		const Vec3f RPY = gyro.get_rpy();

		cam_yaw = RPY.z() - RPY_cam.z();	// [deg]

		std::cout << "RPY: " << RPY[0] << ", " << RPY[1] << ", " << RPY[2] << std::endl;

//		R_WB = rpy_to_rot_zyx_deg<float>({RPY[1], -RPY[0], RPY[2]});
		R_WB = gyro.matrix();

		virtual_cam.R_mat = R_BC * R_WB.transpose();
		virtual_cam.exec(input);

		weight_radius.exec(virtual_cam.out);

		output = weight_radius.out;
	}

};





} // mmpilot

#endif /* INCLUDE_MMPILOT_VIRTUAL_CAMERA_STAGE_H_ */
