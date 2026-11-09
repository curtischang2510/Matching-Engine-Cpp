add_library(order_book_4_0 STATIC OrderBook/4_0/OrderBook4_0.cpp)
target_sources(order_book_4_0 PRIVATE OrderBook/4_0/OrderBook4_0.h OrderBook/OrderBookTypes.h)
target_include_directories(order_book_4_0 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(order_book_4_0 PUBLIC matching_engine_shared data_structures)

add_library(order_book_4_1 STATIC OrderBook/4_1/OrderBook4_1.cpp)
target_sources(order_book_4_1 PRIVATE OrderBook/4_1/OrderBook4_1.h)
target_include_directories(order_book_4_1 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Boost CONFIG REQUIRED)
target_link_libraries(order_book_4_1 PUBLIC matching_engine_shared data_structures Boost::headers)

find_package(Threads REQUIRED)
foreach(version 4_0 4_1)
    add_executable(matching_engine_v${version} main.cpp)
    set_target_properties(matching_engine_v${version} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_compile_definitions(matching_engine_v${version} PRIVATE ENGINE_VERSION_${version})
    target_link_libraries(matching_engine_v${version} PRIVATE order_book_${version} data_structures Threads::Threads)
    if(APPLE)
        add_custom_command(TARGET matching_engine_v${version} POST_BUILD
            COMMAND /usr/bin/xcrun dsymutil
                    "$<TARGET_FILE:matching_engine_v${version}>"
                    -o "$<TARGET_FILE:matching_engine_v${version}>.dSYM"
            COMMAND /usr/bin/codesign --force --sign -
                    --entitlements "${PROJECT_SOURCE_DIR}/shared/profiling.entitlements"
                    "$<TARGET_FILE:matching_engine_v${version}>"
            VERBATIM
        )
    endif()
endforeach()

add_executable(order_book_4_0_test tests/order_book_test.cpp)
target_link_libraries(order_book_4_0_test PRIVATE order_book_4_0)

add_executable(order_book_4_1_test tests/order_book_4_1_test.cpp)
target_link_libraries(order_book_4_1_test PRIVATE order_book_4_1)
