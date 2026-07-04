
if(LIBCAMERA_FOUND)
	add_executable(test_camera test_camera.cpp)
	target_link_libraries(test_camera mmpilot_camera mmpilot_util)
endif()

add_executable(export_images export_images.cpp)

add_executable(test_beta_msp test_beta_msp.cpp)
add_executable(test_msp_override test_msp_override.cpp)
add_executable(test_opengl_fbo test_opengl_fbo.cpp)
add_executable(test_pose_graph test_pose_graph.cpp)
add_executable(test_gpx_parse test_gpx_parse.cpp)
add_executable(test_controller test_controller.cpp)

target_link_libraries(export_images mmpilot_util)

target_link_libraries(test_beta_msp mmpilot_util Threads::Threads)
target_link_libraries(test_msp_override mmpilot_util Threads::Threads)
target_link_libraries(test_opengl_fbo EGL GLESv2)
target_link_libraries(test_pose_graph mmpilot_math)
target_link_libraries(test_gpx_parse pugixml)
target_link_libraries(test_controller mmpilot_math)

add_subdirectory(bench)
add_subdirectory(vapor1)
