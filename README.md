# net.system
Simple async HTTP interface written in modern C++

# build
```bash
git clone https://github.com/peixy0/net.system
cd net.system
mkdir externals
git clone https://github.com/google/googletest  externals/googletest
git clone https://github.com/gabime/spdlog externals/spdlog
git clone https://github.com/nlohmann/json externals/json
mkdir build
cd build
cmake .. -GNinja -DBUILD_TESTS=ON
ninja
```

# example
see src/main
