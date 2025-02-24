# moonlight-xbox 

[![Download](https://get.microsoft.com/images/en-us%20dark.svg)](https://apps.microsoft.com/store/detail/9MW1BS08ZBTH?launch=true&mode=full)

A port of [Moonlight Stream](https://moonlight-stream.org/) for playing games using GeForce Experience or [Sunshine](https://github.com/LizardByte/sunshine) for the Xbox One and Xbox Series X|S family of consoles

**This application is still in early stages of development. Expect things to not work or working badly**

## Installation and Usage
### For Retail Mode (you probably want to use this)
1. Open Microsoft Edge and click the "Get it from Microsoft" Button above 
2. Downlad Moonlight UWP from the Microsoft Store
3. Open Moonlight on Xbox and, if not already, your host app
4. Press the "+" button, Insert your PC IP Address and press "Connect"
5. Pair if neeeded
6. Choose from the list below the application you want to run
7. ???
8. Profit!

### For Dev Mode
**Looking for the Standard Dev Mode Builds? Can be found here:** [Link](https://github.com/TheElixZammuto/moonlight-xbox/releases)
**Looking for the Bleeding Edge Builds? Can be found here:** [Link](https://github.com/TheElixZammuto/moonlight-xbox/actions)
1. Enable the Dev Mode on your Xbox https://docs.microsoft.com/en-us/windows/uwp/xbox-apps/devkit-activation
2. Using the Device Portal, install the Application (moonlight-xbox-dx.msixbundle) and, in the following step, add the required dependencies (Microsoft.UI.Xaml.2.7.appx and Microsoft.VCLibs.x64.14.00.appx)
3. Open Moonlight on Xbox and, if not already, your host app
4. Your PC should already be on the list. If not, press the "+" button, Insert your PC IP Address and press "Connect"
5. Pair if neeeded
6. Choose from the list below the application you want to run
7. ???
8. Profit!

## What does work
- Connection and Pairing
- Application List fetching
- Video Streaming (configurable on a host-basis in the settings)
- Gamepad Input (with Rumble and a mouse mode to move the pointer using the gamepad)
- Keyboard (both on-screen and using an Hardware one)
- Graceful Disconnection
- Host configuration (for resolution and bitrate) and saved host history
- Audio
- HDR _(4k@120 HDR is currently unstable)__
- Wake-on-Lan

## What does NOT work
- Hardware Mouse (UWP Limitations sadly)
- Everything else not listed above

## Building

### Requirements

- Windows 10
- Visual Studio 2022

### Steps to build

1. Clone this repository (`moonlight-xbox`) with submodules enabled!
2. Install [VCPKG](https://vcpkg.io/en/index.html) and all dependencies:
    1. Run `git submodule update --init --recursive`
    2. Run `vcpkg\bootstrap-vcpkg.bat`
    3. Install dependencies: `.\vcpkg\vcpkg.exe install --triplet x64-uwp`
3. Run x64 Visual Studio Prompt (Tools → Command Line → Developer Command Prompt)
    1. Run `generate-thirdparty-projects.bat` to generate `moonlight-common-c` VS project
4. After all the actions above, you finally can open and build solution.
    1. Right-click the `moonlight-xbox-dx` project, select `Publish`, then select `Create App Packages...` to build your .msixbundle and dependencies.