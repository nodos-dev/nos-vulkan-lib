list(APPEND DEPENDENCIES vkl spirv-cross-core stb mzCommon)
list(APPEND INCLUDEFOLDERS ${CMAKE_CURRENT_SOURCE_DIR} ${DYNALO_INCLUDE_DIR})

execute_process(COMMAND ${SHADER_COMPILER_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/GlobVS.vert -c -o ${MZ_SHADER_PATH}/GlobVS.vert.spv)

add_folder_as_project(mzVulkan STATIC "${DEPENDENCIES}" "${INCLUDEFOLDERS}")
