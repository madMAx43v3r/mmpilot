
if(LIBCAMERA_FOUND)
	add_executable(record_all record_all.cpp)
	target_link_libraries(record_all mmpilot_camera mmpilot_util)
endif()
