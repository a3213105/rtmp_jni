#! /usr/bin/env bash
##
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# This script is based on projects below
# https://github.com/yixia/FFmpeg-Android
# http://git.videolan.org/?p=vlc-ports/android.git;a=summary

#----------
UNI_BUILD_ROOT=`pwd`
FF_TARGET=$1
set -e
set +x

FF_ALL_ARCHS="armv7a armv5 x86 arm64-v8a"
FF_ACT_ARCHS="armv5 armv7a x86 arm64-v8a"

echo_archs() {
    echo "===================="
    echo "[*] check archs"
    echo "===================="
    echo "FF_ALL_ARCHS = $FF_ALL_ARCHS"
    echo "FF_ACT_ARCHS = $FF_ACT_ARCHS"
    echo ""
}

#----------
case "$FF_TARGET" in
    "")
        echo_archs
        sh tools/do-compile-librtmp.sh armv7a
    ;;
    armv5|armv7a|arm64-v8a|x86)
        echo_archs
        sh tools/do-compile-librtmp.sh $FF_TARGET
    ;;
    all)
        echo_archs
        for ARCH in $FF_ACT_ARCHS
        do
            sh tools/do-compile-librtmp.sh $ARCH
        done
    ;;
    clean)
        echo_archs
        for ARCH in $FF_ALL_ARCHS
        do
            cd ffmpeg-$ARCH && make distclean && cd -
        done
        rm -rf ./build/librtmp-*
    ;;
    check)
        echo_archs
    ;;
    *)
        echo "Usage:"
        echo "  compile-rtmp.sh armv5|armv7a|x86|arm64-v8a"
        echo "  compile-rtmp.sh all"
        echo "  compile-rtmp.sh clean"
        echo "  compile-rtmp.sh check"
        exit 1
    ;;
esac

#----------
echo "\n--------------------"
echo "[*] Finished"
echo "--------------------"
