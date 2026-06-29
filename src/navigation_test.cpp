/*
 * navigation_test.cpp
 *
 *  Created on: Jun 29, 2026
 *      Author: mad
 */

#include <mmpilot/navigation.h>

using namespace mmpilot;


class DisplayStage : public Stage {
public:
	DisplayStage() : Stage("display") {}

	void init() override {
		// TODO
	}

	void exec() override {
		// TODO
	}
};


class NavigationTest : public NavigationBase {
public:
	DisplayStage display;

	NavigationTest() {
		pipe.add_stage(&display);
	}

};


int main()
{
	NavigationTest test;

	test.exec();

	return 0;
}

