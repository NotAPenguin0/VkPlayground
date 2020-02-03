cmake_minimum_required(VERSION 3.12)

project(VkPlayground)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)

set(Vulkan_LIBRARY CACHE STRING "Vulkan library path")
set(Vulkan_INCLUDE_DIR CACHE STRING "Vulkan include directory")

set(VK_PLAYGROUND_SOURCES "")
set(VK_PLAYGROUND_INCLUDE_DIRS "")
set(VK_PLAYGROUND_LINK_LIBRARIES "")

add_subdirectory("src")
add_subdirectory("external")

add_executable(${PROJECT_NAME} ${VK_PLAYGROUND_SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC ${VK_PLAYGROUND_INCLUDE_DIRS} ${Vulkan_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC ${VK_PLAYGROUND_LINK_LIBRARIES} ${Vulkan_LIBRARY})