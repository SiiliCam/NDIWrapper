# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("ndiwrapper")

add_subdirectory(external/CommonTypes)
add_subdirectory(external/Logger)
set(NDI_SDK_PATH "C:/Program Files/NDI/NDI 5 SDK")
include_directories(${NDI_SDK_PATH}/Include)
find_library(NDI_LIBRARY
  NAMES Processing.NDI.Lib.x64
  PATHS ${NDI_SDK_PATH}/Lib/x64
)

find_package(Threads REQUIRED)

# Include sub-projects.
add_subdirectory ("ndiwrapper")
add_subdirectory ("sampleapplication")
