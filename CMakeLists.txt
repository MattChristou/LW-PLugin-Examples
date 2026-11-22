cmake_minimum_required(VERSION 3.20)
project(LWPluginExamples C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# The unmodified LightWave 2025 SDK headers are vendored so CI can build
# without a LightWave install. Point LWSDK at your own SDK to use that instead.
set(LWSDK "${CMAKE_SOURCE_DIR}/third_party/lwsdk2025.0" CACHE PATH "LightWave SDK root")

# lw_add_plugin(<name> <sources...>): one .p (Windows) or .plugin (macOS).
function(lw_add_plugin name)
  add_library(${name} MODULE ${ARGN})
  target_include_directories(${name} PRIVATE "${LWSDK}/include")
  if(WIN32)
    target_compile_definitions(${name} PRIVATE _MSWIN _CRT_SECURE_NO_WARNINGS)
    set_target_properties(${name} PROPERTIES
        PREFIX "" SUFFIX ".p"
        MSVC_RUNTIME_LIBRARY "MultiThreaded")          # /MT: no redistributable
  elseif(APPLE)
    target_compile_definitions(${name} PRIVATE _MACOS)
    target_compile_options(${name} PRIVATE -Wno-visibility)
    set_target_properties(${name} PROPERTIES
        BUNDLE TRUE BUNDLE_EXTENSION "plugin"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.lwex.${name}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/Info.plist.in")
    target_link_options(${name} PRIVATE "LINKER:-exported_symbol,__mod_descrip")
  endif()
endfunction()

lw_add_plugin(HelloC       c/hello/hello.c)
lw_add_plugin(Lift         c/lift/lift.c)
lw_add_plugin(LiftVParm    c/lift_vparm/lift_vparm.c)
lw_add_plugin(LiftCpp      c/lift_cpp/lift.cpp)
lw_add_plugin(Checker      c/checker_node/checker.c)
lw_add_plugin(Invert       c/invert_filter/invert.c)
lw_add_plugin(LiftTool     c/lift_tool/lift_tool.c)
lw_add_plugin(MeshCentroid c/mesh_centroid/mesh_centroid.c)

# Dear ImGui settings window in its own Win32 + D3D11 window (section 5).
# Windows only: the macOS equivalent (Cocoa + Metal backends) is not written yet.
if(WIN32)
  set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")
  lw_add_plugin(GainImGui c/imgui_gain/gain.cpp
      ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp
      ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
      ${IMGUI_DIR}/backends/imgui_impl_win32.cpp ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp)
  target_include_directories(GainImGui PRIVATE "${IMGUI_DIR}")
  target_link_libraries(GainImGui PRIVATE d3d11)
endif()

# ---- tests: the plug-in's own source in an ordinary program (section 9) ----
enable_testing()
add_executable(test_lift tests/test_lift.c c/lift/lift.c)
target_include_directories(test_lift PRIVATE "${LWSDK}/include" c/lift)
target_compile_definitions(test_lift PRIVATE LIFT_TESTING
    $<IF:$<BOOL:${WIN32}>,_MSWIN,_MACOS>)
if(MSVC)
  set_target_properties(test_lift PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
add_test(NAME lift COMMAND test_lift)
