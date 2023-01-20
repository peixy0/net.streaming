# net.streaming

Simple IPCam written in modern C++.

# note

Edit config.yaml to enable hardware acceleration for your own setup.
Refer to config-rpi.yaml for example configurations for Raspberry Pi.

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
