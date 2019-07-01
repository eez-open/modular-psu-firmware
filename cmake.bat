set SDL2DIR=..\SDL2-2.0.9
set SDL2IMAGEDIR=..\SDL2_image-2.0.4

if not exist "build" mkdir "build"
if not exist "build\win32" mkdir "build\win32"

cd build\win32

cmake ..\..

cd ..\..