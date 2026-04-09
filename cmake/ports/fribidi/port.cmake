include_guard(GLOBAL)

set(fribidi_args
  --default-library=static
  -Ddocs=false
  -Dtests=false
  -Db_staticpic=true
)

# Hide all symbols — statically linked into .bare, none should be public
if(NOT WIN32)
  list(APPEND fribidi_args
    -Dc_visibility_preset=hidden
    -Dcpp_visibility_preset=hidden
  )
endif()

declare_port(
  "https://github.com/fribidi/fribidi/releases/download/v1.0.16/fribidi-1.0.16.tar.xz"
  fribidi
  MESON
  BYPRODUCTS
    lib/libfribidi.a
  ARGS
    ${fribidi_args}
)

add_library(fribidi STATIC IMPORTED GLOBAL)

add_dependencies(fribidi ${fribidi})

set_target_properties(
  fribidi
  PROPERTIES
  IMPORTED_LOCATION "${fribidi_PREFIX}/lib/libfribidi.a"
)

file(MAKE_DIRECTORY "${fribidi_PREFIX}/include")

target_include_directories(
  fribidi
  INTERFACE "${fribidi_PREFIX}/include"
)
