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
#SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -arch x86_64 -m64  -DPIC -Wl,-Bsymbolic -fno-tree-vectorize")
#SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -arch x86_64 -m64  -DPIC -Wl,-Bsymbolic -fno-tree-vectorize")
if(MINGW)
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -arch x86_64 -m64  -DPIC -Wl,-Bsymbolic")
SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -arch x86_64 -m64  -DPIC -Wl,-Bsymbolic")
else(MINGW)
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DPIC -fPIC -Wl,-Bsymbolic")
SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -DPIC -fPIC -Wl,-Bsymbolic")
endif(MINGW)
ENDIF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")

find_package(SDL)
find_package(SDL2)

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
libavcodec/hevc_filter.c
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

add_subdirectory(wrapper_hm)

OPTION(ENABLE_STATIC "enabled static library instead of shared" OFF)
if (ENABLE_STATIC)
add_library (LibOpenHevcWrapper STATIC ${libfilenames})
else(ENABLE_STATIC)
add_library (LibOpenHevcWrapper SHARED ${libfilenames})
endif(ENABLE_STATIC)
include_directories(.)

OPTION(ENABLE_EXECUTABLE "Generate the test application" ON)

if (ENABLE_EXECUTABLE)
if(SDL_FOUND)
set_source_files_properties(main_hm/sdl.c PROPERTIES COMPILE_FLAGS -I"${SDL_INCLUDE_DIR}")
add_executable(hevc main_hm/getopt.c main_hm/main.c main_hm/sdl.c)
if(MINGW)
target_link_libraries(hevc LibOpenHevcWrapper ${SDL_LIBRARY} -lwinmm)
else(MINGW)
target_link_libraries(hevc LibOpenHevcWrapper ${SDL_LIBRARY})
endif(MINGW)
endif()

if(SDL2_FOUND)
set_source_files_properties(main_hm/sdl2.c PROPERTIES COMPILE_FLAGS -I"${SDL2_INCLUDE_DIR}")
add_executable(hevc_sdl2 main_hm/getopt.c main_hm/main.c main_hm/sdl2.c)
if(MINGW)
target_link_libraries(hevc_sdl2 LibOpenHevcWrapper ${SDL2_LIBRARY} -lwinmm)
else(MINGW)
target_link_libraries(hevc_sdl2 LibOpenHevcWrapper ${SDL2_LIBRARY})
endif(MINGW)
endif()
endif(ENABLE_EXECUTABLE)

INSTALL(FILES "openHevcWrapper.h" DESTINATION include)

INSTALL(TARGETS LibOpenHevcWrapper DESTINATION lib PERMISSIONS
OWNER_READ OWNER_WRITE OWNER_EXECUTE
GROUP_READ GROUP_EXECUTE
WORLD_READ WORLD_EXECUTE)
