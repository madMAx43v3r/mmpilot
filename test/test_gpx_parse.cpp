/*
 * test_gpx.cpp
 *
 *  Created on: Jun 28, 2026
 *      Author: mad
 */

#include <mmpilot/gpx.h>


int main(int argc, char** argv)
{
	if(argc < 2) {
		return -1;
	}

	mmpilot::gpx_print_file(argv[1]);

	return 0;
}

