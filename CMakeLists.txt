cmake_minimum_required(VERSION 3.15)

# `src/glad.c` is C, the rest is C++
project(MoldLab3D LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(MoldLab3D)

target_sources(MoldLab3D
  PRIVATE
    src/main.cpp
    src/GameEngine.cpp
    src/MoldLabGame.cpp
    src/InputManager.cpp
    src/ShaderVariable.cpp
    src/glad.c

    # ImGui sources (vendored)
    src/imgui/imgui.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_widgets.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/imgui_impl_glfw.cpp
    src/imgui/imgui_impl_opengl3.cpp
)

target_include_directories(MoldLab3D
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/imgui
)

# ----------------------------
# Dependencies
# ----------------------------
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

include(FetchContent)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

target_link_libraries(MoldLab3D
  PRIVATE
    glfw
    OpenGL::GL
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

# ----------------------------
# Runtime assets (so running from build/ works)
# ----------------------------
add_custom_command(TARGET MoldLab3D POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/shaders
          $<TARGET_FILE_DIR:MoldLab3D>/shaders
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/Fonts
          $<TARGET_FILE_DIR:MoldLab3D>/Fonts
  COMMENT "Copying runtime assets (shaders, Fonts) next to MoldLab3D"
)

# ----------------------------
# Install support (optional but handy)
# ----------------------------
install(TARGETS MoldLab3D RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/shaders DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/Fonts   DESTINATION bin)
