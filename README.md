# mmpilot

 mmpilot is a visual-inertial navigation and autonomous flight-control stack for a small
 quadcopter, written in C++17 by madMAx43v3r (Maxim, also known for the Chia blockchain). It is
 designed to run on a Raspberry Pi carrying a fisheye camera, talking to a Betaflight/INAV flight
 controller over a serial UART using the MSP v2 protocol.

 In short: it turns a standard FPV/racing drone into a self-positioning, self-mapping, self-flying
 vehicle using only a cheap wide-angle camera, an IMU, GPS, and a barometer — no LiDAR, no RTK, no
 expensive sensors. All heavy image processing is done on the GPU via OpenGL ES shaders.

 - License: GPL v3
 - Language: C++17
 - Build: CMake (see CMakeLists.txt, make_debug.sh, make_devel.sh)
 - Dependencies: libeigen3-dev, libturbojpeg0-dev, libegl1-mesa-dev, libpugixml-dev, libcamera (Pi
   camera stack), OpenGL ES 3

## Install

```
sudo apt install libeigen3-dev libturbojpeg0-dev pkg-config libegl1-mesa-dev libpugixml-dev
```
