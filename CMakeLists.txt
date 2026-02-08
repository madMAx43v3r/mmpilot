project(mmpilot)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig)

if(PkgConfig_FOUND)
    pkg_check_modules(LIBCAMERA libcamera)
    pkg_check_modules(LIBCAMERA_BASE libcamera-base)
endif()


include_directories(
	include
)

add_library(mmpilot_util STATIC
	src/util.cpp
)

if(LIBCAMERA_FOUND)
	add_library(mmpilot_camera STATIC
		src/camera.cpp
	)
	target_link_libraries(mmpilot_camera camera camera-base)
endif()

add_subdirectory(test)
