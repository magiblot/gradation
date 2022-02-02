cmake_minimum_required(VERSION 3.5)

project(gradation)

add_library(gradation-vd SHARED
    Source/gradation.rc
    Source/gradation.cpp
    Source/vd.cpp
)

target_compile_definitions(gradation-vd PRIVATE
    _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(gradation-vd PRIVATE
    Comctl32
)

set_target_properties(gradation-vd PROPERTIES OUTPUT_NAME "gradation")
set_target_properties(gradation-vd PROPERTIES SUFFIX ".vdf")
