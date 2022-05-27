set SDL2DIR=..\SDL2-2.0.18
set SDL2IMAGEDIR=..\SDL2_image-2.0.5

if not exist "build" mkdir "build"
if not exist "build\win32" mkdir "build\win32"

cd build\win32

cmake -A Win32 ..\..

cd ..\..