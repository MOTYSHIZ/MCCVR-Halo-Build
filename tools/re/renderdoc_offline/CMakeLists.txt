cmake_minimum_required(VERSION 3.20)
project(ce_renderdoc_offline LANGUAGES CXX)

foreach(required RENDERDOC_API_DIR RENDERDOC_IMPORT_LIBRARY RENDERDOC_BIN_DIR)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required; use prepare.py")
  endif()
endforeach()

add_executable(ce_renderdoc_replay replay.cpp)
target_compile_features(ce_renderdoc_replay PRIVATE cxx_std_17)
target_compile_definitions(ce_renderdoc_replay PRIVATE RENDERDOC_PLATFORM_WIN32 NOMINMAX)
target_include_directories(ce_renderdoc_replay PRIVATE "${RENDERDOC_API_DIR}")
target_link_libraries(ce_renderdoc_replay PRIVATE "${RENDERDOC_IMPORT_LIBRARY}")

add_executable(ce_renderdoc_map_fixture map_boundary_fixture.cpp)
target_compile_features(ce_renderdoc_map_fixture PRIVATE cxx_std_17)
target_include_directories(ce_renderdoc_map_fixture PRIVATE "${RENDERDOC_BIN_DIR}")
target_link_libraries(ce_renderdoc_map_fixture PRIVATE d3d11 dxguid)
