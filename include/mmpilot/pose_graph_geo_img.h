/*
 * pose_graph_geo_img.h
 *
 *  Created on: Feb 26, 2026
 *      Author: mad
 *
 * Pose graph with:
 *   - Optimized per-node state: lat, lon, yaw, log_s   (4 DoF)
 *   - Measured per-node altitude alt_m (meters), NOT optimized
 *   - Absolute GPS position priors per node (unary factors) in EN meters
 *   - Image relative constraints between nodes:
 *       dp_EN(lat/lon)  ≈  R(yaw_i) * (s_i * dpx_ij)
 *     and scale-ratio constraint:
 *       (log_s_j - log_s_i) ≈ log(dscale_ij)
 *
 * Notes:
 *   - No GPS delta edges. Absolute GPS priors anchor the graph => no need to fix node0.
 *   - EN conversion uses WGS84 radii of curvature evaluated at midpoint latitude, and avg altitude for edges.
 *   - For GPS unary priors, EN scale is evaluated at gps_lat and node altitude alt_m.
 *   - dscale is assumed isotropic (same for x/y).
 */

#ifndef INCLUDE_MMPILOT_POSE_GRAPH_GEO_IMG_H_
#define INCLUDE_MMPILOT_POSE_GRAPH_GEO_IMG_H_

#include <Eigen/Dense>
#include <Eigen/Sparse>

#include <vector>
#include <cmath>
#include <stdexcept>
#include <algorithm>


namespace mmpilot {

template<typename T>
class PoseGraphGeoImg {
public:
	static constexpr int D = 4;    // [lat, lon, yaw, log_s]

	using Vec2 = Eigen::Matrix<T, 2, 1>;
	using VecD = Eigen::Matrix<T, D, 1>;
	using Mat2 = Eigen::Matrix<T, 2, 2>;
	using MatD = Eigen::Matrix<T, D, D>;
	using Mat2D = Eigen::Matrix<T, 2, D>;
	using Vector = Eigen::Matrix<T, Eigen::Dynamic, 1>;

	struct Node {
		// Optimized:
		T lat = T(0);   // latitude [rad]
		T lon = T(0);   // longitude [rad]
		T yaw = T(0);   // image to GPS heading delta [rad]
		T ls  = T(0);   // scale: log(m/px)

		// GPS measurement (optional):
		bool gps_valid = false;

		T gps_lat = T(0);     // latitude [rad]
		T gps_lon = T(0);     // longitude [rad]
		T gps_alt = T(0);     // altitude [m]

		// info on EN-meter residual [dE; dN]
		Mat2 gps_info = Mat2::Zero();

		int64_t ts = 0;		  // timestamp [usec]

		// Attach/update absolute GPS measurement prior for node k.
		// info_EN weights residual in meters in EN frame:
		//   r = [dE; dN] = [kE*(lon-lon_gps), kN*(lat-lat_gps)]
		void set_gps(int k, T lat_rad, T lon_rad, T alt_m, const Mat2& info_EN)
		{
			gps_valid = true;
			gps_lat = lat_rad;
			gps_lon = lon_rad;
			gps_alt = alt_m;
			gps_info = info_EN;
			lat = lat_rad;
			lon = lon_rad;
		}
	};

	struct Edge {
		int i = -1;
		int j = -1;

		// Image measurement i->j:
		Vec2 dpx = Vec2::Zero();    // image delta (dx, dy) in pixel

		T dyaw = T(0);              // relative rotation measurement: yaw_j - yaw_i (rad), wrapped
		T dscale = T(1);            // scale delta, interpreted as s_j / s_i

		// info for EN position residual (meters)
		Mat2 info_pos = Mat2::Identity();

		T info_yaw = T(1);          // info (inverse variance) for yaw residual (rad^-2)
		T info_scl = T(1);			// info (inverse variance) for scalar scale residual in log-space
	};

	struct Result {
		double error = 0;
		int num_iters = 0;
		bool converged = false;
	};

	int node_count() const {
		return int(nodes_.size());
	}

	Node& node(const int k) {
		if(k < 0 || k >= node_count()) {
			throw std::runtime_error("node index out of range");
		}
		return nodes_[k];
	}

	std::vector<Node>& nodes() {
		return nodes_;
	}
	const std::vector<Node>& nodes() const {
		return nodes_;
	}
	const std::vector<Edge>& edges() const {
		return edges_;
	}

	// Add node with initial guess
	int add_node(T lat_rad = T(0), T lon_rad = T(0), T yaw_rad = T(0), T m_per_px = T(1))
	{
		Node n;
		n.lat = lat_rad;
		n.lon = lon_rad;

		n.yaw = yaw_rad;
		n.ls = std::log(std::max(m_per_px, T(1e-9)));

		nodes_.push_back(n);
		return int(nodes_.size()) - 1;
	}

	// Add image edge i->j:
	//   - dpx in pixels
	//   - dscale = s_j / s_i (>0) from matching (isotropic)
	//   - info_pos: 2x2 info for EN position residual
	//   - info_yaw: scalar info (inverse variance) for yaw residual
	//   - info_scl: scalar info (inverse variance) for log-scale residual
	void add_edge(
			int i, int j, const Vec2& dpx, T dyaw, T dscale,
			const Mat2& info_pos, T info_yaw, T info_scl)
	{
		check_ij(i, j);

		if(dscale <= 0) {
			throw std::runtime_error("add_edge(): dscale must be > 0");
		}
		if(info_scl <= 0) {
			throw std::runtime_error("add_edge(): info_scl must be > 0");
		}
		Edge e;
		e.i = i;
		e.j = j;
		e.dpx = dpx;
		e.dyaw = dyaw;
		e.dscale = dscale;
		e.info_pos = info_pos;
		e.info_yaw = info_yaw;
		e.info_scl = info_scl;
		edges_.push_back(e);
	}

	Result solve(int max_iters = 50, T step_tol = T(1e-6), T lambda = T(1e-9))
	{
		Result out;
		const int N = node_count();
		if(N < 1) {
			return out;
		}

		// Must have at least one absolute anchor (GPS prior) or the system will be singular.
		bool any_gps = false;
		for(const auto& n : nodes_) {
			if(n.gps_valid) {
				any_gps = true;
				break;
			}
		}
		if(!any_gps && edges_.empty()) {
			return out;
		}
		const int M = D * N;	// number of unknowns

		std::vector<Eigen::Triplet<T>> Ht;		// Hessian entries

		for(int it = 0; it < max_iters; ++it)
		{
			Ht.clear();

			double err = 0;
			Vector g = Vector::Zero(M);

			auto add_block = [&](const int r, const int c, const MatD& B) {
				for(int i = 0; i < D; ++i) {
					for(int k = 0; k < D; ++k) {
						const T v = B(i, k);
						if(v != T(0)) {
							Ht.emplace_back(r + i, c + k, v);
						}
					}
				}
			};
			auto add_g = [&](const int r, const VecD& v) {
				for(int k = 0; k < D; ++k) {
					g[r + k] += v[k];
				}
			};

			// ---- Unary absolute GPS priors ----
			for(int k = 0; k < N; ++k)
			{
				const Node& nk = nodes_[k];
				if(!nk.gps_valid) {
					continue;
				}
				const int bk = block_index(k);

				// Evaluate EN scales at gps_lat (stable), using node altitude
				T kE, kN;
				en_scales_wgs84(nk.gps_lat, nk.gps_alt, kE, kN);

				const Vec2 r(
						kE * (nk.lon - nk.gps_lon), // dE
						kN * (nk.lat - nk.gps_lat)  // dN
				);
				err += double(r.transpose() * (nk.gps_info * r));

				Mat2D J = Mat2D::Zero();
				J(0, 1) = kE; // d(dE)/d(lon)
				J(1, 0) = kN; // d(dN)/d(lat)

				accumulate_unary_2d(add_block, add_g, bk, J, r, nk.gps_info);
			}

			// ---- Binary image edges ----
			for(const auto& e : edges_)
			{
				const Node& ni = nodes_[e.i];
				const Node& nj = nodes_[e.j];

				const int bi = block_index(e.i);
				const int bj = block_index(e.j);

				// Per-edge EN scaling at midpoint latitude, avg altitude
				const T lat_mid = (ni.lat + nj.lat) / 2;
				const T alt_mid = (ni.gps_alt + nj.gps_alt) / 2;

				T kE, kN;
				en_scales_wgs84(lat_mid, alt_mid, kE, kN);

				// Predicted EN delta from lat/lon
				const Vec2 dp_EN(kE * (nj.lon - ni.lon), kN * (nj.lat - ni.lat));

				// (1) Position residual: r = dp_EN - R(yaw_i)*(s_i*dpx)
				{
					const T s_i = std::exp(ni.ls);	// scale factor

					const Vec2 u = s_i * e.dpx;		// delta meters in image frame

					const T c = std::cos(ni.yaw);
					const T s = std::sin(ni.yaw);

					// delta in EN-meters
					const Vec2 en_img(c * u[0] - s * u[1], s * u[0] + c * u[1]);

					const Vec2 r = dp_EN - en_img;	// residual in EN-meters

					err += r.transpose() * (e.info_pos * r);

					Mat2D Ji = Mat2D::Zero();
					Mat2D Jj = Mat2D::Zero();

					// dp_EN wrt lat/lon
					Ji(0, 1) += -kE;
					Ji(1, 0) += -kN;
					Jj(0, 1) += +kE;
					Jj(1, 0) += +kN;

					// -en_img wrt yaw and log_s (node i only)
					// d(en)/d(yaw) = dR/dyaw * u, dR/dyaw = [ -s -c; c -s ]
					const Vec2 den_dyaw((-s) * u[0] + (-c) * u[1], (c) * u[0] + (-s) * u[1]);

					// d(en)/d(log_s) = d(en)/d(s) * ds/dlog_s = R * dpx * s = en_img
					// (because en_img = R*(s*dpx))
					const Vec2 den_dls = en_img;

					Ji(0, 2) += -den_dyaw[0];
					Ji(1, 2) += -den_dyaw[1];
					Ji(0, 3) += -den_dls[0];
					Ji(1, 3) += -den_dls[1];

					accumulate_binary_2d(add_block, add_g, bi, bj, Ji, Jj, r, e.info_pos);
				}

				// (2) Rotation residual (scalar): r = wrap_pi((yaw_j - yaw_i) - dyaw)
				{
					const T r = angle_wrap_pi((nj.yaw - ni.yaw) - e.dyaw);

					err += double(r * (double(e.info_yaw) * r));

					VecD gi = VecD::Zero();
					VecD gj = VecD::Zero();
					MatD Hii = MatD::Zero();
					MatD Hjj = MatD::Zero();
					MatD Hij = MatD::Zero();

					const T W = e.info_yaw;

					// Jr/dyaw_i = -1, Jr/dyaw_j = +1
					Hii(2, 2) += W;
					Hjj(2, 2) += W;
					Hij(2, 2) += -W;

					gi[2] += (-1) * W * r;
					gj[2] += (+1) * W * r;

					add_block(bi, bi, Hii);
					add_block(bj, bj, Hjj);
					add_block(bi, bj, Hij);
					add_block(bj, bi, Hij.transpose());

					add_g(bi, gi);
					add_g(bj, gj);
				}

				// (3) Scale residual (scalar): r = (ls_j - ls_i) - log(dscale)
				{
					const T lds = std::log(std::max(e.dscale, T(1e-9)));
					const T r = (nj.ls - ni.ls) - lds;

					err += r * double(e.info_scl * r);

					VecD gi = VecD::Zero();
					VecD gj = VecD::Zero();
					MatD Hii = MatD::Zero();
					MatD Hjj = MatD::Zero();
					MatD Hij = MatD::Zero();

					// Jr wrt node i: d r / d ls_i = -1
					// Jr wrt node j: d r / d ls_j = +1
					const T W = e.info_scl;

					// Hii += (-1)^2 * W
					Hii(3, 3) += W;
					// Hjj += (+1)^2 * W
					Hjj(3, 3) += W;
					// Hij += (-1)*(+1)*W = -W
					Hij(3, 3) += -W;

					// g += J^T W r
					gi[3] += (-1) * W * r;
					gj[3] += (+1) * W * r;

					add_block(bi, bi, Hii);
					add_block(bj, bj, Hjj);
					add_block(bi, bj, Hij);
					add_block(bj, bi, Hij.transpose());

					add_g(bi, gi);
					add_g(bj, gj);
				}
			}

			Eigen::SparseMatrix<T> H(M, M);
			H.setFromTriplets(Ht.begin(), Ht.end());

			if(lambda > 0) {
				for(int k = 0; k < M; ++k) {
					H.coeffRef(k, k) += lambda;
				}
			}

			Eigen::SimplicialLDLT<Eigen::SparseMatrix<T>> solver;
			solver.compute(H);
			if(solver.info() != Eigen::Success) {
				throw std::runtime_error("solve(): LDLT factorization failed");
			}

			const Vector dx = solver.solve(-g);
			if(solver.info() != Eigen::Success) {
				throw std::runtime_error("solve(): solve failed");
			}

			// Apply update to all nodes
			T max_step = 0;
			for(int k = 0; k < N; ++k)
			{
				Node& n = nodes_[k];

				const T dlat = dx[D * k + 0];
				const T dlon = dx[D * k + 1];
				const T dyaw = dx[D * k + 2];
				const T dls  = dx[D * k + 3];

				n.lat += dlat;
				n.lon += dlon;
				n.yaw += dyaw;
				n.ls  += dls;

				max_step = std::max(max_step, std::abs(dlat));
				max_step = std::max(max_step, std::abs(dlon));
				max_step = std::max(max_step, std::abs(dyaw));
				max_step = std::max(max_step, std::abs(dls));
			}

			out.error = err;
			out.num_iters = it + 1;

			if(max_step < step_tol) {
				out.converged = true;
				break;
			}
		}
		return out;
	}

private:
	std::vector<Node> nodes_;
	std::vector<Edge> edges_;

	static int block_index(int node_id) {
		return D * node_id;
	}

	void check_ij(int i, int j) const
	{
		if(i < 0 || j < 0 || i >= node_count() || j >= node_count()) {
			throw std::runtime_error("edge: node index out of range");
		}
		if(i == j) {
			throw std::runtime_error("edge: i == j");
		}
	}

	// WGS84 scales: meters per radian for lon (East) and lat (North)
	// kE = (N + h) * cos(lat)
	// kN = (M + h)
	static void en_scales_wgs84(const T lat_rad, const T alt_m, T& kE, T& kN)
	{
		const T a = T(6378137.0);
		const T f = T(1.0 / 298.257223563);
		const T e2 = f * (T(2) - f);

		const T s = std::sin(lat_rad);
		const T c = std::cos(lat_rad);

		const T denom = std::sqrt(T(1) - e2 * s * s);

		const T N = a / denom;                                   // prime vertical
		const T M = a * (T(1) - e2) / (denom * denom * denom);   // meridional

		kE = (N + alt_m) * c;
		kN = (M + alt_m);
	}

	static T angle_wrap_pi(T a)
	{
		const T two_pi = T(2) * T(M_PI);
		a = std::fmod(a + T(M_PI), two_pi);
		if(a < T(0)) {
			a += two_pi;
		}
		return a - T(M_PI);
	}

	template<typename AddBlockFn, typename AddGFn>
	static void accumulate_unary_2d(
			AddBlockFn&& add_block, AddGFn&& add_g,
			int bk, const Mat2D& J, const Vec2& r, const Mat2& W)
	{
		const auto JT = J.transpose();

		const Vec2 Wr = W * r;

		const MatD Hkk = JT * W * J;
		const VecD gk  = JT * Wr;

		add_block(bk, bk, Hkk);
		add_g(bk, gk);
	}

	template<typename AddBlockFn, typename AddGFn>
	static void accumulate_binary_2d(
			AddBlockFn&& add_block, AddGFn&& add_g,
			int bi, int bj, const Mat2D& Ji, const Mat2D& Jj, const Vec2& r, const Mat2& W)
	{
		const auto JiT = Ji.transpose();
		const auto JjT = Jj.transpose();

		const Vec2 Wr = W * r;

		const MatD Hii = JiT * W * Ji;
		const MatD Hjj = JjT * W * Jj;
		const MatD Hij = JiT * W * Jj;

		const VecD gi = JiT * Wr;
		const VecD gj = JjT * Wr;

		add_block(bi, bi, Hii);
		add_block(bj, bj, Hjj);
		add_block(bi, bj, Hij);
		add_block(bj, bi, Hij.transpose());

		add_g(bi, gi);
		add_g(bj, gj);
	}

};


} // namespace mmpilot

#endif /* INCLUDE_MMPILOT_POSE_GRAPH_GEO_IMG_H_ */
