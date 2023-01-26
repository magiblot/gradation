cmake_minimum_required(VERSION 3.5)

option(GRADATION_BUILD_TESTS "Build and run tests" OFF)

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

# Target 'tests'

if (GRADATION_BUILD_TESTS)
    file(GLOB_RECURSE TEST_SRC "${CMAKE_CURRENT_LIST_DIR}/test/*.cpp")
    add_executable(gradation-test
        source/gradation.cpp
        ${TEST_SRC}
    )
    target_include_directories(gradation-test PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/source"
    )
    find_library(GTEST gtest REQUIRED)
    find_library(GTEST_MAIN gtest_main REQUIRED)
    target_link_libraries(gradation-test PRIVATE
        ${GTEST}
        ${GTEST_MAIN}
    )
    add_custom_command(
        OUTPUT gradation-test-passed
        COMMAND gradation-test
        COMMAND ${CMAKE_COMMAND} -E touch gradation-test-passed
        DEPENDS gradation-test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    add_custom_target(gradation-run-test ALL DEPENDS gradation-test-passed)
    common_compile_settings(gradation-test)
endif()
