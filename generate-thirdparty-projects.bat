cd third_party\moonlight-common-c
cmake -b build -S . -DCMAKE_TOOLCHAIN_FILE=..\..\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-uwp -G "Visual Studio 16 2019" -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION="10.0" -DTARGET_UWP=ON

