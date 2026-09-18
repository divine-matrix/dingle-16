cmake_minimum_required(VERSION 3.15)
project(DingleApp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Silence the macOS OpenGL deprecation warnings completely
add_compile_definitions(GL_SILENCE_DEPRECATION)

# Find dependencies via vcpkg
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

# Include all your C++ source files so symbols like runProgram are found
add_executable(my_app
    src/main.cpp
    src/assembler.cpp
    src/emulator.cpp
    src/FuzzyMatcher.cpp
)

# Link ImGui, GLFW, and your local FuzzyMatcher library
target_link_libraries(my_app PRIVATE
    imgui::imgui
    glfw
)

# Explicitly link Native macOS OpenGL Frameworks
if(APPLE)
  find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
  find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
  find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
  find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
  target_link_libraries(my_app PRIVATE
        ${OPENGL_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
    )
endif()

