# moonlight-xbox
A port of [Moonlight Stream](https://moonlight-stream.org/) for playing games using GeForce Experience or [Sunshine](https://github.com/loki-47-6F-64/sunshine) for the Xbox One and Xbox Series X|S family of consoles


**Looking for the Store Link? Can be found here:** [Link](https://www.microsoft.com/store/apps/9MW1BS08ZBTH)

**Looking for the Dev Mode Builds? Can be found here:** [Link](https://github.com/TheElixZammuto/moonlight-xbox/releases)

**This application is still in early stages of development. Expect things to not work or working badly**

## Installation and Usage
1. Enable the Dev Mode on your Xbox https://docs.microsoft.com/en-us/windows/uwp/xbox-apps/devkit-activation
2. Using the Device Portal, install the Application (moonlight-xbox-dx.msixbundle) and, in the following step, add the required dependencies (Microsoft.UI.Xaml.2.7.appx and Microsoft.VCLibs.x64.14.00.appx)
3. Open Moonlight on Xbox and, if not already, your host app
4. Press the "+" button, Insert your PC IP Address and press "Connect"
5. Pair if neeeded
6. Choose from the list below the application you want to run
7. ???
8. Profit!

## What does work
- Connection and Pairing
- Application List fetching
- Video Streaming (configurable on a host-basis in the settings)
- Gamepad Input (with Rumble and a mouse mode to move the pointer using the gamepad)
- Graceful Disconnection
- Host configuration (for resolution and bitrate) and saved host history
- Audio

## What does NOT work
- Other means of input (e.g. Hardware Mouse and Keyboard)
- 4K Support
- 120FPS
- HDR (Probably not possible with the Xbox UWP Platform)
- Everything else not listed above

## Building

### Requirements

- Windows 10
- Visual Studio 2022

### Steps to build

1. Clone this repository (`moonlight-xbox`) with submodules enabled!
2. Install [VCPKG](https://vcpkg.io/en/index.html) and all dependencies:
    1. Clone VCPKG (`git clone https://github.com/Microsoft/vcpkg.git`) into `moonlight-xbox/vcpkg`
    2. Run `vcpkg/bootstrap-vcpkg.bat`
    3. Hack `ffmpeg` port by adding `set(OPTIONS "${OPTIONS} --enable-d3d11va")` to `vcpkg/ports/ffmpeg/portfile.cmake`
    4. Install dependencies: `vcpkg install pthread:x64-uwp pthreads:x64-uwp curl:x64-uwp openssl:x64-uwp expat:x64-uwp zlib:x64-uwp ffmpeg[avcodec,avdevice,avfilter,avformat,core,gpl,postproc,swresample,swscale]:x64-uwp nlohmann-json:x64-uwp bzip2:x64-uwp brotli:x64-uwp x264:x64-uwp freetype:x64-uwp opus:x64-uwp`
3. Run x64 Visual Studio Prompt (Tools → Command Line → Developer Command Prompt)
    1. Run `generate-thirdparty-projects.bat` to generate `moonlight-common-c` VS project
    2. Go to `libgamestream` and run `build-uwp.bat` to generate `libgamestream` VS project
4. After all the actions above, you finnally can open and build solution. Please, build it only in Release mode (Debug mode is broken)
