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

# JSON library - glaze (header-only)
find_package(glaze REQUIRED)
message(STATUS "Glaze found - using Glaze for JSON parsing")

# HTTP client configuration
# On Windows, use WinHTTP (built-in). On other platforms, use CURL.
if(WIN32)
  message(STATUS "Windows detected - using WinHTTP for HTTP client")
  set(USE_WINHTTP ON)
else()
  find_package(CURL REQUIRED)
  message(STATUS "CURL found - using CURL for HTTP client")
  set(USE_CURL ON)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/ui/frame.cpp
    src/ui/editor.cpp
    src/ui/terminal.cpp
    src/ui/widget_bar.cpp
    src/ui/widget_activity_bar.cpp
    src/commands/command_palette.cpp
    src/config/config.cpp
    src/theme/theme.cpp
    src/http/http_client.cpp
    src/fs/fs.cpp
)

set(HEADERS
    src/ui/frame.h
    src/ui/editor.h
    src/ui/terminal.h
    src/ui/widget.h
    src/ui/widget_bar.h
    src/ui/widget_activity_bar.h
    src/ui/builtin_widgets.h
    src/commands/command.h
    src/commands/command_registry.h
    src/commands/command_palette.h
    src/commands/builtin_commands.h
    src/config/config.h
    src/theme/theme.h
    src/http/http_client.h
    src/fs/fs.h
)

# Create executable
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

# Link wxWidgets libraries
target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES})

# Link JSON library
target_link_libraries(${PROJECT_NAME} glaze::glaze)

# Link HTTP backend
if(USE_CURL)
  target_link_libraries(${PROJECT_NAME} CURL::libcurl)
endif()

if(USE_WINHTTP)
  target_link_libraries(${PROJECT_NAME} winhttp)
endif()

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

# ============================================================================
# Testing with Google Test
# ============================================================================
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
  enable_testing()
  find_package(GTest REQUIRED)
  include(GoogleTest)

  # Test sources - add test files here
  set(TEST_SOURCES
      tests/test_main.cpp
      tests/test_config.cpp
  )

  # Sources to test (excluding main.cpp)
  set(TESTABLE_SOURCES
      src/config/config.cpp
      src/theme/theme.cpp
      src/fs/fs.cpp
  )

  add_executable(bytemusehq_tests ${TEST_SOURCES} ${TESTABLE_SOURCES})

  target_include_directories(bytemusehq_tests PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(bytemusehq_tests
      GTest::gtest
      GTest::gtest_main
      glaze::glaze
      ${wxWidgets_LIBRARIES}
  )

  # Platform-specific linking for tests
  if(USE_CURL)
    target_link_libraries(bytemusehq_tests CURL::libcurl)
  endif()

  gtest_discover_tests(bytemusehq_tests)
endif()

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
