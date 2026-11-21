# Poneglyph Engine: 32-bit Direct3D 9 proxy. Output file name must be d3d9.dll.
file(TO_CMAKE_PATH "$ENV{SystemRoot}/SysWOW64/d3d9.dll" PG_SYSTEM_D3D9)
set(PG_GENERATED_DEF "${CMAKE_CURRENT_BINARY_DIR}/d3d9_generated.def")
add_custom_command(
  OUTPUT "${PG_GENERATED_DEF}"
  COMMAND pg_tool gendef "${PG_SYSTEM_D3D9}" "${PG_GENERATED_DEF}" "${CMAKE_CURRENT_SOURCE_DIR}/PoneglyphEngine.def.fallback"
  DEPENDS pg_tool "${CMAKE_CURRENT_SOURCE_DIR}/PoneglyphEngine.def.fallback"
  COMMENT "Generating export list (names + ordinals) from the genuine system d3d9.dll")
add_library(PoneglyphEngine SHARED dllmain.cpp proxy.cpp runtime.cpp hooks.cpp PoneglyphEngine.rc "${PG_GENERATED_DEF}")
set_target_properties(PoneglyphEngine PROPERTIES OUTPUT_NAME "d3d9" PREFIX "")
target_include_directories(PoneglyphEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/Source/PoneglyphSDK)
target_link_libraries(PoneglyphEngine PRIVATE PoneglyphCore kernel32 user32)
target_compile_definitions(PoneglyphEngine PRIVATE _CRT_SECURE_NO_WARNINGS)
if(MSVC)
  target_compile_options(PoneglyphEngine PRIVATE /W4 /utf-8 /EHsc /GS /guard:cf /O2)
  target_link_options(PoneglyphEngine PRIVATE /DYNAMICBASE /NXCOMPAT /DEBUG:NONE)
endif()
# Stage B/C proof: export table must match the system DLL.
add_custom_command(TARGET PoneglyphEngine POST_BUILD
  COMMAND pg_tool verify "$<TARGET_FILE:PoneglyphEngine>" "${PG_SYSTEM_D3D9}"
  COMMENT "Verifying proxy exports/ordinals against the system d3d9.dll")
