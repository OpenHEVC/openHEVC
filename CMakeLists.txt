cmake_minimum_required (VERSION 2.8)
project (libavHM)
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/MyCMakeScripts)

if(APPLE)
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DHAVE_AV_CONFIG_H -DPIC -fPIC"  )
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DHAVE_AV_CONFIG_H -DPIC -fPIC"  )
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DHAVE_AV_CONFIG_H -DPIC -fPIC"  )
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DHAVE_AV_CONFIG_H -DPIC -fPIC"  )
endif(APPLE)

IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DPIC -Wl,-Bsymbolic"  )
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}  -DPIC -Wl,-Bsymbolic"  )
ENDIF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")

find_package(SDL REQUIRED)
find_package(SDL2)
find_package(CSFML 2.0 COMPONENTS graphics system)

if(MINGW)
#set(CMAKE_C_FLAGS "-march=i486 -mwindows")
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

add_library (LibOpenHevcWrapper SHARED ${libfilenames})
include_directories(.)
set_source_files_properties(sdl.c PROPERTIES COMPILE_FLAGS -I"${SDL_INCLUDE_DIR}")
add_executable(hevc getopt.c main.c sdl.c)
if(MINGW)
target_link_libraries(hevc LibOpenHevcWrapper ${SDL_LIBRARY} -lwinmm)
else(MINGW)
target_link_libraries(hevc LibOpenHevcWrapper ${SDL_LIBRARY})
endif(MINGW)

if(SDL2_FOUND)
set_source_files_properties(sdl2.c PROPERTIES COMPILE_FLAGS -I"${SDL2_INCLUDE_DIR}")
add_executable(hevc_sdl2 getopt.c main.c sdl2.c)
if(MINGW)
target_link_libraries(hevc_sdl2 LibOpenHevcWrapper ${SDL2_LIBRARY} -lwinmm)
else(MINGW)
target_link_libraries(hevc_sdl2 LibOpenHevcWrapper ${SDL2_LIBRARY})
endif(MINGW)
endif()

if(CSFML_FOUND)
set_source_files_properties(csfml.c PROPERTIES COMPILE_FLAGS -I"${CSFML_INCLUDE_DIR}")
add_executable(hevc_csfml getopt.c main.c csfml.c)
if(MINGW)
target_link_libraries(hevc_csfml LibOpenHevcWrapper ${CSFML_GRAPHICS_LIBRARY_RELEASE} ${CSFML_SYSTEM_LIBRARY_RELEASE} -lwinmm)
else(MINGW)
target_link_libraries(hevc_csfml LibOpenHevcWrapper ${CSFML_GRAPHICS_LIBRARY_RELEASE} ${CSFML_SYSTEM_LIBRARY_RELEASE})
endif(MINGW)
endif()


INSTALL(FILES "openHevcWrapper.h" DESTINATION include)

INSTALL(TARGETS LibOpenHevcWrapper DESTINATION lib PERMISSIONS
OWNER_READ OWNER_WRITE OWNER_EXECUTE
GROUP_READ GROUP_EXECUTE
WORLD_READ WORLD_EXECUTE)
