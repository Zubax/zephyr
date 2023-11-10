# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)


get_filename_component(APPLICATION_PROJECT_DIR
        ${CMAKE_CURRENT_LIST_DIR}/../../../../../epm
        ABSOLUTE
        )

get_filename_component(ONTARGET_DIR
        ${CMAKE_CURRENT_LIST_DIR}/../../../../../verification/on_target
        ABSOLUTE
        )

set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/dtc_configurations/epm_CAN_for_validation.overlay)

# Add this project to list of board roots
list(APPEND BOARD_ROOT ${APPLICATION_PROJECT_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(integration)

target_sources(app PRIVATE src/common.c)
target_sources(app PRIVATE src/classic.c)
target_sources(app PRIVATE src/utilities.c)
target_sources(app PRIVATE src/semihost_extra.c)
zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR}/src)
target_sources_ifdef(CONFIG_CAN_FD_MODE app PRIVATE src/canfd.c)
