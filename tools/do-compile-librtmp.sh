#! /usr/bin/env bash
#
# Copyright (C) 2014 Miguel Botón <waninkoko@gmail.com>
# Copyright (C) 2014 Zhang Rui <bbcallen@gmail.com>
#
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

#--------------------
echo "===================="
echo "[*] check env $1"
echo "===================="
set -e

if [ -z "$ANDROID_NDK" ]; then
    echo "You must define ANDROID_NDK, ANDROID_SDK before starting."
    echo "They must point to your NDK and SDK directories.\n"
    exit 1
fi

#--------------------
# common defines
FF_ARCH=$1
if [ -z "$FF_ARCH" ]; then
    echo "You must specific an architecture 'arm, armv7a, x86, ...'.\n"
    exit 1
fi

# try to detect NDK version
FF_NDK_REL=$(grep -o '^r[0-9]*.*' $ANDROID_NDK/RELEASE.TXT 2>/dev/null|cut -b2-)
case "$FF_NDK_REL" in
    9*|10*)
        # we don't use 4.4.3 because it doesn't handle threads correctly.
        if test -d ${ANDROID_NDK}/toolchains/arm-linux-androideabi-4.8
        # if gcc 4.8 is present, it's there for all the archs (x86, mips, arm)
        then
            echo "NDKr$FF_NDK_REL detected"
        else
            echo "You need the NDKr9 or later"
            exit 1
        fi
    ;;
    7|8|*)
        echo "You need the NDKr9 or later"
        exit 1
    ;;
esac

FF_BUILD_ROOT=`pwd`
FF_ANDROID_PLATFORM=android-9
FF_GCC_VER=4.8


FF_TARGET_NAME=
FF_BUILD_NAME=
FF_SOURCE=
FF_CROSS_PREFIX=

FF_CFG_FLAGS=
FF_PLATFORM_CFG_FLAGS=

FF_EXTRA_CFLAGS=
FF_EXTRA_LDFLAGS=

#----- armv7a begin -----
if [ "$FF_ARCH" = "armv7a" ]; then
    FF_BUILD_NAME=rtmpdump-armv7a
    FF_TARGET_NAME=armv7a
    FF_SOURCE=$FF_BUILD_ROOT/$FF_BUILD_NAME/librtmp
	
    FF_CROSS_PREFIX=arm-linux-androideabi
    FF_TOOLCHAIN_NAME=${FF_CROSS_PREFIX}-${FF_GCC_VER}

    FF_PLATFORM_CFG_FLAGS="android-armv7"

elif [ "$FF_ARCH" = "armv5" ]; then
    FF_BUILD_NAME=rtmpdump-armv5
    FF_TARGET_NAME=armv5
    FF_SOURCE=$FF_BUILD_ROOT/$FF_BUILD_NAME/librtmp
	
    FF_CROSS_PREFIX=arm-linux-androideabi
    FF_TOOLCHAIN_NAME=${FF_CROSS_PREFIX}-${FF_GCC_VER}

    FF_PLATFORM_CFG_FLAGS="android"

elif [ "$FF_ARCH" = "x86" ]; then
    FF_BUILD_NAME=rtmpdump-x86
    FF_TARGET_NAME=x86
    FF_SOURCE=$FF_BUILD_ROOT/$FF_BUILD_NAME/librtmp
	
    FF_CROSS_PREFIX=i686-linux-android
    FF_TOOLCHAIN_NAME=x86-${FF_GCC_VER}

    FF_PLATFORM_CFG_FLAGS="android-x86"

else
    echo "unknown architecture $FF_ARCH";
    exit 1
fi

FF_TOOLCHAIN_PATH=$FF_BUILD_ROOT/build/$FF_TARGET_NAME/toolchain

FF_SYSROOT=$FF_TOOLCHAIN_PATH/sysroot
FF_PREFIX=$FF_BUILD_ROOT/build/$FF_TARGET_NAME/output
FF_TARGET_PREFIX=$FF_BUILD_ROOT/build/$FF_TARGET_NAME/output

mkdir -p $FF_PREFIX
mkdir -p $FF_SYSROOT
mkdir -p $FF_BUILD_ROOT/$FF_BUILD_NAME
cp -r rtmpdump/* $FF_BUILD_ROOT/$FF_BUILD_NAME/

#--------------------
echo "\n--------------------"
echo "[*] make NDK standalone toolchain"
echo "--------------------"
UNAMES=$(uname -s)
FF_MAKE_TOOLCHAIN_FLAGS="--install-dir=$FF_TOOLCHAIN_PATH"
if [ "$UNAMES" = "Darwin" ]; then
    echo "build on darwin-x86_64"
    FF_MAKE_TOOLCHAIN_FLAGS="$FF_MAKE_TOOLCHAIN_FLAGS --system=darwin-x86_64"
    FF_MAKE_FLAG=-j`sysctl -n machdep.cpu.thread_count`
fi

FF_MAKEFLAGS=
if which nproc >/dev/null
then
    FF_MAKEFLAGS=-j`nproc`
elif [ "$UNAMES" = "Darwin" ] && which sysctl >/dev/null
then
    FF_MAKEFLAGS=-j`sysctl -n machdep.cpu.thread_count`
fi

FF_TOOLCHAIN_TOUCH="$FF_TOOLCHAIN_PATH/touch"
if [ ! -f "$FF_TOOLCHAIN_TOUCH" ]; then
    $ANDROID_NDK/build/tools/make-standalone-toolchain.sh \
        $FF_MAKE_TOOLCHAIN_FLAGS \
        --platform=$FF_ANDROID_PLATFORM \
        --toolchain=$FF_TOOLCHAIN_NAME
    touch $FF_TOOLCHAIN_TOUCH;
fi

#--------------------
echo "\n--------------------"
echo "[*] check librtmp env"
echo "--------------------"
export PATH=$FF_TOOLCHAIN_PATH/bin:$PATH

export COMMON_FF_CFG_FLAGS=

FF_CFG_FLAGS="$FF_CFG_FLAGS $COMMON_FF_CFG_FLAGS"
FF_PLATFORM_CFG_FLAGS="-O3 -Wall -pipe \
    -std=c99 \
    -ffast-math \
    -fstrict-aliasing -Werror=strict-aliasing \
    -Wno-psabi -Wa,--noexecstack \
    -DANDROID -DNDEBUG"
#--------------------
# Standard options:
#FF_CFG_FLAGS="$FF_CFG_FLAGS SYS=android"
FF_CFG_FLAGS="$FF_CFG_FLAGS CRYPTO="
FF_CFG_FLAGS="$FF_CFG_FLAGS STATIC="
FF_CFG_FLAGS="$FF_CFG_FLAGS prefix=$FF_PREFIX"
FF_CFG_FLAGS="$FF_CFG_FLAGS CROSS_COMPILE=${FF_CROSS_PREFIX}-"
#FF_CFG_FLAGS="$FF_CFG_FLAGS XCFLAGS=$FF_PLATFORM_CFG_FLAGS"

#--------------------
echo "\n--------------------"
echo "[*] compile librtmp"
echo "--------------------"
cd $FF_SOURCE
echo "make install $FF_CFG_FLAGS"
make install $FF_CFG_FLAGS XCFLAGS="$FF_PLATFORM_CFG_FLAGS"
cp -rf $FF_PREFIX/include/librtmp $FF_TARGET_PREFIX/include
cp -f $FF_PREFIX/lib/librtmp.a $FF_TARGET_PREFIX/lib
cp -f $FF_PREFIX/lib/pkgconfig/librtmp.pc $FF_TARGET_PREFIX/lib/pkgconfig

