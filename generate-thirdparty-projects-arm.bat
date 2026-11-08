@echo off
rem ARM32 (Windows 10 Mobile) variant of generate-thirdparty-projects.bat.
rem Uses the arm-uwp vcpkg triplet; the vcpkg toolchain maps it to the
rem CMake "ARM" generator platform. Projects land in the same build\ dirs
rem the solution already references.
rem
rem NOTE: this overwrites the x64-generated projects in build\ (x64 can be
rem regenerated later by running generate-thirdparty-projects.bat).

call "%~dp0apply-submodule-patches.cmd"
if errorlevel 1 goto :fail

cd third_party\moonlight-common-c
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=..\..\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=arm-uwp -A "ARM,version=10.0.15063.0" -T v141 -DCMAKE_C_FLAGS=/I"%~dp0w10m-compat" /I"%~dp0third_party\moonlight-common-c" /I"%~dp0third_party\moonlight-common-c\enet\include" /wd4028 -G "Visual Studio 17 2022" -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION="10.0" -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DTARGET_UWP=ON -DVCPKG_MANIFEST_MODE=off -DVCPKG_INSTALLED_DIR=%~dp0vcpkg_installed -DVCPKG_MANIFEST_DIR=../.. -DBUILD_SHARED_LIBS=off
if errorlevel 1 goto :fail
cd ..\..\libgamestream
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=..\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=arm-uwp -A "ARM,version=10.0.15063.0" -T v141 -DCMAKE_C_FLAGS=/I"%~dp0w10m-compat" /I"%~dp0third_party\moonlight-common-c" /I"%~dp0third_party\moonlight-common-c\enet\include" /wd4028 -G "Visual Studio 17 2022" -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION="10.0" -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DTARGET_UWP=ON -DVCPKG_MANIFEST_MODE=off -DVCPKG_INSTALLED_DIR=%~dp0vcpkg_installed -DBUILD_SHARED_LIBS=off -DVCPKG_MANIFEST_DIR=..
if errorlevel 1 goto :fail
cd ..
echo ARM third-party projects generated OK
exit /b 0

:fail
echo CMake generation FAILED
exit /b 1
