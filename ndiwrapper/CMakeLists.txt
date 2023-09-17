# Specify the required source files
set(SOURCES "src/NDIReceiver.cpp" "src/ThreadPool.cpp" "src/NDISender.cpp")

# Create the NDIReceiver library
add_library(NDIWrapper ${SOURCES})

include_directories(${NDI_SDK_PATH}/Include)

# Link Boost libraries to NDIReceiver
target_link_libraries(NDIWrapper 
    PUBLIC 
        ${CMAKE_THREAD_LIBS_INIT} 
        Logger 
        CommonTypes
        ${NDI_LIBRARY}
        tinyxml2
)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

target_include_directories(NDIWrapper PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)


add_dependencies(NDIWrapper Logger)