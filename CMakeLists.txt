cmake_minimum_required( VERSION 3.20 )

project( xgameruntime LANGUAGES C CXX )

include( ExternalProject )

find_program(WIDL
        NAMES
        widl )

if (NOT WIDL)
    find_program(MINGW_WIDL
            NAMES
            x86_64-w64-mingw32-widl )

    if (MINGW_WIDL)
        message(WARNING
                "x86_64-w64-mingw32-widl was found at ${MINGW_WIDL},"
                "but it is not supported. This project requires Wine's widl." )
    endif()

    message(FATAL_ERROR
            "Could not find a supported widl executable."
            "Please install Wine's widl." )
endif()

message( STATUS "Using widl: ${WIDL}" )

set(IDL_SOURCE_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/include )

set(IDL_BINARY_DIR
        ${CMAKE_CURRENT_BINARY_DIR}/include )

include_directories( ${PROJECT_SOURCE_DIR}/include )

add_subdirectory( libs )
add_subdirectory( src )