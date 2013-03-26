cmake_minimum_required (VERSION 2.6)
project (libavHM)
if(APPLE)
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DHAVE_AV_CONFIG_H -DPIC -fPIC"  )
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DHAVE_AV_CONFIG_H -DPIC -fPIC"  )
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DHAVE_AV_CONFIG_H -DPIC -fPIC"  )
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DHAVE_AV_CONFIG_H -DPIC -fPIC"  )
endif(APPLE)

IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC"  )
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC"  )
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fPIC"  )
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fPIC"  )
ENDIF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")

find_package(SDL REQUIRED)
if(MINGW)
set(CMAKE_C_FLAGS "-march=i486 -mwindows")
endif(MINGW)

set(libfilenames
openHevcWrapper.c
libavcodec/allcodecs.c
libavcodec/avpacket.c
libavcodec/avpicture.c
libavcodec/cabac.c
libavcodec/dsputil.c
libavcodec/golomb.c
libavcodec/hevc_cabac.c
libavcodec/hevc_mvs.c
libavcodec/hevc_parser.c
libavcodec/hevc_ps.c
libavcodec/hevc_refs.c
libavcodec/hevc_sei.c
libavcodec/hevc.c
libavcodec/hevcdsp.c
libavcodec/hevcpred.c
libavcodec/imgconvert.c
libavcodec/options.c
libavcodec/parser.c
libavcodec/pthread.c
libavcodec/simple_idct.c
libavcodec/utils.c
libavcodec/videodsp.c
libavutil/audioconvert.c
libavutil/avstring.c
libavutil/atomic.c
libavutil/buffer.c
libavutil/dict.c
libavutil/eval.c
libavutil/frame.c
libavutil/imgutils.c
libavutil/log.c
libavutil/mathematics.c
libavutil/md5.c
libavutil/mem.c
libavutil/opt.c
libavutil/pixdesc.c
libavutil/rational.c
libavutil/samplefmt.c
)

include_directories(. ${SDL_INCLUDE_DIR} wrapper_hm/src/Lib)
add_library (LibOpenHevcWrapper STATIC ${libfilenames})
add_executable(hevc getopt.c main.c sdl.c)
if(MINGW)
target_link_libraries(hevc LibOpenHevcWrapper ${SDL_LIBRARY} -lwinmm)
else(MINGW)
target_link_libraries(hevc LibOpenHevcWrapper ${SDL_LIBRARY})
endif(MINGW)
INSTALL(FILES "openHevcWrapper.h" DESTINATION include)

INSTALL(TARGETS LibOpenHevcWrapper DESTINATION lib PERMISSIONS
OWNER_READ OWNER_WRITE OWNER_EXECUTE
GROUP_READ GROUP_EXECUTE
WORLD_READ WORLD_EXECUTE)
