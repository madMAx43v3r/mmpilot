/*
 * helpers.cpp
 *
 *  Created on: Feb 5, 2026
 *      Author: mad
 */

#include <mmpilot/helpers.h>

#include <fstream>
#include <iostream>
#include <csignal>

#include <unistd.h>


namespace mmpilot {

volatile sig_atomic_t g_shutdown = 0;

extern "C" void handle_sigint(int)
{
	if(g_shutdown) {
		::exit(-1);
	}
	g_shutdown = 1;
}

void wait_for_exit()
{
	std::signal(SIGINT, handle_sigint);
	while(!g_shutdown) {
		::pause();   // sleeps until any signal is delivered
	}
}

std::string read_file_txt(const std::string& path)
{
	std::ifstream file(path, std::ios::binary);
	if(!file) {
		throw std::runtime_error("Failed to open file: " + path);
	}
	return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}



} // mmpilot


