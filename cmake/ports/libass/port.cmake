include_guard(GLOBAL)

find_port(freetype)
find_port(fribidi)
find_port(harfbuzz)

set(libass_cflags
  "-I${freetype_PREFIX}/include -I${freetype_PREFIX}/include/freetype2 -I${fribidi_PREFIX}/include -I${harfbuzz_PREFIX}/include -I${harfbuzz_PREFIX}/include/harfbuzz"
)

set(libass_libs
  "-L${freetype_PREFIX}/lib -lfreetype -L${fribidi_PREFIX}/lib -lfribidi -L${harfbuzz_PREFIX}/lib -lharfbuzz"
)

declare_port(
  "https://github.com/libass/libass/releases/download/0.17.3/libass-0.17.3.tar.gz"
  libass
  AUTOTOOLS
  DEPENDS freetype fribidi harfbuzz
  BYPRODUCTS
    lib/libass.a
  ENV
    "CFLAGS=${libass_cflags}"
    "LIBS=${libass_libs}"
    "FREETYPE_CFLAGS=-I${freetype_PREFIX}/include/freetype2"
    "FREETYPE_LIBS=-L${freetype_PREFIX}/lib -lfreetype"
    "FRIBIDI_CFLAGS=-I${fribidi_PREFIX}/include"
    "FRIBIDI_LIBS=-L${fribidi_PREFIX}/lib -lfribidi"
    "HARFBUZZ_CFLAGS=-I${harfbuzz_PREFIX}/include/harfbuzz"
    "HARFBUZZ_LIBS=-L${harfbuzz_PREFIX}/lib -lharfbuzz"
  ARGS
    --disable-shared
    --enable-static
    --disable-fontconfig
    --disable-require-system-font-provider
    --with-pic
)

add_library(ass STATIC IMPORTED GLOBAL)

add_dependencies(ass ${libass})

set_target_properties(
  ass
  PROPERTIES
  IMPORTED_LOCATION "${libass_PREFIX}/lib/libass.a"
)

file(MAKE_DIRECTORY "${libass_PREFIX}/include")

target_include_directories(
  ass
  INTERFACE "${libass_PREFIX}/include"
)

target_link_libraries(ass INTERFACE freetype fribidi harfbuzz)
