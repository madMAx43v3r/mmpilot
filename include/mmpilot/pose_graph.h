/*
 * pose_graph.h
 *
 *  Created on: Feb 18, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_POSE_GRAPH_H_
#define INCLUDE_MMPILOT_POSE_GRAPH_H_

#include <mmpilot/math.h>

#include <Eigen/Dense>
#include <Eigen/Sparse>

#include <vector>
#include <cmath>
#include <stdexcept>


namespace mmpilot {

template<typename T>
class PoseGraphXY {
public:
	using Vec2   = Eigen::Matrix<T, 2, 1>;
	using Mat2   = Eigen::Matrix<T, 2, 2>;
	using Vector = Eigen::Matrix<T, Eigen::Dynamic, 1>;

	struct Edge {
		int i = -1;
		int j = -1;
		Vec2 delta = Vec2::Zero();       // measurement (node_j - node_i)
		Mat2 info = Mat2::Identity();    // 2x2 information (inverse covariance)
	};

	struct Result {
		double error = 0;
		int num_iters = 0;
		bool converged = false;
	};

	int add_node(const Vec2& pose = {})
	{
		nodes_.push_back(pose);
		return int(nodes_.size()) - 1;
	}

	void add_edge(	const int i, const int j, const Vec2& delta,
					const Mat2& info = Mat2::Identity())
	{
		if(i < 0 || j < 0 || i >= node_count() || j >= node_count()) {
			throw std::runtime_error("add_edge(): node index out of range");
		}
		edges_.push_back({i, j, delta, info});
	}

	int node_count() const {
		return nodes_.size();
	}

	std::vector<Vec2>& nodes() {
		return nodes_;
	}

	const std::vector<Vec2>& nodes() const {
		return nodes_;
	}

	const std::vector<Edge>& edges() const {
		return edges_;
	}

	double solve()
	{
		const int N = node_count();
		const int M = 2 * (N - 1);     // total unknowns

		if(N < 2) {
			return 0;
		}
		if(edges_.empty()) {
			return 0;
		}
		std::vector<Eigen::Triplet<T>> entries;
		entries.reserve(edges_.size() * 16);

		double error = 0;
		Vector G = Vector::Zero(M);		// gradient

		const auto add_block = [&](const int r, const int c, const Mat2& block) {
			for(int i = 0; i < 2; ++i)
				for(int k = 0; k < 2; ++k)
					if(block(i, k) != 0)
						entries.emplace_back(r + i, c + k, block(i, k));
		};

		for(const auto& edge : edges_)
		{
			const auto& pos_i = nodes_[edge.i];
			const auto& pos_j = nodes_[edge.j];

			// residual
			const Vec2 R_ij = (pos_j - pos_i) - edge.delta;

			// weighted residual
			const Vec2 R_w = edge.info * R_ij;

			error += R_ij.transpose() * R_w;

			const Mat2& W = edge.info;

			const int bi = block_index(edge.i); // -1 if node0
			const int bj = block_index(edge.j);

			if(bi >= 0) {
				add_block(bi, bi, W);
				G.template segment<2>(bi) +=
						(-Mat2::Identity()).transpose() * R_w; // = -R_w
			}
			if(bj >= 0) {
				add_block(bj, bj, W);
				G.template segment<2>(bj) +=
						(Mat2::Identity()).transpose() * R_w; // = +R_w
			}
			if(bi >= 0 && bj >= 0) {
				add_block(bi, bj, -W);
				add_block(bj, bi, -W);
			}
		}

		Eigen::SparseMatrix<T> H(M, M);
		H.setFromTriplets(entries.begin(), entries.end());

		Eigen::SimplicialLDLT<Eigen::SparseMatrix<T>> solver;

		solver.compute(H);

		if(solver.info() != Eigen::Success) {
			throw std::runtime_error("solve(): LDLT factorization failed");
		}
		const Vector X = solver.solve(-G);

		if(solver.info() != Eigen::Success) {
			throw std::runtime_error("solve(): solve failed");
		}

		// apply update to nodes 1..N-1
		for(int k = 1; k < N; ++k)
		{
			const auto bk = 2 * (k - 1);
			const auto delta = Vec2(X[bk + 0], X[bk + 1]);
			nodes_[k] += delta;
		}
		return error;
	}

private:
	std::vector<Vec2> nodes_;
	std::vector<Edge> edges_;

	// map node id -> block offset in reduced vector (node0 removed)
	static int block_index(int node_id)
	{
		if(node_id <= 0) {
			return -1;
		}
		return 2 * (node_id - 1);
	}
};


} // mmpilot

#endif /* INCLUDE_MMPILOT_POSE_GRAPH_H_ */
