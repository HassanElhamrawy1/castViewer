# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\castViewer_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\castViewer_autogen.dir\\ParseCache.txt"
  "castViewer_autogen"
  )
endif()
