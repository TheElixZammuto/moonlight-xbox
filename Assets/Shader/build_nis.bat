@echo off
setlocal
pushd "%~dp0" || exit /b 1
set "HLSL=..\..\third_party\NVIDIAImageScaling\NIS\NIS_Main.hlsl"
set "EXIT_CODE=0"
fxc /T cs_5_0 /E main /O3 /D NIS_SCALER=1 /D NIS_HDR_MODE=0 /D NIS_BLOCK_WIDTH=32 /D NIS_BLOCK_HEIGHT=32 /D NIS_THREAD_GROUP_SIZE=128 /D NIS_USE_HALF_PRECISION=1 /Fo "nis_sdr_fp16.cso" "%HLSL%" || goto :fail
fxc /T cs_5_0 /E main /O3 /D NIS_SCALER=1 /D NIS_HDR_MODE=2 /D NIS_BLOCK_WIDTH=32 /D NIS_BLOCK_HEIGHT=32 /D NIS_THREAD_GROUP_SIZE=128 /D NIS_USE_HALF_PRECISION=1 /Fo "nis_hdr_fp16.cso" "%HLSL%" || goto :fail
fxc /T cs_5_0 /E main /O3 /D NIS_SCALER=1 /D NIS_HDR_MODE=0 /D NIS_BLOCK_WIDTH=32 /D NIS_BLOCK_HEIGHT=24 /D NIS_THREAD_GROUP_SIZE=128 /D NIS_USE_HALF_PRECISION=0 /Fo "nis_sdr_fp32.cso" "%HLSL%" || goto :fail
fxc /T cs_5_0 /E main /O3 /D NIS_SCALER=1 /D NIS_HDR_MODE=2 /D NIS_BLOCK_WIDTH=32 /D NIS_BLOCK_HEIGHT=24 /D NIS_THREAD_GROUP_SIZE=128 /D NIS_USE_HALF_PRECISION=0 /Fo "nis_hdr_fp32.cso" "%HLSL%" || goto :fail
goto :cleanup

:fail
set "EXIT_CODE=%ERRORLEVEL%"

:cleanup
popd
endlocal & exit /b %EXIT_CODE%
