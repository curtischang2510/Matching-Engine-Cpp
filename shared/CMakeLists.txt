add_library(matching_engine_shared INTERFACE)
target_sources(matching_engine_shared INTERFACE
    MatchingEngine.h
    Orders/orders.h
    Orders/orderParser.h
)
target_include_directories(matching_engine_shared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
