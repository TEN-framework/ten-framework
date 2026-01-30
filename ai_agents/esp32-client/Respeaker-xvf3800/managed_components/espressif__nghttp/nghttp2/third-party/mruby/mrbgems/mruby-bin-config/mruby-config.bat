@echo off

call :init "%~dp0."
goto top

:init
rem call init2 for treatments last directory separator
call :init2 "%~dp1."
goto :eof

:init2
set MRUBY_PACKAGE_DIR=%~f1
goto :eof

:top
shift
if "%0" equ "" goto :eof
if "%0" equ "--cc" goto cc
if "%0" equ "--cflags" goto cflags
if "%0" equ "--cxx" goto cxx
if "%0" equ "--cxxflags" goto cxxflags
if "%0" equ "--as" goto as
if "%0" equ "--asflags" goto asflags
if "%0" equ "--objc" goto objc
if "%0" equ "--objcflags" goto objcflags
if "%0" equ "--ld" goto ld
if "%0" equ "--ldflags" goto ldflags
if "%0" equ "--ldflags-before-libs" goto ldflagsbeforelibs
if "%0" equ "--libs" goto libs
if "%0" equ "--libmruby-path" goto libmrubypath
if "%0" equ "--help" goto showhelp
echo Invalid Option
goto :eof

:cc
echo MRUBY_CC
goto top

:cflags
echo MRUBY_CFLAGS
goto top

:cxx
echo MRUBY_CXX
goto top

:cxxflags
echo MRUBY_CXXFLAGS
goto top

:as
echo MRUBY_AS
goto top

:asflags
echo MRUBY_ASFLAGS
goto top

:objc
echo MRUBY_OBJC
goto top

:objcflags
echo MRUBY_OBJCFLAGS
goto top

:ld
echo MRUBY_LD
goto top

:libs
echo MRUBY_LIBS
goto top

:ldflags
echo MRUBY_LDFLAGS
goto top

:ldflagsbeforelibs
echo MRUBY_LDFLAGS_BEFORE_LIBS
goto top

:libmrubypath
echo MRUBY_LIBMRUBY_PATH
goto top

:showhelp
echo Usage: mruby-config [switches]
echo   switches:
echo   --cc                       print C compiler name
echo   --cflags                   print flags passed to C compiler
echo   --cxx                      print C++ compiler name
echo   --cxxflags                 print flags passed to C++ compiler
echo   --as                       print assembler name
echo   --asflags                  print flags passed to assembler
echo   --objc                     print Objective C compiler name
echo   --objcflags                print flags passed to Objective C compiler
echo   --ld                       print linker name
echo   --ldflags                  print flags passed to linker
echo   --ldflags-before-libs      print flags passed to linker before linked libraries
echo   --libs                     print linked libraries
echo   --libmruby-path            print libmruby path
