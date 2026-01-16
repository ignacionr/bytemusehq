cmake_minimum_required(VERSION 3.10)
project(bytemusehq)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force static linking on Windows
if(WIN32)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Find wxWidgets with static linking
set(wxWidgets_USE_STATIC ON)
find_package(wxWidgets REQUIRED core base stc)
include(${wxWidgets_USE_FILE})

# Source files
set(SOURCES
    src/main.cpp
    src/ui/frame.cpp
    src/ui/editor.cpp
    src/commands/command_palette.cpp
)

set(HEADERS
    src/ui/frame.h
    src/ui/editor.h
    src/commands/command.h
    src/commands/command_registry.h
    src/commands/command_palette.h
    src/commands/builtin_commands.h
)

# Create executable
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

# Link wxWidgets libraries
target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES})

# Set static linking options for Windows
if(WIN32 AND MSVC)
  # Use static runtime library
  set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  # Link statically with Windows system libraries
  target_link_libraries(${PROJECT_NAME}
    advapi32 comctl32 comdlg32 gdi32 gdiplus imm32 kernel32 msimg32
    ole32 oleacc oleaut32 opengl32 rpcrt4 shell32 shlwapi user32 uuid
    uxtheme version wininet winmm winspool ws2_32
  )
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Install rules
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# CPack configuration for Windows installer
if(WIN32)
  set(CPACK_GENERATOR NSIS)
  set(CPACK_PACKAGE_NAME "ByteMuse HQ")
  set(CPACK_PACKAGE_VENDOR "ByteMuse")
  set(CPACK_PACKAGE_VERSION "1.0.0")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "wxWidgets Text Editor")
  set(CPACK_NSIS_INSTALL_ROOT "C:/Program Files")

  include(CPack)
endif()
