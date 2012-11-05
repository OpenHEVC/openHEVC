cmake_minimum_required (VERSION 2.6)

# Common packages required
project (HM-wrapper)

find_package(SDL REQUIRED)
add_subdirectory(HM)

include_directories(HM/source/Lib src/Lib ${SDL_INCLUDE_DIR})


file ( GLOB LibWrapper_sources_files src/Lib/wrapper/*.cpp src/Lib/wrapper/*.h)
add_library (LibWrapper STATIC ${LibWrapper_sources_files})

file ( GLOB HevcDecoder_source_files src/App/decoder/*.c src/App/decoder/*.h)
add_executable(HevcDecoder ${HevcDecoder_source_files})
target_link_libraries(HevcDecoder LibWrapper TLibDecoder TLibVideoIO TLibCommon libmd5 TAppCommon ${SDL_LIBRARY})