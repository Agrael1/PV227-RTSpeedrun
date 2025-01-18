# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("PV227-RTSpeedrun")

#set(WISDOM_FORCE_VULKAN ON CACHE BOOL "Force Vulkan as the rendering API")
include(cmake/deps.cmake)
include(assets/copy_assets.cmake)

set(CPM_SOURCE_CACHE "${CMAKE_BINARY_DIR}/_deps" CACHE STRING "Where to cache CPM dependencies")


set(HEADERS  "src/app.hpp" 
"src/sdl.hpp"
"src/consts.hpp"
"src/graphics.hpp"
"src/scene.hpp"
"src/model_loader.hpp"
"src/texture.hpp"
"src/model.hpp"
"src/camera.hpp"
 "src/stb.h")
set(SOURCES "src/entry_main.cpp" "src/sdl.cpp" "src/app.cpp" "src/graphics.cpp" "src/model_loader.cpp" "src/scene.cpp" "src/model.cpp" "src/texture.cpp")

add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})

target_link_libraries(${PROJECT_NAME} 
	PUBLIC 
	wis::wisdom 
	wis::raytracing
	wis::platform
	wis::debug
	assimp::assimp
	SDL3::SDL3
)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)
target_include_directories(${PROJECT_NAME} PUBLIC ${fpng_SOURCE_DIR}/src)

# Copy the dlls to the build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	$<TARGET_FILE:SDL3::SDL3>
	$<TARGET_FILE_DIR:${PROJECT_NAME}>
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	$<TARGET_FILE:assimp::assimp>
	$<TARGET_FILE_DIR:${PROJECT_NAME}>
)

WIS_INSTALL_DEPS(${PROJECT_NAME})

add_dependencies(${PROJECT_NAME} copy_assets)


set(SHADER_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/shaders
    CACHE PATH "")

# create folder for shaders
file(MAKE_DIRECTORY ${SHADER_DIR})

add_custom_target(shaders)

wis_compile_shader(TARGET shaders
SHADER "shaders/raytracing.lib.hlsl"
OUTPUT "${SHADER_DIR}/raytracing.lib"
TYPE "lib"
SHADER_MODEL "6.3"
)

add_dependencies(${PROJECT_NAME} shaders)