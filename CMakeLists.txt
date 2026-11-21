cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0091 NEW)
project(Poneglyph VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT: the engine is injected into games and must not need a Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
enable_testing()
add_subdirectory(Source/PoneglyphCore)
add_subdirectory(Source/PoneglyphTools)
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
  add_subdirectory(Source/PoneglyphEngine)          # x86 only: the games are 32-bit
endif()
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_subdirectory(Source/PoneglyphManager)         # x64 GUI
endif()
add_subdirectory(Source/PoneglyphTests)
