# Copyright MediaZ AS. All Rights Reserved.
cmake_minimum_required(VERSION 3.24)
project("nosVulkan")

set(CMAKE_CXX_STANDARD 20)

# Dependencies
# ------------
set(EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External)

# SPIRV Cross

if(NOT TARGET "spirv-cross-core")
	message("Processing SPIRV-Cross")
	set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "Enable SPIRV-Cross tests." FORCE)
	set(SPIRV_CROSS_CLI OFF CACHE BOOL "Build the CLI binary. Requires SPIRV_CROSS_STATIC." FORCE)
	set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "Enable C API wrapper support in static library." FORCE)
	set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "Enable util module support." FORCE)
	add_subdirectory(${EXTERNAL_DIR}/SPIRV-Cross)
endif()

# vkl
if(NOT TARGET "vkl")
	message("Processing vkl")
	add_subdirectory(${EXTERNAL_DIR}/vkl)
	if(WIN32)
	    target_compile_definitions(vkl PUBLIC VK_USE_PLATFORM_WIN32_KHR)
	endif()
endif()

# vma
if(NOT TARGET "VulkanMemoryAllocator")
	message("Processing VMA")
	set(VMA_STATIC_VULKAN_FUNCTIONS OFF CACHE BOOL "Link statically with Vulkan API" FORCE)
	set(VMA_DYNAMIC_VULKAN_FUNCTIONS ON CACHE BOOL "Fetch pointers to Vulkan functions internally (no static linking)" FORCE)
	add_subdirectory(${EXTERNAL_DIR}/vma)
endif()

list(APPEND DEPENDENCIES vkl VulkanMemoryAllocator spirv-cross-core d3d12 d3d11 dxgi)
list(APPEND INCLUDE_FOLDERS ${CMAKE_CURRENT_SOURCE_DIR}/Include)
list(APPEND SOURCE_FOLDERS ${CMAKE_CURRENT_SOURCE_DIR}/Source ${INCLUDE_FOLDERS})

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

if(NOT TARGET ${PROJECT_NAME})
	add_library(${PROJECT_NAME} STATIC ${COLLECTED_SOURCES} ${COLLECTED_HEADERS})
	target_link_libraries(${PROJECT_NAME} PUBLIC ${DEPENDENCIES})
	target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_FOLDERS})
endif()

# TODO: Use these also in plugins instead of python script
set(TOOLS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Tools/${CMAKE_SYSTEM_NAME})
file(GLOB_RECURSE SHADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.frag" "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.vert")
message("Shaders: ${SHADERS}")
foreach(SHADER ${SHADERS})
	get_filename_component(SHADER_DIR ${SHADER} DIRECTORY)
	set(SHADER_SPV "${SHADER}.unopt.spv")
	set(SHADER_OPT_SPV "${SHADER}.spv")
	# Rename *_gen_opt.spv.h toQ gen.spv.dat
	add_custom_command(
		TARGET ${PROJECT_NAME} PRE_BUILD
		COMMAND "${TOOLS_DIR}/glslc" "-O" "-g" "-o" "${SHADER_SPV}" "${SHADER}"
		COMMAND "${TOOLS_DIR}/spirv-opt" "-O" "-o" "${SHADER_OPT_SPV}" "${SHADER_SPV}"
		COMMAND "${TOOLS_DIR}/bin2header" "${SHADER_OPT_SPV}"
		COMMAND ${CMAKE_COMMAND} -E rename "${SHADER_OPT_SPV}.h" "${SHADER_OPT_SPV}.dat"
		# Delete *.spv
		COMMAND ${CMAKE_COMMAND} -E remove "${SHADER_SPV}"
		COMMAND ${CMAKE_COMMAND} -E remove "${SHADER_OPT_SPV}"
		DEPENDS ${SHADER})
endforeach()
