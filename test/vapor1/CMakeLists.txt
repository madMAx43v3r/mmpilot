
if(LIBCAMERA_FOUND)
	add_executable(vapor1_record_all record_all.cpp)
	add_executable(vapor1_run_test run_test.cpp)
	add_executable(vapor1_test_control test_control.cpp)
	target_link_libraries(vapor1_record_all mmpilot_camera mmpilot_util mmpilot_jpeg)
	target_link_libraries(vapor1_run_test mmpilot_camera mmpilot_util mmpilot_math mmpilot_vision mmpilot_display mmpilot_jpeg Threads::Threads)
	target_link_libraries(vapor1_test_control mmpilot_camera mmpilot_util mmpilot_math mmpilot_vision mmpilot_display mmpilot_jpeg Threads::Threads)
endif()

add_executable(vapor1_test_replay test_replay.cpp)
target_link_libraries(vapor1_test_replay mmpilot_vision mmpilot_math mmpilot_display mmpilot_jpeg Threads::Threads)
