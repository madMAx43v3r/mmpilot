
if(LIBCAMERA_FOUND)
	add_executable(record_all.cpp record_all.cpp)
	target_link_libraries(record_all mmpilot_camera)
endif()
