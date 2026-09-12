add_library(libdecoders STATIC
  flac.c
  mp3.cpp
  mp3_seek_table.cpp
  SDL_sound.c
  vorbis.c
  wav.c
)

# Opus is the only codec here that needs an external library; the rest decode
# through vendored headers. See OPT_OPUS in the top-level CMakeLists.
if (C_OPUS)
  target_sources(libdecoders PRIVATE opus.cpp)
endif()

target_include_directories(libdecoders PUBLIC ..)

if (C_OPUS)
  if(USE_SYSTEM_LIBS)
    pkg_check_modules(OPUSFILE REQUIRED IMPORTED_TARGET opusfile)

    target_include_directories(
      libdecoders PRIVATE ${OPUSFILE_INCLUDE_DIRS}
    )
    target_compile_options(
      libdecoders PRIVATE ${OPUSFILE_CFLAGS_OTHER}
    )
    target_link_libraries(
      libdecoders PRIVATE ${OPUSFILE_LIBRARIES}
    )
  else()
    find_package(OpusFile REQUIRED)
    target_link_libraries(
      libdecoders PRIVATE OpusFile::opusfile
    )
  endif()
endif()

target_link_libraries(libdecoders
  PRIVATE
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
