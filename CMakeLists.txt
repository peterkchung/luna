cmake_minimum_required(VERSION 3.20)
project(luna3d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Dependencies ---
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# --- Shader compilation ---
find_program(GLSLANG_VALIDATOR glslangValidator HINTS $ENV{VULKAN_SDK}/bin)
if(NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found — install the Vulkan SDK")
endif()

file(GLOB SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/shaders/*.vert
    ${CMAKE_SOURCE_DIR}/shaders/*.frag)

set(SHADER_SPIRV_FILES "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPIRV_OUTPUT ${CMAKE_SOURCE_DIR}/shaders/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPIRV_OUTPUT}
        COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER} -o ${SPIRV_OUTPUT}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${SHADER_NAME}")
    list(APPEND SHADER_SPIRV_FILES ${SPIRV_OUTPUT})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADER_SPIRV_FILES})

# --- Utility library ---
add_library(luna_util STATIC
    src/util/Log.cpp
    src/util/FileIO.cpp)
target_include_directories(luna_util PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(luna_util PUBLIC glm::glm)

# --- Core library ---
add_library(luna_core STATIC
    src/core/VulkanContext.cpp
    src/core/Swapchain.cpp
    src/core/RenderPass.cpp
    src/core/Image.cpp
    src/core/CommandPool.cpp
    src/core/Sync.cpp
    src/core/ShaderModule.cpp
    src/core/Pipeline.cpp
    src/core/Buffer.cpp)
target_include_directories(luna_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(luna_core PUBLIC luna_util Vulkan::Vulkan glfw)

# --- Input library ---
add_library(luna_input STATIC
    src/input/InputManager.cpp)
target_include_directories(luna_input PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(luna_input PUBLIC luna_util glfw)

# --- Camera library ---
add_library(luna_camera STATIC
    src/camera/Camera.cpp
    src/camera/CameraController.cpp)
target_include_directories(luna_camera PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(luna_camera PUBLIC luna_util)

# --- Simulation library (no Vulkan dependency) ---
add_library(luna_sim STATIC
    src/sim/TerrainQuery.cpp
    src/sim/Heightmap.cpp
    src/sim/Physics.cpp)
target_include_directories(luna_sim PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(luna_sim PUBLIC luna_util)

# --- Scene library ---
add_library(luna_scene STATIC
    src/scene/Mesh.cpp
    src/scene/Terrain.cpp
    src/scene/ChunkGenerator.cpp
    src/scene/CubesphereBody.cpp
    src/scene/Starfield.cpp)
target_include_directories(luna_scene PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(luna_scene PUBLIC luna_core luna_sim luna_util)

# --- HUD library ---
add_library(luna_hud STATIC
    src/hud/Hud.cpp)
target_include_directories(luna_hud PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(luna_hud PUBLIC luna_core luna_scene luna_sim luna_util)

# --- Executable ---
add_executable(luna3d src/main.cpp)
target_link_libraries(luna3d PRIVATE
    luna_core luna_scene luna_hud luna_sim luna_input luna_camera luna_util
    Vulkan::Vulkan glfw)
add_dependencies(luna3d shaders)
