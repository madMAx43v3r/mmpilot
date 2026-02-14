
if(LIBCAMERA_FOUND)
	add_executable(bench_record_all record_all.cpp)
	add_executable(bench_run_test run_test.cpp)
	target_link_libraries(bench_record_all mmpilot_camera mmpilot_util mmpilot_jpeg)
	target_link_libraries(bench_run_test mmpilot_camera mmpilot_util mmpilot_vision mmpilot_display mmpilot_jpeg Threads::Threads)
endif()

add_executable(bench_test_replay test_replay.cpp)
target_link_libraries(bench_test_replay mmpilot_vision mmpilot_display mmpilot_jpeg Threads::Threads)
