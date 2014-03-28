LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
APP_STL := gnustl_static
APP_ABI := armeabi
APP_OPTIM := debug
LOCAL_ARM_MODE := arm

#LOCAL_CFLAGS    := -I <Your header files goes here>
LOCAL_CFLAGS    += -g
LOCAL_CFLAGS    += -ggdb
LOCAL_CFLAGS    += -O1

openhevc_files := \
    libavutil/audioconvert.c \
    libavutil/avstring.c \
    libavutil/atomic.c \
    libavutil/base64.c \
    libavutil/buffer.c \
    libavutil/cpu.c \
    libavutil/dict.c \
    libavutil/eval.c \
    libavutil/frame.c \
    libavutil/imgutils.c \
    libavutil/log.c \
    libavutil/mathematics.c \
    libavutil/crc.c \
    libavutil/lzo.c \
    libavutil/md5.c \
    libavutil/mem.c \
    libavutil/opt.c \
    libavutil/parseutils.c \
    libavutil/pixdesc.c \
    libavutil/rational.c \
    libavutil/random_seed.c \
    libavutil/samplefmt.c \
    libavutil/time.c \
    libavutil/arm/cpu.c \
    gpac/modules/openhevc_dec/openHevcWrapper.c \
    libavformat/allformats.c \
    libavformat/avc.c \
    libavformat/avio.c \
    libavformat/aviobuf.c \
    libavformat/concat.c \
    libavformat/cutils.c \
    libavformat/file.c \
    libavformat/format.c \
    libavformat/id3v2.c \
    libavformat/isom.c \
    libavformat/hevcdec.c \
    libavformat/matroska.c \
    libavformat/matroskadec.c \
    libavformat/metadata.c \
    libavformat/mov.c \
    libavformat/movenc.c \
    libavformat/movenchint.c \
    libavformat/mpegts.c \
    libavformat/options.c \
    libavformat/os_support.c \
    libavformat/rawdec.c \
    libavformat/utils.c \
    libavformat/url.c \
    libavformat/urldecode.c \
    libavcodec/allcodecs.c \
    libavcodec/avpacket.c \
    libavcodec/avpicture.c \
    libavcodec/cabac.c \
    libavcodec/dsputil.c \
    libavcodec/golomb.c \
    libavcodec/hevc_cabac.c \
    libavcodec/hevc_mvs.c \
    libavcodec/hevc_parser.c \
    libavcodec/hevc_ps.c \
    libavcodec/hevc_refs.c \
    libavcodec/hevc_sei.c \
    libavcodec/hevc_filter.c \
    libavcodec/hevc.c \
    libavcodec/hevcdsp.c \
    libavcodec/hevcpred.c \
    libavcodec/imgconvert.c \
    libavcodec/options.c \
    libavcodec/parser.c \
    libavcodec/pthread_slice.c \
    libavcodec/pthread_frame.c \
    libavcodec/pthread.c \
    libavcodec/simple_idct.c \
    libavcodec/utils.c \
    libavcodec/videodsp.c \
    main_hm/getopt.c \
    main_hm/main.c

LOCAL_SRC_FILES := $(openhevc_files)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/platform/arm/ \
                    $(LOCAL_PATH)/gpac/modules/openhevc_dec/
LOCAL_MODULE := openhevc
ifeq ($(TARGET_ARCH),arm)
  LOCAL_SDK_VERSION := 9
endif
include $(BUILD_EXECUTABLE)
