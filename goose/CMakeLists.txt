cmake_minimum_required(VERSION 3.20)
project(XylarGoose LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
include(CTest)
option(GOOSE_BUILD_SANDBOX "Build the separate original voxel prototype (not the Rust port)" OFF)
option(GOOSE_TEST_GRAPHICS "Run sandbox graphics tests (requires a display or SDL offscreen driver)" OFF)
option(GOOSE_TEST_PORT_PARITY "Compare translated modules with the pinned Rust checkout" OFF)

# Keep the earlier sandbox out of the normal port build. Its windowing backend
# is unrelated to the translated client, and SDL/OpenGL are unnecessary for the
# pure Goose foundation modules.
if(GOOSE_BUILD_SANDBOX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    find_package(OpenGL REQUIRED)
    add_library(goose_mesh STATIC native/mesh.c)
    target_include_directories(goose_mesh PUBLIC native)
    add_library(goose_platform STATIC native/platform.c)
    target_link_libraries(goose_platform PUBLIC goose_mesh PkgConfig::SDL2 OpenGL::GL m)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(goose_mesh PRIVATE -Wall -Wextra -Werror)
        target_compile_options(goose_platform PRIVATE -Wall -Wextra -Werror)
    endif()
    add_executable(goose_mesh_test native/mesh_test.c)
    target_link_libraries(goose_mesh_test PRIVATE goose_mesh m)
    add_test(NAME native_mesh COMMAND goose_mesh_test)
    add_executable(goose_backend_smoke native/backend_smoke.c)
    target_link_libraries(goose_backend_smoke PRIVATE goose_platform)
    if(GOOSE_TEST_GRAPHICS)
        add_test(NAME native_graphics COMMAND goose_backend_smoke)
    endif()
endif()
find_program(GOOSE_EXECUTABLE NAMES goose)
if(GOOSE_EXECUTABLE)
    find_package(ZLIB REQUIRED)
    add_executable(goose_compression_test native/compression_test.c native/compression.c)
    target_link_libraries(goose_compression_test PRIVATE ZLIB::ZLIB)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(goose_compression_test PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME native_compression COMMAND goose_compression_test)
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xylar-goose-port"
        COMMAND "${GOOSE_EXECUTABLE}" build -o "${CMAKE_CURRENT_BINARY_DIR}/xylar-goose-port"
            "${CMAKE_CURRENT_SOURCE_DIR}/port_demo.goose"
        DEPENDS port_demo.goose core/storage.goose core/text.goose core/seed_hash.goose
        VERBATIM)
    add_custom_target(goose_port_demo ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/xylar-goose-port")
    # Build native test executables: TinyCC's runner cannot use this Gentoo
    # multilib installation's /usr/lib 32-bit libc linker scripts.
    file(GLOB goose_core_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/core/*.goose")
    file(GLOB goose_net_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/net/*.goose")
    file(GLOB goose_data_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/*.goose")
    set(goose_test_suites protocol storage text seed_hash paletted position wire chunk_wire chunk_data packets frame login jmath random nbt configuration player_sync column)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(OpenSSL REQUIRED COMPONENTS Crypto)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        find_program(OPENSSL_EXECUTABLE NAMES openssl REQUIRED)
        set(goose_network_dependencies native/jmath.c native/network.c native/network.h native/crypto.c native/crypto.h
            native/compression.c native/compression.h)
        list(APPEND goose_test_suites crypto connection)
        add_executable(goose_native_crypto_test native/crypto_test.c native/crypto.c)
        target_link_libraries(goose_native_crypto_test PRIVATE OpenSSL::Crypto)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(goose_native_crypto_test PRIVATE -Wall -Wextra -Werror)
        endif()
        add_test(NAME native_crypto COMMAND goose_native_crypto_test)
        set(network_binary "${CMAKE_CURRENT_BINARY_DIR}/xylar-goose-network")
        add_custom_command(OUTPUT "${network_binary}"
            COMMAND "${GOOSE_EXECUTABLE}" build -o "${network_binary}" "${CMAKE_CURRENT_SOURCE_DIR}/network.goose"
            DEPENDS network.goose network.build ${goose_core_sources} ${goose_net_sources} ${goose_data_sources} ${goose_network_dependencies}
            VERBATIM)
        set(goose_network_binaries "${network_binary}")
        set(previous_probe)
        foreach(mode debug release)
            set(probe_binary "${CMAKE_CURRENT_BINARY_DIR}/goose_session_probe_${mode}")
            set(probe_flags)
            if(mode STREQUAL "debug")
                list(APPEND probe_flags --debug)
            endif()
            add_custom_command(OUTPUT "${probe_binary}"
                COMMAND "${GOOSE_EXECUTABLE}" build ${probe_flags} -o "${probe_binary}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/session_probe.goose"
                DEPENDS session_probe.goose session_probe.build tools/pipe_transport.c ${goose_core_sources} ${goose_net_sources} ${goose_data_sources}
                    ${goose_network_dependencies} ${previous_probe}
                VERBATIM)
            list(APPEND goose_network_binaries "${probe_binary}")
            add_test(NAME goose_session_${mode}
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/session_integration.py"
                    --probe "${probe_binary}" --network "${network_binary}" --openssl "${OPENSSL_EXECUTABLE}"
                    --report "${CMAKE_CURRENT_BINARY_DIR}/session-${mode}-report.json")
            set_tests_properties(goose_session_${mode} PROPERTIES TIMEOUT 90)
            # Goose emits both modes of one entry point under the same cache
            # path. Serialize these builds so their generated C cannot race.
            set(previous_probe "${probe_binary}")
        endforeach()
        add_custom_target(goose_network ALL DEPENDS ${goose_network_binaries})
    endif()
    if(GOOSE_BUILD_SANDBOX)
        list(APPEND goose_test_suites engine)
    endif()
    set(goose_test_binaries)
    foreach(suite IN LISTS goose_test_suites)
        set(test_binary "${CMAKE_CURRENT_BINARY_DIR}/goose_${suite}_test")
        set(suite_dependencies)
        if(suite STREQUAL "frame" OR suite STREQUAL "login")
            list(APPEND suite_dependencies "${suite}_test.build" native/compression.c native/compression.h)
        endif()
        if(suite STREQUAL "jmath" OR suite STREQUAL "random" OR suite STREQUAL "player_sync" OR suite STREQUAL "column")
            list(APPEND suite_dependencies "${suite}_test.build" native/jmath.c)
        endif()
        if(suite STREQUAL "crypto" OR suite STREQUAL "connection")
            list(APPEND suite_dependencies "${suite}_test.build" ${goose_network_dependencies})
        endif()
        add_custom_command(OUTPUT "${test_binary}"
            COMMAND "${GOOSE_EXECUTABLE}" build --debug -o "${test_binary}"
                "${CMAKE_CURRENT_SOURCE_DIR}/${suite}_test.goose"
            DEPENDS "${suite}_test.goose" ${goose_core_sources} ${goose_net_sources} ${goose_data_sources} ${suite_dependencies} protocol.goose world.goose player.goose
            VERBATIM)
        list(APPEND goose_test_binaries "${test_binary}")
        add_test(NAME goose_${suite} COMMAND "${test_binary}")
    endforeach()
    add_custom_target(goose_test_programs ALL DEPENDS ${goose_test_binaries})
    if(GOOSE_TEST_PORT_PARITY)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        find_program(RUSTC_EXECUTABLE NAMES rustc REQUIRED)
        add_test(NAME goose_port_parity
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_port.py"
                --goose "${GOOSE_EXECUTABLE}" --rustc "${RUSTC_EXECUTABLE}"
                --build-dir "${CMAKE_CURRENT_BINARY_DIR}/port")
        set_tests_properties(goose_port_parity PROPERTIES TIMEOUT 120)
    endif()
    if(GOOSE_BUILD_SANDBOX)
        add_custom_target(goose_client ALL
            COMMAND "${GOOSE_EXECUTABLE}" build -o "${CMAKE_CURRENT_BINARY_DIR}/xylar-goose" "${CMAKE_CURRENT_SOURCE_DIR}/client.goose"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            VERBATIM)
        if(GOOSE_TEST_GRAPHICS)
            file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/client-preview")
            add_test(NAME goose_client_preview
                COMMAND "${CMAKE_CURRENT_BINARY_DIR}/xylar-goose" --seed 42 --frames 60)
            set_tests_properties(goose_client_preview PROPERTIES
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/client-preview" TIMEOUT 30)
        endif()
    endif()
else()
    if(GOOSE_TEST_PORT_PARITY)
        message(FATAL_ERROR "GOOSE_TEST_PORT_PARITY requires the aardappel Goose compiler")
    endif()
    message(STATUS "Goose compiler absent: no translated executable will be produced")
endif()
