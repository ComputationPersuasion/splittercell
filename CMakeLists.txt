cmake_minimum_required(VERSION 3.6)

include_directories(include)

add_library(splittercell src/distribution.cpp src/flock.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -O3 -pedantic -Wall -pthread")
