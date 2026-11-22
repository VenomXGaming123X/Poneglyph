add_library(PoneglyphCore STATIC
  src/sha256.cpp src/util.cpp src/ini.cpp src/log.cpp src/pe.cpp src/zip.cpp src/games.cpp
  src/addons.cpp src/stats.cpp src/ops.cpp src/report.cpp src/launchopts.cpp)
target_include_directories(PoneglyphCore PUBLIC include)
target_compile_features(PoneglyphCore PUBLIC cxx_std_17)
if(MSVC)
  target_compile_options(PoneglyphCore PRIVATE /W4 /permissive- /utf-8)
  target_compile_definitions(PoneglyphCore PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
  target_compile_options(PoneglyphCore PRIVATE -Wall -Wextra -Wno-misleading-indentation)
endif()
