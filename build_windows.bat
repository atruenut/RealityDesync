@echo off
rem ============================================================================
rem  Reality Desync - build the Windows VST3 (a 64-bit DLL) with MinGW-w64 gcc.
rem  Needs: g++ (MinGW-w64, 64-bit) on PATH, and git (only the first time, to get the VST3 SDK).
rem  Result: dist\Reality Desync.vst3\Contents\x86_64-win\Reality Desync.vst3
rem ============================================================================
setlocal EnableDelayedExpansion
cd /d "%~dp0"

if "%VST3_SDK_DIR%"=="" set "VST3_SDK_DIR=%~dp0vst3sdk"
if not exist "%VST3_SDK_DIR%\public.sdk" (
    echo Downloading the Steinberg VST3 SDK ^(MIT licence^)...
    git clone --depth 1 --branch v3.8.1_build_84 https://github.com/steinbergmedia/vst3sdk.git "%VST3_SDK_DIR%" || goto :fail
    git -C "%VST3_SDK_DIR%" submodule update --init --depth 1 base pluginterfaces public.sdk || goto :fail
)
where g++ >nul 2>nul || ( echo g++ was not found on PATH. Install MinGW-w64 ^(x86_64^) and try again. & goto :fail )

set "OBJ=build\obj"
if not exist "%OBJ%" mkdir "%OBJ%"
set "FLAGS=-std=c++17 -O3 -DRELEASE=1 -fvisibility=hidden -I"%VST3_SDK_DIR%" -Isrc/core"

set "SDKFILES=base/source/baseiids.cpp base/source/fbuffer.cpp base/source/fdebug.cpp base/source/fobject.cpp base/source/fstreamer.cpp base/source/fstring.cpp base/source/timer.cpp base/source/updatehandler.cpp base/thread/source/fcondition.cpp base/thread/source/flock.cpp pluginterfaces/base/conststringtable.cpp pluginterfaces/base/coreiids.cpp pluginterfaces/base/funknown.cpp pluginterfaces/base/ustring.cpp public.sdk/source/common/commoniids.cpp public.sdk/source/common/pluginview.cpp public.sdk/source/main/pluginfactory.cpp public.sdk/source/main/dllmain.cpp public.sdk/source/vst/vstaudioeffect.cpp public.sdk/source/vst/vstbus.cpp public.sdk/source/vst/vstcomponent.cpp public.sdk/source/vst/vstcomponentbase.cpp public.sdk/source/vst/vsteditcontroller.cpp public.sdk/source/vst/vstinitiids.cpp public.sdk/source/vst/vstparameters.cpp"
set "OWNFILES=src/core/Engine.cpp src/core/ParamTable.cpp src/vst3/RdProcessor.cpp src/vst3/RdController.cpp src/vst3/RdFactory.cpp"

set "OBJS="
for %%F in (%SDKFILES%) do (
    set "SRC=%VST3_SDK_DIR%\%%F"
    echo   %%~nxF
    g++ %FLAGS% -c "!SRC:/=\!" -o "%OBJ%\%%~nF.o" || goto :fail
    set "OBJS=!OBJS! %OBJ%\%%~nF.o"
)
for %%F in (%OWNFILES%) do (
    echo   %%~nxF
    g++ %FLAGS% -c "%%F" -o "%OBJ%\%%~nF.o" || goto :fail
    set "OBJS=!OBJS! %OBJ%\%%~nF.o"
)

set "OUT=dist\Reality Desync.vst3\Contents\x86_64-win"
if not exist "%OUT%" mkdir "%OUT%"
echo Linking...
g++ -shared -static -static-libgcc -static-libstdc++ !OBJS! -lole32 -luser32 -o "%OUT%\Reality Desync.vst3" || goto :fail

echo.
echo Done:  %~dp0%OUT%\Reality Desync.vst3
echo Copy the whole "Reality Desync.vst3" folder to  C:\Program Files\Common Files\VST3\  and rescan plugins in your DAW.
exit /b 0

:fail
echo.
echo BUILD FAILED
exit /b 1
