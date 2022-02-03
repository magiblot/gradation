cmake_minimum_required(VERSION 3.5)

project(gradation)

add_library(gradation-vd SHARED
    source/gradation.cpp
    source/gradation.rc
    source/vd.cpp
)

set_target_properties(gradation-vd PROPERTIES OUTPUT_NAME "gradation")
set_target_properties(gradation-vd PROPERTIES SUFFIX ".vdf")

target_include_directories(gradation-vd PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${CMAKE_CURRENT_LIST_DIR}/include/vd2/OldFilterSdk"
)

if (WIN32)
    target_compile_definitions(gradation-vd PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(gradation-vd PRIVATE
        Comctl32
    )
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(gradation-vd PRIVATE
        /Zc:__cplusplus
        /Zc:inline
    )
endif()
