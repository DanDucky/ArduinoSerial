cmake_minimum_required(VERSION 3.24)
project(ArduinoSerial)

set(CMAKE_CXX_STANDARD 23)

add_executable(ArduinoSerial src/main.cpp)

file (DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/get_cpm.cmake
        ${CMAKE_SOURCE_DIR}/include/cmake/get_cpm.cmake)
file(DOWNLOAD
        https://raw.githubusercontent.com/p-ranav/indicators/master/single_include/indicators/indicators.hpp
        ${CMAKE_SOURCE_DIR}/include/indicators.hpp)
file(DOWNLOAD
        https://raw.githubusercontent.com/DanDucky/ArgumentParser/master/include/CLIParser.hpp
        ${CMAKE_SOURCE_DIR}/include/optargParser.hpp
)

include(include/cmake/get_cpm.cmake)

execute_process(COMMAND
        pip install catkin_pkg)
execute_process(COMMAND
        pip install empy)

CPMAddPackage("gh:google/googletest@1.14.0")
CPMAddPackage("gh:ros/catkin#0.8.10")
set(catkin_EXTRAS_DIR ${catkin_SOURCE_DIR}/cmake)
set(CATKIN_DEVEL_PREFIX ${CMAKE_BINARY_DIR}/devel)
set(GTEST_LIBRARIES gtest)
set(BUILD_SHARED_LIBS OFF)
CPMAddPackage(
        NAME serial
        GIT_TAG 1.2.1
        GITHUB_REPOSITORY wjwwood/serial
        OPTIONS
            "BUILD_SHARED_LIBS OFF"
            "CMAKE_CXX_FLAGS -I${CMAKE_BINARY_DIR}/_deps/serial-src/include -static -lstdc++ -lm"
)

set(CMAKE_CXX_FLAGS "-static")

target_include_directories(ArduinoSerial PUBLIC ${serial_SOURCE_DIR}/include)
target_include_directories(ArduinoSerial PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(ArduinoSerial ${CPM_LAST_PACKAGE_NAME} stdc++ m)

set(CMAKE_INSTALL_COMPONENT "no")
set(CMAKE_INSTALL_COMPONENT OFF)
install(TARGETS ArduinoSerial)
