/*
 * mapping.h
 *
 *  Created on: Feb 17, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_MAPPING_H_
#define INCLUDE_MMPILOT_MAPPING_H_

#include <mmpilot/opengl.h>
#include <mmpilot/texture.h>
#include <mmpilot/affine.h>
#include <mmpilot/transform.h>
#include <mmpilot/merge.h>
#include <mmpilot/gps.h>
#include <mmpilot/pose_graph_geo_img.h>

#include <vector>
#include <optional>
#include <iostream>


namespace mmpilot {

class Mapping {
public:
	using WGS84 = mmpilot::WGS84<double>;
	using PoseGraph = PoseGraphGeoImg<double>;

	double gps_sigma = 5;			// GPS position stddev [m]
	double dxy_sigma = 0.2;			// image delta stddev [m]
	double dyaw_sigma = 0.05;		// image rotation stddev [rad]
	double dscale_sigma = 0.1;		// image scale stddev [log(m/px)]

	std::optional<double> gps_alt_override;

	struct Node {
		int64_t ts = 0;			// [us]
		float weight = 1;
		Affine::Params A;
		std::shared_ptr<GL_Tex2D> image;
		std::shared_ptr<PoseGraph::Node> node;

		Transform2D pose(const WGS84& origin) const;
	};

	struct Buffer {
		std::shared_ptr<GL_Tex2D> map;
		std::shared_ptr<GL_Tex2D> weight;

		GLuint fbo = 0;
		GLuint rbo = 0;

		Buffer(int width, int height, bool is_mono);
		~Buffer();
		void clear();
	};

	MergeFilter merge;

	std::shared_ptr<GL_Tex2D> tex_tmp;
	std::shared_ptr<GL_Tex2D> tex_debug;

	void init(int width, int height, GLenum format);

	void update(const int64_t ts, std::shared_ptr<GL_Tex2D> img, const Affine::Params& A);

	void render(std::shared_ptr<GL_Tex2D> img, const Affine::Params& A);

	void on_gps(std::shared_ptr<MSP2Client::RawGPS> gps);

	std::shared_ptr<GL_Tex2D> finalize();

private:
	void render_image(
			std::shared_ptr<Buffer> buf,
			std::shared_ptr<GL_Tex2D> img,
			const std::vector<Vec2f>& coords);

	void compress(GLuint fbo, std::shared_ptr<Buffer> buf);

	void optimize(std::shared_ptr<Node> L, std::shared_ptr<Node> R);

	void set_gps(std::shared_ptr<PoseGraph::Node> n, std::shared_ptr<const GPS::State> gps);

private:
	int width = 0;
	int height = 0;

	bool have_init = false;
	bool is_mono = false;

	GPS gps_api;
	PoseGraph graph;

	std::vector<std::shared_ptr<Node>> nodes;

	std::list<std::shared_ptr<Node>> waiting_gps;

	GLuint prog_render = 0;
	GLuint prog_compress = 0;

	GLuint vao = 0;
	GLuint vbo = 0;
	GLuint ebo = 0;

	GLuint fbo_debug = 0;

};





} // mmpilot

#endif /* INCLUDE_MMPILOT_MAPPING_H_ */
