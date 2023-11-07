cmake_minimum_required(VERSION 3.27)
project(ArduinoSerial)

set(CMAKE_CXX_STANDARD 23)

add_executable(ArduinoSerial src/main.cpp)

target_link_libraries(ArduinoSerial serial ncurses)
