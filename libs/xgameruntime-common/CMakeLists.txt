add_library(xgameruntime-common STATIC
        env.c
        logging.c )

target_include_directories(xgameruntime-common
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${IDL_BINARY_DIR} )