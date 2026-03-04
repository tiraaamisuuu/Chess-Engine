cmake_minimum_required(VERSION 3.21)

project(chess_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

find_package(SFML 2.6 COMPONENTS graphics window system REQUIRED)

add_executable(gui
  src/main.cpp
  src/ui.cpp
)

if(TARGET sfml-graphics)
  target_link_libraries(gui PRIVATE sfml-graphics sfml-window sfml-system)
elseif(TARGET SFML::Graphics)
  target_link_libraries(gui PRIVATE SFML::Graphics SFML::Window SFML::System)
else()
  message(FATAL_ERROR "SFML targets not found. Ensure SFML 2.6.x is installed and SFML_DIR is set.")
endif()

add_custom_command(TARGET gui POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets"
          "$<TARGET_FILE_DIR:gui>/assets"
)

if(WIN32)
  add_custom_command(TARGET gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:gui>
            $<TARGET_FILE_DIR:gui>
    COMMAND_EXPAND_LISTS
  )
endif()
