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

    add_executable(gnc_skeleton_smoke tests/skeleton_smoke.cpp)
    target_link_libraries(gnc_skeleton_smoke PRIVATE foundation)
    gnc_enable_warnings(gnc_skeleton_smoke)

    # R0 validation spike: intentionally isolated from product modules and Legacy.
    add_executable(
        gnc_scientific_conventions
        tests/scientific_conventions.cpp)
    target_compile_features(gnc_scientific_conventions PRIVATE cxx_std_17)
    gnc_enable_warnings(gnc_scientific_conventions)

    # R0 scientific oracle probe: fixture-local and isolated from product code.
    add_executable(
        gnc_minimal_3dof_probe
        tests/minimal_3dof.cpp)
    target_compile_features(gnc_minimal_3dof_probe PRIVATE cxx_std_17)
    gnc_enable_warnings(gnc_minimal_3dof_probe)

    add_test(NAME skeleton.library COMMAND gnc_skeleton_smoke)
    add_test(NAME skeleton.cli COMMAND gnc_sim --self-check)
    add_test(
        NAME r0.scientific-conventions.properties
        COMMAND gnc_scientific_conventions --self-check)
    add_test(
        NAME r0.minimal-3dof.probe
        COMMAND gnc_minimal_3dof_probe --self-check)

    find_program(GNC_POWERSHELL_EXECUTABLE NAMES pwsh powershell)
    if(NOT GNC_POWERSHELL_EXECUTABLE)
        message(FATAL_ERROR "PowerShell is required for R0 conformance tests.")
    endif()
    add_test(
        NAME r0.schema-contracts
        COMMAND
            ${GNC_POWERSHELL_EXECUTABLE}
            -NoLogo
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate-r0-specs.ps1)
    add_test(
        NAME r0.architecture-baseline
        COMMAND
            ${GNC_POWERSHELL_EXECUTABLE}
            -NoLogo
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate-architecture-baseline.ps1)
    add_test(
        NAME r0.legacy-reproduction-evidence
        COMMAND
            ${GNC_POWERSHELL_EXECUTABLE}
            -NoLogo
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate-legacy-reproduction.ps1
            -Quiet)
    add_test(
        NAME r0.license-provenance
        COMMAND
            ${GNC_POWERSHELL_EXECUTABLE}
            -NoLogo
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate-license-provenance.ps1
            -Quiet)
    find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
    add_test(
        NAME r0.scientific-conventions.cross-tool
        COMMAND
            ${GNC_POWERSHELL_EXECUTABLE}
            -NoLogo
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate-scientific-conventions.ps1
            -CppExecutable $<TARGET_FILE:gnc_scientific_conventions>
            -PythonExecutable ${Python3_EXECUTABLE}
            -Quiet)
    add_test(
        NAME r0.minimal-3dof.oracle
        COMMAND
            ${Python3_EXECUTABLE}
            -I
            -B
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/minimal_3dof_reference.py
            --cases
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/ref-minimal-3dof/cases.json
            --oracle
            ${CMAKE_CURRENT_SOURCE_DIR}/oracles/ref-minimal-3dof/reference.json
            --probe
            $<TARGET_FILE:gnc_minimal_3dof_probe>)
endif()
