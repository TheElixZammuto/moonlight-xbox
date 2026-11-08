# Moonlight for Windows 10 Mobile

A Windows 10 Mobile (ARM32) port of [moonlight-xbox](https://github.com/TheElixZammuto/moonlight-xbox), itself a port of [Moonlight Stream](https://moonlight-stream.org/) for playing games from GeForce Experience or [Sunshine](https://github.com/LizardByte/sunshine).

This repository is the Windows 10 Mobile port, maintained at [FutureZQ/moonlight-windows10mobile](https://github.com/FutureZQ/moonlight-windows10mobile). The upstream Xbox project and its history are credited in the release branch.

**Status: experimental.** The target device is a Lumia-class Windows 10 Mobile phone (ARM32, build 15254) with D3D11 feature level 9_3 and H.264 High Profile hardware decode. PC/Xbox targets are not the focus of this fork.

## What works

- mDNS host discovery, HTTPS, PIN pairing
- Host list, app list, cover art
- ARM32 H.264 decode at 1280x720, NV12 + 9_3 shader rendering
- Audio, touch/trackpad input, Bluetooth mouse passthrough
- Graceful disconnect and back-button handling

## Install (sideload)

The Windows 10 Mobile Store publishing channel is closed. Install the signed .appx through Device Portal or enterprise sideloading:

1. Build or obtain moonlight-xbox-dx_<version>_ARM.appx.
2. Make sure the signing certificate is trusted on the phone.
3. Deploy through Device Portal or the package installer.
4. Start Moonlight, add the host, and pair with the PIN shown by Sunshine.

The binary/project name remains moonlight-xbox-dx for compatibility with the upstream codebase.

## Building

### Requirements

- Windows 10
- Visual Studio 2022
    - WinUI Application Development Workload
        - Windows 10 SDK (10.0.15063.0)
        - Universal Windows Platform Tools
        - C++ (v141) Universal Windows Platform Tools

### Steps to build

**You have two options:**

1. Clone this repository (`moonlight-windows10mobile`) with submodules enabled!
1. Install [VCPKG](https://vcpkg.io/en/index.html) and all dependencies:
    1. Run `git submodule update --init --recursive`
    1. Run `apply-submodule-patches.cmd` to apply the W10M submodule changes
    1. Run `vcpkg\bootstrap-vcpkg.bat`
    1. Install dependencies: `.\vcpkg\vcpkg.exe install --vcpkg-root vcpkg --triplet arm-uwp`
1. Run the **x64 Native Tools Command Prompt** (Tools → Command Line → Developer Command Prompt)
    1. Run `generate-thirdparty-projects-arm.bat` to generate the `moonlight-common-c` and `libgamestream` VS projects
1. Open `moonlight-xbox-dx.vcxproj` in Visual Studio 2022
1. In Solution Explorer, build these projects in order:
    - enet
    - moonlight-common-c
    - gamestream
1. In Solution Explorer, expand `moonlight-xbox-dx (Universal Windows)`, then double click `Package.appxmanifest`
1. Under the `Packaging` tab, next to `Publisher`, click the `Choose Certificate` button
    - If you already have a certificate, select it and skip the final step
1. In the `Choose a Certificate` window, click `Create...`
1. Fill out the create prompt. A password is unnecessary for personal/debug use.
1. Build the solution. The UWP project's `GenerateAppxPackageOnBuild` produces the package under `ARM\Release\AppPackages\...`.

**Or**

1. Clone this repository
1. Run `./setup-dev.ps1`
1. Have fun!
   
