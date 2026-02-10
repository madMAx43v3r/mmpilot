project(mmpilot)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig)
find_package(Threads REQUIRED)

if(PkgConfig_FOUND)
    pkg_check_modules(LIBCAMERA libcamera)
    pkg_check_modules(LIBCAMERA_BASE libcamera-base)
    
    pkg_check_modules(EGL REQUIRED egl)
	pkg_check_modules(GLESv3 REQUIRED glesv2)
endif()


include_directories(
	include
)

add_library(mmpilot_util STATIC
	src/util.cpp
)

add_library(mmpilot_opengl STATIC
	src/egl.cpp
	src/opengl.cpp
	src/render.cpp
	src/bayer.cpp
)

target_include_directories(mmpilot_opengl
	PUBLIC ${EGL_INCLUDE_DIRS} ${GLESv3_INCLUDE_DIRS}
)

target_link_directories(mmpilot_opengl
	PRIVATE ${EGL_LIBRARY_DIRS} ${GLESv3_LIBRARY_DIRS}
)

target_link_libraries(mmpilot_opengl
	PUBLIC mmpilot_util
	PRIVATE ${EGL_LIBRARIES} ${GLESv3_LIBRARIES}
)

if(LIBCAMERA_FOUND)
	add_library(mmpilot_camera STATIC
		src/camera.cpp
	)
	target_link_libraries(mmpilot_camera camera camera-base)
endif()

add_subdirectory(test)
