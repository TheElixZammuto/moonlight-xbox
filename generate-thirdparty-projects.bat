cd third_party\moonlight-common-c
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=..\..\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-uwp -G "Visual Studio 17 2022" -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION="10.0" -DTARGET_UWP=ON -DVCPKG_MANIFEST_MODE=on -DVCPKG_MANIFEST_DIR=../..
cd ..\..\libgamestream
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=..\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-uwp -G "Visual Studio 17 2022" -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION="10.0" -DTARGET_UWP=ON  -DVCPKG_MANIFEST_MODE=on -DBUILD_SHARED_LIBS=off -DVCPKG_MANIFEST_DIR=..
cd ..
