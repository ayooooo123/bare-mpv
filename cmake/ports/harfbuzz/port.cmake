include_guard(GLOBAL)

find_port(freetype)

declare_port(
  "https://github.com/harfbuzz/harfbuzz/releases/download/9.0.0/harfbuzz-9.0.0.tar.xz"
  harfbuzz
  CMAKE
  DEPENDS freetype
  BYPRODUCTS
    lib/libharfbuzz.a
  ARGS
    -DBUILD_SHARED_LIBS=OFF
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DHB_HAVE_FREETYPE=ON
    -DFREETYPE_INCLUDE_DIR=${freetype_PREFIX}/include/freetype2
    -DFREETYPE_INCLUDE_DIRS=${freetype_PREFIX}/include/freetype2
    -DFREETYPE_LIBRARY=${freetype_PREFIX}/lib/libfreetype.a
    -DFREETYPE_LIBRARIES=${freetype_PREFIX}/lib/libfreetype.a
    -DFREETYPE_FOUND=TRUE
    -DHB_BUILD_TESTS=OFF
    -DHB_BUILD_UTILS=OFF
    -DHB_BUILD_SUBSET=OFF
    -DHB_HAVE_GLIB=OFF
    -DHB_HAVE_GOBJECT=OFF
    -DHB_HAVE_ICU=OFF
    -DHB_HAVE_GRAPHITE2=OFF
    -DHB_HAVE_INTROSPECTION=OFF
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
