### Tracy Profiling Setup

This has only been tested with the profiler running on macOS. There aren't very many defined Zones yet, YMMV. 

# Edit Utils/TracyBootstrap.cpp and enter the IP where the Tracy profiler will be running.

# In Visual Studio, build the ReleaseWithTracy solution.

# You can build the profiler out of the submodule directory:

$ cd third_party/tracy-xbox
$ cmake -B profiler/build -S profiler -DCMAKE_BUILD_TYPE=Release
$ cmake --build profiler/build --config Release -j8

# In another terminal, start the Python relay server that allows both client and server to connect via outbound connections.

$ third_party/tracy-xbox/extra/relay/tracy-relay.py

# Launch the profiler. It will open to an ImGui UI with a connection dialog.

$ profiler/build/tracy-profiler

# Hit Connect to connect to 127.0.0.1, you should see "Waiting for connection...". The relay will log a server connecting.

# Start a Moonlight stream on Xbox. The Tracy session starts in StreamPage when the stream starts.

