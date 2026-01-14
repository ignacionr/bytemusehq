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
)

set(HEADERS
    src/ui/frame.h
)

# Create executable
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

# Set compile definitions for static linking
if(WIN32)
  target_compile_definitions(${PROJECT_NAME} PRIVATE WXUSINGDLL=0)
endif()

# Link wxWidgets libraries
target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES})

# When using static wxWidgets, link required Windows system libraries
if(WIN32 AND NOT SHARED)
  target_link_libraries(${PROJECT_NAME}
    advapi32 comctl32 comdlg32 gdi32 gdiplus imm32 kernel32 msimg32
    ole32 oleacc oleaut32 opengl32 rpcrt4 shell32 shlwapi user32 uuid
    uxtheme version wininet winmm winspool ws2_32 wxlexilla wxscintilla
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
