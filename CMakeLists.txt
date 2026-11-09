cmake_minimum_required(VERSION 3.20)
project(MatchingEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(data-structures)
add_subdirectory(shared)
add_subdirectory(V3)
add_subdirectory(V4)
add_subdirectory(V5)
