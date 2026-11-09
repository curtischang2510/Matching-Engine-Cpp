add_library(order_book_5_0 STATIC OrderBook/5_0/OrderBook5_0.cpp)
target_sources(order_book_5_0 PRIVATE OrderBook/5_0/OrderBook5_0.h OrderBook/OrderBookTypes.h)
target_include_directories(order_book_5_0 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(order_book_5_0 PUBLIC matching_engine_shared data_structures)

add_library(order_book_5_1 STATIC OrderBook/5_1/OrderBook5_1.cpp)
target_sources(order_book_5_1 PRIVATE OrderBook/5_1/OrderBook5_1.h OrderBook/OrderBookTypes.h)
target_include_directories(order_book_5_1 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Boost CONFIG REQUIRED)
target_link_libraries(order_book_5_1 PUBLIC matching_engine_shared data_structures Boost::headers)

add_library(order_book_5_2 STATIC OrderBook/5_2/OrderBook5_2.cpp)
target_sources(order_book_5_2 PRIVATE OrderBook/5_2/OrderBook5_2.h OrderBook/OrderBookTypes.h)
target_include_directories(order_book_5_2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(order_book_5_2 PUBLIC matching_engine_shared data_structures Boost::headers)

add_library(order_book_5_4 STATIC OrderBook/5_4/OrderBook5_4.cpp)
target_sources(order_book_5_4 PRIVATE OrderBook/5_4/OrderBook5_4.h OrderBook/OrderBookTypes.h)
target_include_directories(order_book_5_4 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(order_book_5_4 PUBLIC matching_engine_shared data_structures)

find_package(Threads REQUIRED)
foreach(version 5_0 5_1 5_2 5_4)
    add_executable(matching_engine_v${version} ../V4/main.cpp)
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

add_executable(order_book_list_test tests/order_book_list_test.cpp)
target_link_libraries(order_book_list_test PRIVATE order_book_4_0 order_book_5_0 order_book_5_1 order_book_5_2 order_book_5_4)

add_executable(delayed_cancel_bench benchmarks/delayed_cancel_bench.cpp)
target_link_libraries(delayed_cancel_bench PRIVATE order_book_4_0 order_book_5_0 order_book_5_1 order_book_5_2 order_book_5_4)
if(APPLE)
    add_custom_command(TARGET delayed_cancel_bench POST_BUILD
        COMMAND /usr/bin/xcrun dsymutil
                "$<TARGET_FILE:delayed_cancel_bench>"
                -o "$<TARGET_FILE:delayed_cancel_bench>.dSYM"
        COMMAND /usr/bin/codesign --force --sign -
                --entitlements "${PROJECT_SOURCE_DIR}/shared/profiling.entitlements"
                "$<TARGET_FILE:delayed_cancel_bench>"
        VERBATIM
    )
endif()
