# Copyright MediaZ AS. All Rights Reserved.
cmake_minimum_required(VERSION 3.24)
project("mzVulkan")

set(CMAKE_CXX_STANDARD 20)

# Dependencies
# ------------
set(EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External)

# SPIRV Cross
message("Processing SPIRV-Cross")
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "Enable SPIRV-Cross tests." FORCE)
set(SPIRV_CROSS_CLI OFF CACHE BOOL "Build the CLI binary. Requires SPIRV_CROSS_STATIC." FORCE)
set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "Enable C API wrapper support in static library." FORCE)
set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "Enable util module support." FORCE)
add_subdirectory(${EXTERNAL_DIR}/SPIRV-Cross)

# vkl
message("Processing vkl")
if(WIN32)
    add_compile_definitions(VK_USE_PLATFORM_WIN32_KHR)
endif()
add_subdirectory(${EXTERNAL_DIR}/vkl)

# vma
message("Processing VMA")
set(VMA_STATIC_VULKAN_FUNCTIONS OFF CACHE BOOL "Link statically with Vulkan API" FORCE)
set(VMA_DYNAMIC_VULKAN_FUNCTIONS ON CACHE BOOL "Fetch pointers to Vulkan functions internally (no static linking)" FORCE)
add_subdirectory(${EXTERNAL_DIR}/vma)

list(APPEND DEPENDENCIES vkl VulkanMemoryAllocator spirv-cross-core d3d12 d3d11 dxgi)
list(APPEND INCLUDE_FOLDERS ${CMAKE_CURRENT_SOURCE_DIR}/Include)
list(APPEND SOURCE_FOLDERS ${CMAKE_CURRENT_SOURCE_DIR}/Source)

foreach(folder IN LISTS SOURCE_FOLDERS)	
    message(STATUS "${PROJECT_NAME}: Scanning ${folder}")
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${folder} "${folder}/*.cpp" 
        "${folder}/*.cc" "${folder}/*.c" "${folder}/*.inl"
        "${folder}/*.frag" "${folder}/*.vert" "${folder}/*.glsl" "${folder}/*.comp" "${folder}/*.natvis")
    file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS ${folder} "${folder}/*.h" "${folder}/*.hpp")
    file(GLOB_RECURSE RESOURCES CONFIGURE_DEPENDS ${folder} "${folder}/*.rc")
    list(APPEND COLLECTED_SOURCES ${SOURCES})
    list(APPEND COLLECTED_HEADERS ${HEADERS})
    list(APPEND COLLECTED_RESOURCES ${RESOURCES})
endforeach()

add_library(${PROJECT_NAME} STATIC ${COLLECTED_SOURCES} ${COLLECTED_HEADERS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${DEPENDENCIES})
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_FOLDERS})
