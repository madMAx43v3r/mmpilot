
if(LIBCAMERA_FOUND)
	add_executable(test_camera test_camera.cpp)
	target_link_libraries(test_camera mmpilot_camera mmpilot_util)
endif()

add_executable(test_beta_msp test_beta_msp.cpp)
add_executable(test_opengl_fbo test_opengl_fbo.cpp)

target_link_libraries(test_beta_msp mmpilot_util Threads::Threads)
target_link_libraries(test_opengl_fbo EGL GLESv2)

add_subdirectory(bench)
add_subdirectory(vapor1)
