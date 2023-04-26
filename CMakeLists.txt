# Copyright MediaZ AS. All Rights Reserved.

list(APPEND DEPENDENCIES mzCommon vkl spirv-cross-core d3d12 d3d11 dxgi dynalo)
list(APPEND INCLUDE_FOLDERS ${CMAKE_CURRENT_SOURCE_DIR}/Include ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_folder_as_project(${CMAKE_CURRENT_SOURCE_DIR} mzVulkan SHARED "${DEPENDENCIES}" "${INCLUDE_FOLDERS}")

if (${MZ_DEV_BUILD})
    list(APPEND TEST_DEPENDENCIES mzVulkan)
    list(APPEND TEST_SOURCE_FOLDERS "${CMAKE_CURRENT_SOURCE_DIR}/Tests")
    list(APPEND TEST_INCLUDE_FOLDERS "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    add_folder_as_project("${TEST_SOURCE_FOLDERS}" mzVulkan_Tests EXE "${TEST_DEPENDENCIES}" "${TEST_INCLUDE_FOLDERS}")
endif()