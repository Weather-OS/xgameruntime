cmake_minimum_required( VERSION 3.20 )

project( xgameruntime LANGUAGES C CXX )

include( ExternalProject )

find_program(WIDL
        NAMES
        widl
        x86_64-w64-mingw32-widl
        REQUIRED )

message( STATUS "Using widl: ${WIDL}" )

set(IDL_SOURCE_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/include )

set(IDL_BINARY_DIR
        ${CMAKE_CURRENT_BINARY_DIR}/include )

include_directories( ${PROJECT_SOURCE_DIR}/include )

add_subdirectory( libs )
add_subdirectory( src )