@echo off
setlocal

if "%SFML_DIR%"=="" (
  echo SFML_DIR is not set.
  echo Set SFML_DIR to your SFML 2.6.x CMake folder, for example:
  echo   set SFML_DIR=C:\libs\SFML-2.6.2\lib\cmake\SFML
  exit /b 1
)

cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64 -DSFML_DIR="%SFML_DIR%"
if errorlevel 1 exit /b 1

cmake --build build-windows --config Release
if errorlevel 1 exit /b 1

echo Built: build-windows\Release\gui.exe
