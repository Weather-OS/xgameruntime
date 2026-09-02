add_library(winrt STATIC
        IAsyncImpl.cpp )

add_dependencies(winrt
        xgameruntime-idl )

target_link_libraries( winrt PRIVATE
        runtimeobject )

target_include_directories(winrt
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${IDL_BINARY_DIR} )