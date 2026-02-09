
if(LIBCAMERA_FOUND)
	add_executable(record_all record_all.cpp)
	target_link_libraries(record_all mmpilot_camera mmpilot_util)
endif()

add_executable(test_replay test_replay.cpp)
target_link_libraries(test_replay mmpilot_util Threads::Threads)
