@rem Build and test Mbed TLS with Visual Studio using msbuild.
@rem Usage: windows_msbuild [RETARGET]
@rem   RETARGET: version of Visual Studio to emulate
@rem             https://docs.microsoft.com/en-us/cpp/build/how-to-modify-the-target-framework-and-platform-toolset

@rem These parameters are hard-coded for now.
set "arch=x64" & @rem "x86" or "x64"
set "cfg=Release" & @rem "Debug" or "Release"
set "vcvarsall=C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"

if not "%~1"=="" set "retarget=,PlatformToolset=%1"

@rem If the %USERPROFILE%\Source directory exists, then running
@rem vcvarsall.bat will silently change the directory to that directory.
@rem Setting the VSCMD_START_DIR environment variable causes it to change
@rem to that directory instead.
set "VSCMD_START_DIR=%~dp0\..\visualc\VS2010"

"%vcvarsall%" x64 && ^
msbuild /t:Rebuild /p:Configuration=%cfg%%retarget% /m mbedTLS.sln
