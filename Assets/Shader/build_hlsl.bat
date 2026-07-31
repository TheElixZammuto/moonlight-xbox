fxc /T vs_4_0_level_9_3 /Fo d3d11_vertex.fxc d3d11_vertex.hlsl

REM Texture2DArray sampling requires feature level 10.0, so it cannot use the _level_9_3 profile.
fxc /T ps_4_0 /Fo d3d11_yuv420_pixel_array.fxc d3d11_yuv420_pixel_array.hlsl
