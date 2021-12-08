# moonlight-xbox
A port of [Moonlight Stream](https://moonlight-stream.org/) for playing games using GeForce Experience or [Sunshine](https://github.com/loki-47-6F-64/sunshine) for the Xbox One and Xbox Series X|S family of consoles


**This Application uses [XBOX UWP Dev Mode](https://docs.microsoft.com/en-us/windows/uwp/xbox-apps/devkit-activation) to function properly**

**This application is still in early stages of development. Expect things to not work or working badly**

## Installation and Usage
1. Enable the Dev Mode on your Xbox https://docs.microsoft.com/en-us/windows/uwp/xbox-apps/devkit-activation
2. Using the Device Portal, install the Application, including dependencies
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
Remember to clone with submodules enabled!
You need [VCPKG](https://vcpkg.io/en/index.html) installed in the "vcpkg" folder inside the repository to handle the dependencies. 

In particular, the FFMPEG port file (https://github.com/microsoft/vcpkg/blob/master/ports/ffmpeg/portfile.cmake) shoud be modified to include the `--enable-d3d11va` configure flag

Required ports are `pthread:x64-uwp, pthreads:x64-uwp, curl:x64-uwp, openssl:x64-uwp, expat:x64-uwp, zlib:x64-uwp, ffmpeg:x64-uwp, nlohmann-json:x64-uwp`
**NEW:** nlohmann-json:x64-uwp is also needed as of builds > 1.6.0

On a native x64 Visual Studio Prompt, run the `generate-thirdparty-projects.bat` inside the source folder to generate moonlight-common-c build files

After that, you can open and run the .sln fie
