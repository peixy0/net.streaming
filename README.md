# net.streaming

Simple IPCam written in modern C++

# note
To use on Raspberry Pi or other devices with various hardware codec support, edit options in src/main/main.cpp.

Example:

```
-  encoderOptions.codec = "hevc_qsv";
+  encoderOptions.codec = "h264_v4l2m2m";
```

# build

```bash
sudo apt install libavcodec-dev libavutil-dev libavfilter-dev libavformat-dev
git clone https://github.com/peixy0/net.streaming
cd net.streaming
mkdir externals
git clone https://github.com/gabime/spdlog externals/spdlog
git clone https://github.com/jbeder/yaml-cpp.git externals/yaml-cpp
git clone https://github.com/google/googletest externals/googletest
mkdir build
cd build
cmake .. -GNinja -DBUILD_TESTS=ON
ninja
```
