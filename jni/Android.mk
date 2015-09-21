# copyright (c) 2013 Zhang Rui <bbcallen@gmail.com>
#
# This file is part of ksyPlayer.
#
# ksyPlayer is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# ksyPlayer is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with ksyPlayer; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

LOCAL_PATH := $(call my-dir)

MY_APP_JNI_ROOT := $(realpath $(LOCAL_PATH))
MY_APP_ANDROID_ROOT := $(realpath $(MY_APP_JNI_ROOT)/..)
TARGET_ARCH_ABI := armeabi-v7a
TARGET_ABI := armeabi-v7a

MY_APP_OUTPUT_PATH := $(realpath $(MY_APP_ANDROID_ROOT)/build/armv7a/output)
MY_APP_INCLUDE_PATH := $(realpath $(MY_APP_OUTPUT_PATH)/include)

include $(call all-subdir-makefiles)
