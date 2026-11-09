add_library(order_book_3_0 STATIC OrderBook/3_0/OrderBook3_0.cpp)

target_sources(order_book_3_0 PRIVATE
    OrderBook/3_0/OrderBook3_0.h
)

target_include_directories(order_book_3_0 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(order_book_3_0 PUBLIC matching_engine_shared)

add_library(order_book_3_1 STATIC OrderBook/3_1/OrderBook3_1.cpp)
target_sources(order_book_3_1 PRIVATE OrderBook/3_1/OrderBook3_1.h)
target_include_directories(order_book_3_1 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(order_book_3_1 PUBLIC matching_engine_shared)

find_package(Threads REQUIRED)
add_executable(matching_engine_v3 main.cpp)
set_target_properties(matching_engine_v3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(matching_engine_v3 PRIVATE order_book_3_0 order_book_3_1 data_structures Threads::Threads)

add_executable(matching_engine_v3_1 main.cpp)
set_target_properties(matching_engine_v3_1 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(matching_engine_v3_1 PRIVATE DEFAULT_ENGINE_VERSION="3.1")
target_link_libraries(matching_engine_v3_1 PRIVATE order_book_3_0 order_book_3_1 data_structures Threads::Threads)

if(APPLE)
    foreach(target matching_engine_v3 matching_engine_v3_1)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND /usr/bin/xcrun dsymutil
                    "$<TARGET_FILE:${target}>"
                    -o "$<TARGET_FILE:${target}>.dSYM"
            COMMAND /usr/bin/codesign --force --sign -
                    --entitlements "${PROJECT_SOURCE_DIR}/shared/profiling.entitlements"
                    "$<TARGET_FILE:${target}>"
            VERBATIM
        )
    endforeach()
endif()
