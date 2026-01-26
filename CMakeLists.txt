cmake_minimum_required(VERSION 3.20)
project(VulkanApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Fetch GLFW
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Main executable
add_executable(vulkan_app
    src/main.cpp
)

target_include_directories(vulkan_app PRIVATE
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(vulkan_app PRIVATE
    Vulkan::Vulkan
    glfw
)

# Enable warnings
if(MSVC)
    target_compile_options(vulkan_app PRIVATE /W4)
else()
    target_compile_options(vulkan_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Compile shaders
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders"
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
        COMMENT "Compiling ${FILE_NAME}"
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()

add_custom_target(
    shaders
    DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(vulkan_app shaders)
