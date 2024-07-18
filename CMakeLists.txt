cmake_minimum_required(VERSION 3.19)
project(moonlight-xbox
        DESCRIPTION "Moonlight port for Xbox One/Series Family of Console"
        HOMEPAGE_URL "https://github.com/TheElixZammuto/moonlight-xbox"
)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "Windows SDK version" FORCE)

# set the generator for Visual Studio 2022 if not already set
if(NOT CMAKE_GENERATOR)
    message(STATUS "Setting generator to 'Visual Studio 17 2022' as none was specified.")
    set(CMAKE_GENERATOR "Visual Studio 17 2022" CACHE STRING "CMake generator" FORCE)
endif()

# set env variables
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_overlay_triplets")  # attempt to specify build type... not working :/
set(VCPKG_DOWNLOADS "${CMAKE_CURRENT_BINARY_DIR}/vcpkg/downloads")
set(VCPKG_TARGET_TRIPLET "x64-uwp-${CMAKE_BUILD_TYPE}")  # vcpkg will automatically make this lowercase

# This is a custom target which will automatically update git submodules and install dependencies via vcpkg
# Other targets will depend on this one, to guarantee that this one is executed first
add_custom_target(deps
        COMMAND "${CMAKE_COMMAND}" -E echo "Updating git submodules..."
        COMMAND git submodule update --init --recursive
        COMMAND "${CMAKE_COMMAND}" -E echo "Bootstrapping vcpkg..."
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/bootstrap-vcpkg.bat" -disableMetrics
        COMMAND "${CMAKE_COMMAND}" -E echo "Installing dependencies via vcpkg..."
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/vcpkg.exe" install --downloads-root="${VCPKG_DOWNLOADS}" --overlay-triplets="${VCPKG_OVERLAY_TRIPLETS}" --triplet="${VCPKG_TARGET_TRIPLET}" --x-install-root="${CMAKE_CURRENT_BINARY_DIR}/vcpkg/installed" --x-buildtrees-root="${CMAKE_CURRENT_BINARY_DIR}/vcpkg/buildtrees" --clean-after-build
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Bootstrapping vcpkg and installing dependencies"
)

# Path to the vcpkg toolchain file
set(VCPKG_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_TOOLCHAIN_FILE "${VCPKG_TOOLCHAIN_FILE}")

set(CMAKE_SYSTEM_NAME WindowsStore)
set(TARGET_UWP ON)
set(VCPKG_MANIFEST_MODE ON)
set(VCPKG_MANIFEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Common CMake arguments for external projects
set(COMMON_CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES}
        -DCMAKE_TOOLCHAIN_FILE=${VCPKG_TOOLCHAIN_FILE}
        -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
        -DCMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}
        -DTARGET_UWP=${TARGET_UWP}
        -DVCPKG_MANIFEST_MODE=${VCPKG_MANIFEST_MODE}
        -DVCPKG_MANIFEST_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}
        -G "${CMAKE_GENERATOR}"
)
message(STATUS "COMMON_CMAKE_ARGS: ${COMMON_CMAKE_ARGS}")

include(ExternalProject)

# moonlight-common-c
ExternalProject_Add(moonlight-common-c
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/moonlight-common-c"
        BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/moonlight-common-c/build"
        CMAKE_ARGS ${COMMON_CMAKE_ARGS}
        BUILD_ALWAYS TRUE
)
add_dependencies(moonlight-common-c deps)

# libgamestream
ExternalProject_Add(libgamestream
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libgamestream"
        BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libgamestream/build"
        CMAKE_ARGS ${COMMON_CMAKE_ARGS}
        -DBUILD_SHARED_LIBS=off
        BUILD_ALWAYS TRUE
)
add_dependencies(libgamestream deps)
