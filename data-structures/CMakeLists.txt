add_library(data_structures INTERFACE)

target_sources(data_structures INTERFACE
    OccupancyBitmap.h
    SPSCBuffer.h
    SPSCBufferV1.h
    SPSCBufferV2.h
    SPSCBufferV3.h
)

target_include_directories(data_structures INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(Threads REQUIRED)
add_executable(spsc_versions_bench benchmarks/spsc_versions.cpp)
target_link_libraries(spsc_versions_bench PRIVATE data_structures Threads::Threads)
