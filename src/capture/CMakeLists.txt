target_sources(libdosboxcommon PRIVATE 
  capture.cpp
  capture_audio.cpp
  capture_midi.cpp)

# Image and video capture each carry a third-party dependency nothing else in
# the tree needs -- libpng and zlib-ng respectively -- so an embedder that
# cannot reach them can drop both. See OPT_CAPTURE_IMAGE / OPT_CAPTURE_VIDEO in
# the top-level CMakeLists. Audio and MIDI capture have no such dependency and
# are always built.
if (C_CAPTURE_IMAGE)
  target_sources(libdosboxcommon PRIVATE
    image/image_capturer.cpp
    image/image_saver.cpp
    image/image_scaler.cpp
    image/png_writer.cpp)

  target_link_libraries(libdosboxcommon PRIVATE PNG::PNG)
endif()

if (C_CAPTURE_VIDEO)
  target_sources(libdosboxcommon PRIVATE capture_video.cpp)

  target_link_libraries(libdosboxcommon PRIVATE zmbv)
endif()
