# Compile C++ as C++17 on every target (the ESP32 Arduino core defaults to C++11).
Import("env")
env.Append(CXXFLAGS=["-std=gnu++17"])
