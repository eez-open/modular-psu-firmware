set SDL2DIR=..\SDL2-2.0.18
set SDL2IMAGEDIR=..\SDL2_image-2.0.5

if not exist "build" mkdir "build"
if not exist "build\win32-64bit" mkdir "build\win32-64bit"

cd build\win32-64bit

cmake -A x64 ..\..

cd ..\..