add_library(xgameruntime SHARED
        main.cpp
        ${IDL_GENERATED_FILES} )

if(WIN32)
    set_target_properties(xgameruntime PROPERTIES
            PREFIX "" )
endif()

target_link_libraries( xgameruntime PRIVATE
        xgameruntime-common )

target_include_directories(xgameruntime
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${IDL_BINARY_DIR} )

target_compile_features(xgameruntime
        PUBLIC
        cxx_std_23 )