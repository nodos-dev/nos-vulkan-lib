list(APPEND DEPENDENCIES vkl spirv-cross-core shaderc mzCommon)
list(APPEND INCLUDEFOLDERS ${CMAKE_CURRENT_SOURCE_DIR} ${DYNALO_INCLUDE_DIR})

add_folder_as_project(mzVulkan STATIC "${DEPENDENCIES}" "${INCLUDEFOLDERS}")
