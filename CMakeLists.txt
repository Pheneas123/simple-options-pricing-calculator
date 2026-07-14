cmake_minimum_required(VERSION 3.20)
project(options_calculator_wasm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(core STATIC
  src/bs.cpp
)
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  add_executable(core_tests tests/bs_tests.cpp)
  target_link_libraries(core_tests PRIVATE core)
  add_test(NAME core_tests COMMAND core_tests)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  add_executable(options wasm/bindings.cpp)
  target_link_libraries(options PRIVATE core)
  target_include_directories(options PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

  target_compile_options(options PRIVATE
    -O3
    -sWASM=1
  )

  target_link_options(options PRIVATE
    -O3
    -sWASM=1
    -sMODULARIZE=1
    -sEXPORT_NAME=createModule
    -sENVIRONMENT=web
    -sALLOW_MEMORY_GROWTH=1
    --bind
    --no-entry
  )

  set_target_properties(options PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/docs"
    OUTPUT_NAME "options"
    SUFFIX ".js"
  )
endif()
