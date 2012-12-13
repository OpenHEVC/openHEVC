cmake_minimum_required (VERSION 2.6)

project (libavHM)

# Common packages required
find_package(SDL REQUIRED)

add_subdirectory(wrapper_hm)

set(filenames
	main.c	sdl.c
libavcodec/allcodecs.clibavcodec/avpacket.clibavcodec/cabac.clibavcodec/dsputil.clibavcodec/golomb.clibavcodec/hevc_cabac.clibavcodec/hevc_parser.clibavcodec/hevc_ps.clibavcodec/hevc.clibavcodec/hevcdsp.clibavcodec/hevcpred.clibavcodec/imgconvert.clibavcodec/options.clibavcodec/parser.clibavcodec/pthread.clibavcodec/simple_idct.clibavcodec/utils.c
libavutil/audioconvert.clibavutil/avstring.clibavutil/dict.clibavutil/eval.clibavutil/imgutils.clibavutil/log.clibavutil/mathematics.clibavutil/mem.clibavutil/opt.c
libavutil/pixdesc.clibavutil/rational.clibavutil/samplefmt.c
)

include_directories(. ${SDL_INCLUDE_DIR} wrapper_hm/src/Lib)

add_executable(hevc ${filenames})

target_link_libraries(hevc ${SDL_LIBRARY} LibWrapper TLibDecoder TLibVideoIO TLibCommon libmd5)

