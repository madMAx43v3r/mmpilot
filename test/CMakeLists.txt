
add_executable(test_camera test_camera.cpp ../src/camera.cpp)
add_executable(test_opengl_fbo test_opengl_fbo.cpp)

target_link_libraries(test_camera camera)
target_link_libraries(test_opengl_fbo EGL GLESv2)
