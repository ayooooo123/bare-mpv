include_guard(GLOBAL)

find_port(freetype)
find_port(fribidi)
find_port(harfbuzz)

declare_port(
  "https://github.com/libass/libass/releases/download/0.17.3/libass-0.17.3.tar.gz"
  libass
  MESON
  DEPENDS freetype fribidi harfbuzz
  BYPRODUCTS
    lib/libass.a
  ENV
    "PKG_CONFIG_PATH=${harfbuzz_PREFIX}/lib/pkgconfig:${fribidi_PREFIX}/lib/pkgconfig:${freetype_PREFIX}/lib/pkgconfig"
  ARGS
    --default-library=static
    -Dfontconfig=disabled
    -Db_staticpic=true
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
