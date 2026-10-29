#!/usr/bin/env bash
# Cross-compile the Windows VST3 DLL from Linux/macOS with MinGW-w64 (no CMake needed).
#   VST3_SDK_DIR=/path/to/vst3sdk ./build_cross_to_windows.sh
set -euo pipefail
cd "$(dirname "$0")"
SDK="${VST3_SDK_DIR:-$PWD/vst3sdk}"
if [ ! -d "$SDK/public.sdk" ]; then
  git clone --depth 1 --branch v3.8.1_build_84 https://github.com/steinbergmedia/vst3sdk.git "$SDK"
  git -C "$SDK" submodule update --init --depth 1 base pluginterfaces public.sdk
fi
CXX=${CXX:-x86_64-w64-mingw32-g++-posix}
command -v "$CXX" >/dev/null || CXX=x86_64-w64-mingw32-g++
mkdir -p build/obj
FLAGS="-std=c++17 -O3 -DRELEASE=1 -fvisibility=hidden -I$SDK -Isrc/core"
OBJS=()
for f in base/source/baseiids.cpp base/source/fbuffer.cpp base/source/fdebug.cpp base/source/fobject.cpp base/source/fstreamer.cpp base/source/fstring.cpp base/source/timer.cpp base/source/updatehandler.cpp base/thread/source/fcondition.cpp base/thread/source/flock.cpp pluginterfaces/base/conststringtable.cpp pluginterfaces/base/coreiids.cpp pluginterfaces/base/funknown.cpp pluginterfaces/base/ustring.cpp public.sdk/source/common/commoniids.cpp public.sdk/source/common/pluginview.cpp public.sdk/source/main/pluginfactory.cpp public.sdk/source/main/dllmain.cpp public.sdk/source/vst/vstaudioeffect.cpp public.sdk/source/vst/vstbus.cpp public.sdk/source/vst/vstcomponent.cpp public.sdk/source/vst/vstcomponentbase.cpp public.sdk/source/vst/vsteditcontroller.cpp public.sdk/source/vst/vstinitiids.cpp public.sdk/source/vst/vstparameters.cpp src/core/Engine.cpp src/core/ParamTable.cpp src/vst3/RdProcessor.cpp src/vst3/RdController.cpp src/vst3/RdFactory.cpp; do
  o="build/obj/$(basename "${f%.cpp}").o"
  case "$f" in src/*) s="$f";; *) s="$SDK/$f";; esac
  "$CXX" $FLAGS -c "$s" -o "$o"; OBJS+=("$o")
done
OUT="dist/Reality Desync.vst3/Contents/x86_64-win"
mkdir -p "$OUT"
"$CXX" -shared -static -static-libgcc -static-libstdc++ "${OBJS[@]}" -lole32 -luser32 -o "$OUT/Reality Desync.vst3"
echo "built: $OUT/Reality Desync.vst3"
