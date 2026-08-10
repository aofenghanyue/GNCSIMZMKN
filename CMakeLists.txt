cmake_minimum_required(VERSION 3.20)

project(
    GNCZMKNNext
    VERSION 0.0.0
    DESCRIPTION "Greenfield implementation skeleton for the GNCZMKN target architecture"
    LANGUAGES CXX)

option(GNC_BUILD_TESTS "Build repository smoke tests" ON)
option(GNC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

include(cmake/GncWarnings.cmake)

add_library(gnc_build_options INTERFACE)
target_compile_features(gnc_build_options INTERFACE cxx_std_17)
target_include_directories(
    gnc_build_options
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/framework/include>)

function(gnc_add_interface_module target)
    add_library(${target} INTERFACE)
    target_link_libraries(${target} INTERFACE gnc_build_options)
    add_library(gnc::${target} ALIAS ${target})
endfunction()

gnc_add_interface_module(foundation)
gnc_add_interface_module(contracts)
target_link_libraries(contracts INTERFACE foundation)

gnc_add_interface_module(model_sdk)
target_link_libraries(model_sdk INTERFACE foundation contracts)

gnc_add_interface_module(compiler)
target_link_libraries(compiler INTERFACE foundation contracts model_sdk)

gnc_add_interface_module(kernel)
target_link_libraries(kernel INTERFACE foundation contracts)

gnc_add_interface_module(evidence)
target_link_libraries(evidence INTERFACE foundation contracts)

gnc_add_interface_module(workflow)
target_link_libraries(workflow INTERFACE foundation contracts evidence)

gnc_add_interface_module(application)
target_link_libraries(
    application
    INTERFACE foundation contracts compiler kernel evidence workflow)

gnc_add_interface_module(adapters)
target_link_libraries(adapters INTERFACE foundation contracts application)

add_executable(gnc_sim apps/cli/main.cpp)
target_link_libraries(gnc_sim PRIVATE adapters)
gnc_enable_warnings(gnc_sim)

if(GNC_BUILD_TESTS)
    include(CTest)
    enable_testing()
    find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)

    add_executable(gnc_skeleton_smoke tests/skeleton_smoke.cpp)
    target_link_libraries(gnc_skeleton_smoke PRIVATE foundation)
    gnc_enable_warnings(gnc_skeleton_smoke)

    add_executable(
        gnc_scientific_conventions_properties
        tests/scientific_conventions_properties.cpp)
    target_link_libraries(gnc_scientific_conventions_properties PRIVATE foundation)
    gnc_enable_warnings(gnc_scientific_conventions_properties)

    add_test(NAME skeleton.library COMMAND gnc_skeleton_smoke)
    add_test(NAME skeleton.cli COMMAND gnc_sim --self-check)
    add_test(
        NAME scientific_conventions.properties
        COMMAND gnc_scientific_conventions_properties)
    add_test(
        NAME scientific_conventions.python_reference
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/oracles/scientific-conventions/reference.py
            --self-test)
    add_test(
        NAME scientific_conventions.cross_tool
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/oracles/scientific-conventions/cross_tool_check.py
            --cpp $<TARGET_FILE:gnc_scientific_conventions_properties>
            --cases ${CMAKE_CURRENT_SOURCE_DIR}/oracles/scientific-conventions/cases.csv
            --cpp-source ${CMAKE_CURRENT_SOURCE_DIR}/tests/scientific_conventions_properties.cpp
            --cmake-cache ${CMAKE_BINARY_DIR}/CMakeCache.txt
            --output ${CMAKE_CURRENT_BINARY_DIR}/evidence/r0-sci-001-cross-tool-report.json)
    set_tests_properties(
        scientific_conventions.properties
        scientific_conventions.python_reference
        scientific_conventions.cross_tool
        PROPERTIES LABELS "r0;science")
endif()
