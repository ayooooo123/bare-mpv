include_guard(GLOBAL)

find_port(freetype)

declare_port(
  "https://github.com/harfbuzz/harfbuzz/releases/download/9.0.0/harfbuzz-9.0.0.tar.xz"
  harfbuzz
  MESON
  DEPENDS freetype
  BYPRODUCTS
    lib/libharfbuzz.a
  ENV
    "PKG_CONFIG_PATH=${freetype_PREFIX}/lib/pkgconfig"
  ARGS
    --default-library=static
    -Dfreetype=enabled
    -Dglib=disabled
    -Dgobject=disabled
    -Dcairo=disabled
    -Dchafa=disabled
    -Dicu=disabled
    -Dgraphite=disabled
    -Dgraphite2=disabled
    -Dtests=false
    -Dintrospection=disabled
    -Ddocs=false
    -Dutilities=false
    -Dbenchmark=false
    -Db_staticpic=true
)

add_library(harfbuzz STATIC IMPORTED GLOBAL)

add_dependencies(harfbuzz ${harfbuzz})

set_target_properties(
  harfbuzz
  PROPERTIES
  IMPORTED_LOCATION "${harfbuzz_PREFIX}/lib/libharfbuzz.a"
)

file(MAKE_DIRECTORY "${harfbuzz_PREFIX}/include")

target_include_directories(
  harfbuzz
  INTERFACE "${harfbuzz_PREFIX}/include"
)

target_link_libraries(harfbuzz INTERFACE freetype)
