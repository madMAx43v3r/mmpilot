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
#include <mmpilot/multi_affine.h>
#include <mmpilot/transform.h>
#include <mmpilot/merge.h>
#include <mmpilot/stitch.h>
#include <mmpilot/gps.h>
#include <mmpilot/map.h>
#include <mmpilot/pose_graph_geo_img.h>

#include <set>
#include <vector>
#include <optional>
#include <iostream>


namespace mmpilot {

class Mapping {
public:
	using WGS84 = mmpilot::WGS84<double>;
	using PoseGraph = PoseGraphGeoImg<double>;

	float node_delta = 50;				// min edge length [px]

	double max_map_scale = 20;			// [px/m]

	double max_loop_delta = 500;		// maximum initial image shift [px]
	double max_loop_dyaw = 0.2;			// max yaw difference [rad]
	double max_loop_dscale = 0.2;		// max log(scale) difference
	double outlier_threshold = 1;		// multiples of average

	double max_merge_delta = 500;		// [px]
	double max_merge_error = 10;		// average square pixel error

	double gps_sigma = 3;				// GPS position [m]
	double dxy_sigma = 0.2;				// image delta [m]
	double dyaw_sigma = 0.002;			// image rotation [rad]
	double dscale_sigma = 0.02;			// image scale [log(m/px)]

	int64_t gps_delay = 500;			// [ms]

	std::optional<double> gps_alt_override;

	struct Node {
		int64_t ts = 0;			// [us]
		int weight = 1;
		double distance = 0;		// from start [px]
		Transform2D delta;
		std::shared_ptr<GL_Tex2D> out;
		std::shared_ptr<GL_Tex2D> image;
		std::shared_ptr<PoseGraph::Node> node;

		Transform2D pose(const WGS84& origin) const;
	};

	struct Buffer {
		std::shared_ptr<GL_Tex2D> map;
		std::shared_ptr<GL_Tex2D> weight;

		float min_scale = 0;		// [px/m]
		float max_scale = 0;		// [px/m]

		GLuint fbo = 0;
		GLuint rbo = 0;

		Buffer(int width, int height, bool is_mono);
		~Buffer();
		void clear();
	};

	MergeFilter merge;
	MultiAffine affine;
	StitchFilter stitch;

	Transform2D delta;

	std::vector<std::shared_ptr<Node>> nodes;

	std::shared_ptr<Map> map;				// output

	void init(int width, int height, GLenum format);

	void exec(const int64_t ts, std::shared_ptr<GL_Tex2D> img, const Affine::Params& A);

	void on_gps(std::shared_ptr<MSP2::RawGPS> gps);

	void finalize(const int num_pass);

	std::shared_ptr<GL_Tex2D> render_map();

private:
	void render(
			std::shared_ptr<Node> node,
			std::shared_ptr<Buffer> buf,
			const std::vector<Vec2f>& coords);

	void compress(GLuint fbo, std::shared_ptr<Buffer> buf);

	void optimize(std::shared_ptr<Node> L, std::shared_ptr<Node> R, const bool do_merge);

	void set_gps(std::shared_ptr<Node> node, std::shared_ptr<const GPS::State> gps);

	static Affine::Params get_affine(std::shared_ptr<Node> L, std::shared_ptr<Node> R);

private:
	int width = 0;
	int height = 0;
	int merge_count = 0;

	bool have_init = false;
	bool is_mono = false;

	GPS gps_api;
	PoseGraph graph;

	std::list<std::shared_ptr<Node>> waiting_gps;

	GLuint prog_render = 0;
	GLuint prog_compress = 0;

	GLuint vao = 0;
	GLuint vbo = 0;
	GLuint ebo = 0;

};





} // mmpilot

#endif /* INCLUDE_MMPILOT_MAPPING_H_ */
