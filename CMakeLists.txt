# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

option(BUILD_ONLY_LIB "Build only the library" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("ndiwrapper")

# other projects use logger and commontypes as well so no reason to have multiple versions of them
if (NOT TARGET Logger)
  add_subdirectory(external/Logger)
endif()

if (NOT TARGET CommonTypes)
  add_subdirectory(external/CommonTypes)
endif()

add_subdirectory(external/tinyxml2)

if (DEFINED ENV{NDI_SDK_ENV_PATH})
    set(NDI_SDK_PATH $ENV{NDI_SDK_ENV_PATH})
else()
    if (ANDROID)
        set(NDI_SDK_PATH "C:/Program Files/NDI/NDI 6 SDK (Android)")
    else()
        set(NDI_SDK_PATH "C:/Program Files/NDI/NDI 6 SDK")
    endif()
endif()

# Conditional NDI SDK Path and Library
if (ANDROID)
  include_directories(${NDI_SDK_PATH}/Include)

  if(ANDROID_ABI STREQUAL "armeabi-v7a")
      set(NDI_LIBRARY_PATH "${NDI_SDK_PATH}/Lib/armeabi-v7a")
      set(NDI_LIBRARY "${NDI_SDK_PATH}/Lib/armeabi-v7a/libndi.so")
  elseif(ANDROID_ABI STREQUAL "arm64-v8a")
      set(NDI_LIBRARY_PATH "${NDI_SDK_PATH}/Lib/arm64-v8a")
      set(NDI_LIBRARY "${NDI_SDK_PATH}/Lib/arm64-v8a/libndi.so")
  elseif(ANDROID_ABI STREQUAL "x86_64")
      set(NDI_LIBRARY_PATH "${NDI_SDK_PATH}/Lib/x86_64")
      set(NDI_LIBRARY "${NDI_SDK_PATH}/Lib/x86_64/libndi.so")
  elseif(ANDROID_ABI STREQUAL "x86")
      set(NDI_LIBRARY_PATH "${NDI_SDK_PATH}/Lib/x86")
      set(NDI_LIBRARY "${NDI_SDK_PATH}/Lib/x86/libndi.so")
  endif()

else()
  include_directories(${NDI_SDK_PATH}/Include)
  
  set(NDI_LIBRARY_PATH "${NDI_SDK_PATH}/Lib/x64")
  
  find_library(NDI_LIBRARY
    NAMES Processing.NDI.Lib.x64
    PATHS ${NDI_SDK_PATH}/Lib/x64
  )
endif()

find_package(Threads REQUIRED)

# Include sub-projects.
add_subdirectory ("ndiwrapper")
message("build only lib: ${BUILD_ONLY_LIB}")
if(NOT BUILD_ONLY_LIB)
  add_subdirectory ("sampleapplication")
endif()