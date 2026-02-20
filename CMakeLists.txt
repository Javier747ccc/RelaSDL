cmake_minimum_required(VERSION 3.20)

project(RelaSDL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/yaml-cpp)

add_executable(RelaSDL
  RelaSDL.cpp
  PracticalSocket.cpp
)

target_include_directories(RelaSDL PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3
  ${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3_ttf
)

target_compile_definitions(RelaSDL PRIVATE WIN32)

target_link_directories(RelaSDL PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

target_link_libraries(RelaSDL PRIVATE
  SDL3
  SDL3_ttf
  yaml-cpp
  ws2_32
)

add_custom_command(TARGET RelaSDL POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_CURRENT_SOURCE_DIR}/SDL3.dll
          $<TARGET_FILE_DIR:RelaSDL>/SDL3.dll
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_CURRENT_SOURCE_DIR}/SDL3_ttf.dll
          $<TARGET_FILE_DIR:RelaSDL>/SDL3_ttf.dll
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_CURRENT_SOURCE_DIR}/Arial-Rounded-MT-Bold.ttf
          $<TARGET_FILE_DIR:RelaSDL>/Arial-Rounded-MT-Bold.ttf
)
