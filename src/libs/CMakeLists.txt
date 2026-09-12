add_subdirectory(decoders)
add_subdirectory(glad)
add_subdirectory(ESFMu)
add_subdirectory(loguru)
add_subdirectory(manymouse)
add_subdirectory(nuked)

if (OPT_DEBUGGER)
  add_subdirectory(PDCurses)
endif()

add_subdirectory(residfp)
add_subdirectory(riffcpp)
add_subdirectory(simde)
add_subdirectory(tal-chorus)
add_subdirectory(whereami)
add_subdirectory(YM7128B_emu)
# The ZMBV codec exists only for video capture, and is the tree's only user of
# zlib-ng. See OPT_CAPTURE_VIDEO in the top-level CMakeLists.
if (C_CAPTURE_VIDEO)
  add_subdirectory(zmbv)
endif()

include_directories(.)
