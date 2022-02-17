cmake_minimum_required(VERSION 3.5)

project(gradation)

function(common_compile_settings t)
    target_compile_features(${t} PUBLIC cxx_std_14)
    set_target_properties(${t} PROPERTIES CXX_VISIBILITY_PRESET "hidden")

    if (WIN32)
        target_compile_definitions(${t} PRIVATE
            _CRT_NONSTDC_NO_WARNINGS
            _CRT_SECURE_NO_WARNINGS
        )
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${t} PRIVATE
            /Zc:__cplusplus
            /Zc:inline
        )
    endif()
endfunction()

# Target gradation-vd

if (WIN32)
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

    common_compile_settings(gradation-vd)

    target_link_libraries(gradation-vd PRIVATE
        Comctl32
    )
endif()

# Target gradation-avs

add_library(gradation-avs SHARED
    source/avs.cpp
    source/gradation.cpp
)

target_include_directories(gradation-avs PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/include/avisynth"
)

common_compile_settings(gradation-avs)
