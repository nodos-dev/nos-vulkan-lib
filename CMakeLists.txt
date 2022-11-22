list(APPEND DEPENDENCIES mzCommon vkl spirv-cross-core d3d12 d3d11 dxgi dynalo mzUtil)
list(APPEND INCLUDEFOLDERS ${CMAKE_CURRENT_SOURCE_DIR}/Include ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_folder_as_project(${CMAKE_CURRENT_SOURCE_DIR} mzVulkan SHARED "${DEPENDENCIES}" "${INCLUDEFOLDERS}")

# add_library(mzVulkanCore SHARED 
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/Allocator.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/Binding.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/Buffer.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/Command.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/Device.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/DXGIFormatMapping.c"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/Image.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/Source/mzVulkan/Common.cpp"
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
