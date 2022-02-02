cmake_minimum_required(VERSION 3.5)

project(gradation)

add_library(gradation-vd SHARED
    Source/gradation.rc
    Source/gradation.cpp
    Source/vd.cpp
)

set_target_properties(gradation-vd PROPERTIES OUTPUT_NAME "gradation")
set_target_properties(gradation-vd PROPERTIES SUFFIX ".vdf")

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