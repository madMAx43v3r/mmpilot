/*
 * gpx.h
 *
 *  Created on: Jun 28, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_GPX_H_
#define INCLUDE_MMPILOT_GPX_H_

#include <pugixml.hpp>

#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <iostream>


namespace mmpilot {

struct GpxPoint {
	double lat = 0;
	double lon = 0;
	std::optional<double> ele;
	std::optional<std::string> name;
};

struct GpxRoute {
	std::string name;
	std::vector<GpxPoint> points;
};

struct GpxFile {
	std::vector<GpxRoute> routes;
};

inline std::optional<double> read_optional_double(pugi::xml_node node)
{
	if(!node) {
		return std::nullopt;
	}

	return node.text().as_double();
}

inline std::optional<std::string> read_optional_string(pugi::xml_node node)
{
	if(!node) {
		return std::nullopt;
	}

	return std::string(node.text().as_string());
}

inline GpxFile gpx_read_file(const std::string& path)
{
	pugi::xml_document doc;

	pugi::xml_parse_result result = doc.load_file(path.c_str());

	if(!result) {
		throw std::runtime_error(std::string("Failed to parse GPX file: ") + result.description());
	}
	pugi::xml_node gpx = doc.child("gpx");

	if(!gpx) {
		throw std::runtime_error("Invalid GPX file: missing <gpx> root element");
	}
	GpxFile out;

	for(pugi::xml_node rte : gpx.children("rte"))
	{
		GpxRoute route;

		if(pugi::xml_node name = rte.child("name"))
		{
			route.name = name.text().as_string();
		}

		for(pugi::xml_node rtept : rte.children("rtept"))
		{
			pugi::xml_attribute lat_attr = rtept.attribute("lat");
			pugi::xml_attribute lon_attr = rtept.attribute("lon");

			if(!lat_attr || !lon_attr) {
				throw std::runtime_error("Invalid GPX route point: missing lat/lon");
			}

			GpxPoint point;
			point.lat = lat_attr.as_double();
			point.lon = lon_attr.as_double();

			point.ele = read_optional_double(rtept.child("ele"));
			point.name = read_optional_string(rtept.child("name"));

			route.points.push_back(std::move(point));
		}

		out.routes.push_back(std::move(route));
	}

	return out;
}

inline std::vector<GpxPoint> gpx_combine_route(const GpxFile& gpx)
{
	std::vector<GpxPoint> out;

	for(const auto& route : gpx.routes) {
		out.insert(out.end(), route.points.begin(), route.points.end());
	}
	return out;
}


inline void gpx_print_file(const std::string& file_path)
{
	try {
		GpxFile gpx = gpx_read_file(file_path);

		std::cout << "Loaded " << gpx.routes.size() << " route(s)\n";

		for(const auto& route : gpx.routes)
		{
			std::cout << "Route: " << route.name << " points=" << route.points.size() << std::endl;

			for(const auto& p : route.points)
			{
				std::cout << "  lat = " << p.lat << " lon = " << p.lon;

				if(p.ele) {
					std::cout << " ele = " << *p.ele;
				}
				if(p.name) {
					std::cout << " name = " << *p.name;
				}
				std::cout << std::endl;
			}
		}
	} catch(const std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
}


} // mmpilot

#endif /* INCLUDE_MMPILOT_GPX_H_ */
