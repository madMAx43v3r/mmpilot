
if(LIBCAMERA_FOUND)
	add_executable(record_all record_all.cpp)
	add_executable(run_test run_test.cpp)
	target_link_libraries(record_all mmpilot_camera mmpilot_util mmpilot_jpeg)
	target_link_libraries(run_test mmpilot_camera mmpilot_util mmpilot_vision mmpilot_display mmpilot_jpeg Threads::Threads)
endif()

add_executable(test_replay test_replay.cpp)
target_link_libraries(test_replay mmpilot_vision mmpilot_display mmpilot_jpeg Threads::Threads)
