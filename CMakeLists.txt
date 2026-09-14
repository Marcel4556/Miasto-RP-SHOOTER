cmake_minimum_required(VERSION 3.20)
project(VulkanEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------- Pakiety ----------
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

# ---------- ImGui (vendored) ----------
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/external/imgui")

if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
    message(FATAL_ERROR
        "Brak ImGui w ${IMGUI_DIR}!\n"
        "Sklonuj: git clone https://github.com/ocornut/imgui.git external/imgui")
endif()

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

# ---------- Zrodla ----------
add_executable(engine
    src/main.cpp
    src/core/Window.cpp
    src/core/Input.cpp
    src/core/Telemetry.cpp
    src/renderer/VulkanContext.cpp
    src/renderer/Renderer.cpp
    src/renderer/Mesh.cpp
    src/renderer/Texture.cpp
    src/renderer/Cubemap.cpp
    src/game/Scene.cpp
    src/ui/Menu.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(engine PRIVATE src)
target_include_directories(engine PRIVATE ${IMGUI_DIR})
target_include_directories(engine PRIVATE ${IMGUI_DIR}/backends)

target_compile_definitions(engine PRIVATE
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    VK_NO_PROTOTYPES
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

target_link_libraries(engine PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    GPUOpen::VulkanMemoryAllocator
)

# Windows-specific dla telemetrii
if(WIN32)
    target_link_libraries(engine PRIVATE dxgi iphlpapi)
endif()

add_custom_command(TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:engine>/shaders)