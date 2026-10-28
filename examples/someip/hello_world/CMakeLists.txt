# SPDX-FileCopyrightText: Copyright 2026 LG Electronics Inc.
# SPDX-License-Identifier: MIT

# Find vsomeip3 package (optional)
find_package(vsomeip3 QUIET)

if(vsomeip3_FOUND)
    message(STATUS "vsomeip3 found. Building real COVESA hello_world example.")

    # Only timpani_rt.h comes from here now; the mock vsomeip/vsomeip.hpp
    # stub lives under mock_include/ (used by radar_based_fcw instead),
    # so it can no longer shadow the real vsomeip3 headers below.
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

    # hello_world_service
    add_executable(vsomeip_hello_world_service
        hello_world_service_main.cpp
    )
    target_link_libraries(vsomeip_hello_world_service vsomeip3 timpani_rt pthread)

    # hello_world_client
    add_executable(vsomeip_hello_world_client
        hello_world_client_main.cpp
    )
    target_link_libraries(vsomeip_hello_world_client vsomeip3 timpani_rt pthread)
else()
    message(WARNING "vsomeip3 not found. Skipping real COVESA hello_world example targets.")
endif()

