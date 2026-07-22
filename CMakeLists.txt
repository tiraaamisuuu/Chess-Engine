cmake_minimum_required(VERSION 3.21)

project(TiramisuChess VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CHESS_BUILD_GUI "Build the SFML desktop application" ON)
option(CHESS_ENABLE_WARNINGS "Enable strict compiler warnings" ON)

find_package(Threads REQUIRED)

add_library(chess_core INTERFACE)
target_include_directories(chess_core INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(chess_uci STATIC src/uci.cpp)
target_link_libraries(chess_uci PUBLIC chess_core Threads::Threads)

add_executable(tiramisu-uci src/uci_main.cpp)
target_link_libraries(tiramisu-uci PRIVATE chess_uci)

add_executable(tiramisu-tools src/tools_main.cpp)
target_link_libraries(tiramisu-tools PRIVATE chess_core Threads::Threads)

if(CHESS_BUILD_GUI)
  find_package(SFML 2.6 COMPONENTS graphics window system REQUIRED)

  add_executable(gui
    src/main.cpp
    src/ui.cpp
  )
  target_link_libraries(gui PRIVATE chess_uci)

  if(TARGET sfml-graphics)
    target_link_libraries(gui PRIVATE sfml-graphics sfml-window sfml-system)
  elseif(TARGET SFML::Graphics)
    target_link_libraries(gui PRIVATE SFML::Graphics SFML::Window SFML::System)
  else()
    message(FATAL_ERROR "SFML targets not found. Install SFML 2.6.x or configure with -DCHESS_BUILD_GUI=OFF.")
  endif()

  if(WIN32)
    target_compile_definitions(gui PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    target_link_libraries(gui PRIVATE Psapi)
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
endif()

if(CHESS_ENABLE_WARNINGS)
  if(MSVC)
    set(CHESS_WARNING_FLAGS /W4 /permissive-)
  else()
    set(CHESS_WARNING_FLAGS -Wall -Wextra -Wpedantic -Wshadow)
  endif()
  target_compile_options(chess_uci PRIVATE ${CHESS_WARNING_FLAGS})
  target_compile_options(tiramisu-uci PRIVATE ${CHESS_WARNING_FLAGS})
  target_compile_options(tiramisu-tools PRIVATE ${CHESS_WARNING_FLAGS})
  if(TARGET gui)
    target_compile_options(gui PRIVATE ${CHESS_WARNING_FLAGS})
  endif()
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(chess-core-tests tests/core_tests.cpp)
  target_link_libraries(chess-core-tests PRIVATE chess_core Threads::Threads)
  if(CHESS_ENABLE_WARNINGS)
    target_compile_options(chess-core-tests PRIVATE ${CHESS_WARNING_FLAGS})
  endif()
  add_test(NAME chess-core-tests COMMAND chess-core-tests)
  if(UNIX)
    add_test(NAME uci-smoke
      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_uci_smoke.sh" $<TARGET_FILE:tiramisu-uci>)
  endif()
endif()

install(TARGETS tiramisu-uci tiramisu-tools RUNTIME DESTINATION bin)
if(TARGET gui)
  install(TARGETS gui RUNTIME DESTINATION bin)
  install(DIRECTORY assets DESTINATION share/tiramisu-chess)
endif()
install(FILES README.md DESTINATION .)
install(DIRECTORY docs DESTINATION .)

set(CPACK_PACKAGE_NAME "TiramisuChess")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "TiramisuChess-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR "ZIP")
include(CPack)
