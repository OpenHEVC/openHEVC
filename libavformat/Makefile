include $(SUBDIR)../config.mak

NAME = avformat

HEADERS = avformat.h                                                    \
          avio.h                                                        \
          version.h                                                     \

OBJS = allformats.o         \
       avio.o               \
       aviobuf.o            \
       cutils.o             \
       format.o             \
       id3v1.o              \
       id3v2.o              \
       metadata.o           \
       options.o            \
       qtpalette.o          \
       protocols.o          \
       riff.o               \
       url.o                \
       utils.o              \

OBJS-$(HAVE_LIBC_MSVCRT)                 += file_open.o

# subsystems
OBJS-$(CONFIG_ISO_MEDIA)                 += isom.o
OBJS-$(CONFIG_NETWORK)                   += network.o
OBJS-$(CONFIG_RIFFDEC)                   += riffdec.o
OBJS-$(CONFIG_SHARED)                    += log2_tab.o golomb_tab.o

# muxers/demuxers
OBJS-$(CONFIG_H264_DEMUXER)              += h264dec.o rawdec.o
OBJS-$(CONFIG_HEVC_DEMUXER)              += hevcdec.o rawdec.o
OBJS-$(CONFIG_MOV_DEMUXER)               += mov.o mov_chan.o replaygain.o
OBJS-$(CONFIG_MPEGTS_DEMUXER)            += mpegts.o

# protocols I/O
OBJS-$(CONFIG_FILE_PROTOCOL)             += file.o



# Windows resource file
SLIBOBJS-$(HAVE_GNU_WINDRES)             += avformatres.o

SKIPHEADERS-$(CONFIG_FFRTMPCRYPT_PROTOCOL) += rtmpdh.h
SKIPHEADERS-$(CONFIG_NETWORK)            += network.h rtsp.h

