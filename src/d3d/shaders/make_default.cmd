@echo off
"%DXSDK_DIR%\utilities\bin\x86\fxc.exe" /T vs_2_0 /Fh default_amb_VS.h default_VS.hlsl
"%DXSDK_DIR%\utilities\bin\x86\fxc.exe" /T vs_2_0 /DDIRECTIONALS /Fh default_amb_dir_VS.h default_VS.hlsl
"%DXSDK_DIR%\utilities\bin\x86\fxc.exe" /T vs_2_0 /DDIRECTIONALS /DPOINTLIGHTS /DSPOTLIGHTS /Fh default_all_VS.h default_VS.hlsl

"%DXSDK_DIR%\utilities\bin\x86\fxc.exe" /T ps_2_0 /Fh default_color_PS.h default_color_PS.hlsl
"%DXSDK_DIR%\utilities\bin\x86\fxc.exe" /T ps_2_0 /Fh default_color_tex_PS.h default_color_tex_PS.hlsl
