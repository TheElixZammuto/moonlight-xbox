## Setup Tips

### Xbox Series S/X

#### Video

* Connect Xbox to an HDMI 2.1 port with a good-quality cable.
* In Xbox Settings -> General -> TV & display options:
  - Set 4K120 or the maximum your TV can support. HDR only works when the Xbox is outputting 4K.
  - Device control: enable any CEC options you want.
  - Video modes:
    - Check every checkbox except Dolby Vision for Gaming. (50Hz, 24Hz, auto low-latency mode, 4:2:2, 4K, HDR10, Auto HDR, Allow Dolby Vision).
    - Set Variable refresh rate to Gaming Only. Xbox UWP apps can't use VRR.
  - Video fidelity & overscan
    - Color depth: 30 bits per pixel (10-bit)
    - Color space: Standard

These settings should have already put your TV into Game Mode, but make sure it's in that mode before continuing. All TV settings will be from an LG CX. Please feel free to add info about other TV brands.

Go into Xbox's "Calibrate HDR for games" menu.

Go to TV picture settings for HDR Game Mode. Find "Dynamic Tone Mapping" (LG: Picture Mode Settings -> Advanced Controls) and select HGiG. This option probably only appears in game mode. If you don't have this option, look up the peak brightness for your TV on rtings.com and set that value. While you're in settings, make sure Picture Options -> Black Level is set to Auto.

Press Next for the Minimum Luminance screen. Press RB+RT+LB+LT on the controller to display the actual nits values being displayed in the upper-right. Follow the instructions to set Min, Max, and Max Full Frame values. On my CX, the values are 0, 800, 800. Select Done. The TV should go back to SDR Game Mode.

In picture settings for SDR Game Mode, make sure Picture Options -> Black Level is set to Auto.

#### Audio

* Volume & audio output
  - HDMI audio: Stereo, 5.1, or 7.1 uncompressed. Any other format will result in added latency.
  - Headset format: Stereo uncompressed

### Xbox One S/X

#### Video

* In Xbox Settings -> General -> TV & display options:
  - Set 4K60 or 1440p120. HDR only works when the Xbox is outputting 4K.
  - Device control: enable any CEC options you want.
  - Video modes:
    - Check every checkbox that is available.
    - Set Variable refresh rate to Gaming Only. Xbox UWP apps can't use VRR.
  - Video fidelity & overscan
    - Color depth: if using HDR: 30 bits per pixel (10-bit), otherwise 8-bit.
    - Color space: Standard

ALLM may have set your TV to Game Mode. If not, you may need to manually switch to it.

#### Audio

* Volume & audio output
  - HDMI audio: Stereo, 5.1, or 7.1 uncompressed. Any other format will result in added latency.
  - Headset format: Stereo uncompressed
  
#### Networking

Gigabit Ethernet is strongly recommended. If you must use Wi-Fi, try to have your access point within line of sight of the console. Your host PC must be wired. The performance graphs can be used to get a sense of Wi-Fi vs. wired quality. 

If your host uses a 2.5Gb or higher network card, be aware that this can cause streaming problems at high bitrates due to the speed difference between host and client. If you have any packet loss, try forcing your host to 1Gb in Device Manager -> NIC -> Advanced -> Speed & Duplex. 

## **Moonlight Host Settings** (after pairing, Start -> Host Settings)

#### Resolution/FPS

4K120 works best with a Windows host and an Nvidia 4070+, 5070+ GPU. Be sure to leave the Xbox system output at 4K when streaming a lower resolution, so that HDR will still work.

#### Bitrate

Moonlight will choose a default bitrate based on resolution and FPS that should work well. Higher bitrates can be used, but above 300Mbps you may start to experience packet loss with HEVC. This number will be lower on the slower consoles. A good number to try with 4K120 HEVC is 150Mbps. For PyroWave, the slider allows values up to 1000Mbps, but the recommended bitrate for 4K120 4:4:4 is 800Mbps. Some headroom needs to be left for audio, input, and other overhead. 

#### Audio

Stereo, 5.1, and 7.1 should match the system audio output. If using surround sound with Xbox connected to your TV, you must enable eARC in your TV and AVR/sound bar because of the additional bandwidth needed for LPCM audio. Use Stereo if you have any problems.

Play Audio on Host PC: check this if using Stereo, and uncheck for surround. When unchecked, the server will use Steam speakers instead of the default audio device.

#### Autostart

No App will show Desktop and other app items, or you can choose one to start it automatically.

#### Video Codecs

HEVC (H.265) should be used in most cases. H.264 can be used if you have problems, but only supports SDR and is limited to 1080p60. If your server supports PyroWave, additional options will be available for 4:2:0 and 4:4:4 PyroWave. Choosing a codec will adjust the bitrate to the recommended value for the chosen resolution and FPS. 

#### Enable HDR

Check the HDR box if your Xbox supports HDR output. The host's current state will determine which mode the Xbox uses. When the host is using HDR, Xbox switches to HDR. If the host is in SDR, a 10-bit SDR stream is used. You can toggle Windows HDR mode in the Quick Menu -> Other actions -> Toggle HDR.

#### Optimize host resolution

Check this box if the server needs to change the host's resolution, refresh rate, or HDR to match the client. Most users don't need this setting.

#### Frame Pacing

#### Show performance stats

If checked, stats are displayed when connecting. This setting is best left unchecked, since stats can be enabled from the Quick Menu.

#### Show performance graphs

Leave this checked and stats unchecked. It will be grayed out, but when showing stats, the graphs will also be shown.

#### Audio buffer

Select the desired audio buffer size from 10ms to 50ms (default 30ms). It is a trade-off between low latency and audio glitches and dropouts. 10ms requires a very stable network. If you experience any audio glitches, raise the buffer one notch in the Quick Menu.

Audio latency when streaming is affected by the host & client audio and network stacks, the TV/AVR/sound bar audio latency, and this audio buffer. Total audio latency is difficult to measure and will be much higher than this value. Latency above 150ms is not uncommon. 

The lowest latency on Xbox is usually found by connecting headphones to the controller’s 3.5mm jack, or by using a dongle-based wireless headset. Be careful with the volume of the headset audio when using headphones. 

## **Quick Menu**

While streaming, press both View + Menu (select & start) to bring up the quick menu. These buttons are buffered for 50ms to give enough time to trigger the combo without sending extra unwanted button presses to the host. This may result in these buttons feeling slightly laggier compared to other buttons when pausing games or bringing up the game map, etc.

### _Mouse Mode_

Select Enter or Exit Mouse Mode. In this mode:

* Move pointer: Left stick
* Slow pointer speed: hold RB
* Mouse wheel: right stick
* Left click: A or LT
* Right click: X or RT
* Open keyboard: Y
* Xbox/Guide button: B

### _Show Keyboard_

Brings up the standard Xbox keyboard, or a larger custom keyboard with more buttons such as the Windows key. Other keyboards can be enabled in global Moonlight settings.

Press B to dismiss the keyboard.

### _Other Actions_

This menu lets you send the Xbox button press to Steam, for example. Toggle HDR sends the Win-Alt-B combo to toggle HDR<->SDR. 

_Reset Decoder_ can be used if there are any compression artifacts. This option also resets the frame pacer and flushes the frame queue.

_Toggle frame pacing mode_ swaps between Immediate and Display-locked. See below for more details.

### _Show Stats_

Displays stream stats in the upper left. If graphs are enabled in host settings, 6 additional graphs are shown to the right. On Xbox One, stats and graphs should not be left visible because they can hurt stream performance. 

### _Show Logs_

Displays recent log output messages. Having logs open will lower stream performance, especially on Xbox One.

### _Audio_

If you experience audio glitches, you can raise the audio buffer size here. The current glitch total since the start of the stream or last buffer change is also shown here. It's normal to have a few glitches. Increase the buffer if you have more than 10.

### _Disconnect_, _Disconnect and Close_

Stop streaming, or stop and close the server's session. 

## **Stats & Graphs**

The following stats are displayed:

* Video stream: Resolution, FPS, codec, bit depth, SDR/HDR
* Bitrate: Average video bitrate in Mbps, plus the peak bitrate within the last 10 seconds.
* Incoming frame rate from network: raw framerate from the server. A properly configured server will limit itself and send frames no faster than the Xbox's refresh rate (59.94 or 119.88).
* Decoding frame rate: Rate of frames being decoded. Should always match the incoming rate.
* Rendering frame rate: Either the decoding frame rate (immediate frame pacing) or the max refresh rate (display-locked). A discrepancy in any of the framerate numbers can indicate problems such as a bitrate that is too high, a resolution/FPS that an older Xbox cannot handle, etc.
* Host processing latency: This number represents the encoding time from the host GPU. A high number here is not necessarily cause for alarm, as long as you are receiving the correct framerate, the host is keeping up. If this is high and the incoming FPS is too low, check for the following common causes: NVENC presets above P1 or other non-default encoder settings. Steam Recording, Game Bar Recording, OBS, or other apps using the GPU encoder.
* Packets/sec: the total number of UDP video packets being processed in one second. Lost/Recovered shows the number of packets in the last 60s that didn't arrive, and those that could be recovered through Forward Error Correction (FEC). If these numbers are not zero, you have a network problem that needs to be addressed. If PyroWave shows high numbers here, check that your host NIC is set to 1.0 Gbps and not 2.5 or higher. 
* Frames dropped by your network connection: any packet loss is shown here. Any dropped frames causes Moonlight to request a new keyframe from the server, a slow process. Using Ethernet should eliminate all dropped packets. If you have packet loss on a wired network, check all cabling and reboot network switches. It's possible to cover for a small amount of packet loss (< 5-10%) through raising the server's FEC (forward error correction) percentage. Use iperf3 to measure network performance in more detail. Be sure to test in [UDP mode](https://oneuptime.com/blog/post/2026-03-20-test-udp-throughput-iperf3/view) in order to better simulate Moonlight's network traffic.
* Frames dropped due to network jitter: Despite the word network, this stat reports packets dropped for any reason by the frame pacer. The usual reason the pacer drops frames is when receiving frames faster than they can be rendered. Try resetting the decoder/frame queue/pacer by using the Reset Decoder quick menu option.
* Average network latency: a very rough estimate of network ping. A wired network should generally never show more than 1ms here with 0 variance. Wi-Fi will be significantly higher and more variable, and will directly affect all other frame-related stats.
* Average reassembly/decoding time: reassembly is the CPU cost to piece together the dozens of UDP packets for each frame, check them for errors, and deliver this to the decoder. Decoding time is the CPU cost of passing the frame to the GPU for actual decoding. For PyroWave, the additional GPU decoding time is shown. PyroWave uses a lot of packets and has a higher reassembly time than other codecs. Use a lower bitrate if the number seems too high. Typical numbers for Xbox Series X @ 800Mbps is around 6.5ms reassembly, 0.5ms CPU, 1.8ms GPU. Don't add these numbers together or compare them to other platforms which may not track data the same way.
* Average frames in queue: The frame queue holds 1-3 frames after they have been decoded and are waiting to be displayed. The ideal state is a value of 1.0, meaning a frame is always ready as soon as it's needed, and the incoming framerate is not exceeding the capability of the Xbox. If you use Apollo, this number will be higher than 1.0 because Apollo lacks support for Xbox. You should consider switching to another server. Sometimes this value can report a higher value, and the best way to deal with this is to stream something that isn't at max framerate. Any lower framerate will quickly clear the frame queue. You can also use the quick menu's Reset Decoder feature to flush the queue. Changing frame pacing modes may also help.
* Average frame queue/render/present delay: This line reports the following timings:
  * Time a frame spent in the frame queue. This is from the end of decoding until the frame is pulled out of the queue by the renderer.
  * Render: Time spent drawing the frame, including the stats, graphs, and log overlays. 
  * Present Delay: This value is how long a frame takes from the Present API call until being shown on the display. It is usually about 1 frame. 

## **Frame pacing modes**

* Immediate: This is the default frame pacer on Xbox Series and provides the lowest latency at any framerate. Incoming frames are presented at the next available frame interval (8.34ms for 120hz, 16.68ms at 60hz). When receiving the maximum 119.88fps from a compatible server (i.e. not Apollo), this should result in perfect pacing with no dropped frames. At lower framerates, some minor stutter is unavoidable, as this mode optimizes for the lowest latency. When possible, try to run games at or above 120fps. The ideal stream should show all graphs (except bitrate) as a flat line.

* Display-locked: Default for Xbox One. This pacer always renders at the output refresh rate, so it will always deliver 119.88fps regardless of incoming framerate. It uses the incoming timestamps from the host to choose the best time to display each frame. When there aren't enough frames, it will repeat the previous frame. This mode may produce a smoother stream for variable framerates as well as situations such as 24fps within 60fps (3:2 pulldown). This mode can have slightly higher latency.
