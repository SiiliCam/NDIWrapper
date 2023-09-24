# CMakeList.txt : CMake project for Logger, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (SampleExample main.cpp)

target_link_libraries(SampleExample PUBLIC NDIWrapper Logger)
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET SampleExample PROPERTY CXX_STANDARD 20)
endif()


add_custom_command(TARGET SampleExample POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  "${NDI_SDK_PATH}/Bin/x64/Processing.NDI.Lib.x64.dll"
  $<TARGET_FILE_DIR:SampleExample>
)

# TODO: Add tests and install targets if needed.