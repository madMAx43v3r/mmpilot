/*
 * test_pose_graph.cpp
 *
 *  Created on: Feb 18, 2026
 *      Author: mad
 */

#include <mmpilot/pose_graph.h>

#include <iostream>

using namespace mmpilot;


int main()
{
	PoseGraphXY<double> graph;

	const int N = 6;

	for(int i = 0; i < N; ++i)
	{
		graph.add_node(Vec2d(i, 0));
		if(i > 0) {
			graph.add_edge(i - 1, i, Vec2d(1, 0));
		}
	}
	graph.add_edge(0, N - 1, Vec2d(N, 1), 2);

	for(const auto& node : graph.nodes()) {
		std::cout << node.transpose() << std::endl;
	}

	const auto error = graph.solve();

	std::cout << "error = " << error << std::endl;

	for(const auto& node : graph.nodes()) {
		std::cout << node.transpose() << std::endl;
	}

}


