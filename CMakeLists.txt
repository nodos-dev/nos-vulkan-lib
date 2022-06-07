list(APPEND DEPENDENCIES mzCommon vkl spirv-cross-core d3d12 dxgi)
list(APPEND INCLUDEFOLDERS ${CMAKE_CURRENT_SOURCE_DIR}/Include ${DYNALO_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)

execute_process(COMMAND ${SHADER_COMPILER_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/GlobVS.vert -c -o ${MZ_SHADER_PATH}/GlobVS.vert.spv)
add_folder_as_project(mzVulkan SHARED "${DEPENDENCIES}" "${INCLUDEFOLDERS}")

# execute_process(COMMAND ${SHADER_COMPILER_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/GlobVS.vert -c -o ${MZ_SHADER_PATH}/GlobVS.vert.spv)

# add_library(mzVulkanCore SHARED 
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/Allocator.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/Binding.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/Buffer.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/Command.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/Device.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/DXGIFormatMapping.c"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/Image.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/mzVkCommon.cpp"
# )

# add_library(mzVulkan SHARED 
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/DynamicPipeline.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/DynamicPipeline1.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/Layout.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/Shader.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/NativeAPID3D12.cpp"
# )

# target_link_libraries(mzVulkanCore PUBLIC mzCommon vkl d3d12 dxgi)
# target_link_libraries(mzVulkan PUBLIC mzVulkanCore spirv-cross-core)

# target_include_directories(mzVulkanCore 
#   PRIVATE 
#     <$BUILD_INTERFACE:${DYNALO_INCLUDE_DIR}>
#   PUBLIC
#     ${CMAKE_CURRENT_SOURCE_DIR}/Include
# )
