cmake_minimum_required (VERSION 2.6)

# Common packages required
project (HM-wrapper)

if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fPIC")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fPIC")
endif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")

find_package(SDL)
add_subdirectory(HM)

include_directories(HM/source/Lib src/Lib)

if(SDL_FOUND)
	include_directories("${SDL_INCLUDE_DIR}")
endif()

file ( GLOB LibWrapper_sources_files src/Lib/wrapper/*.cpp src/Lib/wrapper/*.h)
add_library (LibWrapper STATIC ${LibWrapper_sources_files})

file ( GLOB HevcDecoder_source_files src/App/decoder/*.c src/App/decoder/*.h)
