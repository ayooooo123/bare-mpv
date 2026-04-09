include_guard(GLOBAL)

set(freetype_args
  -DBUILD_SHARED_LIBS=OFF
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  -DFT_WITH_BROTLI=OFF
  -DFT_WITH_HARFBUZZ=OFF
  -DFT_WITH_ZLIB=OFF
  -DFT_WITH_BZIP2=OFF
  -DFT_WITH_PNG=OFF
)

# Hide all symbols — statically linked into .bare, none should be public
if(NOT WIN32)
  list(APPEND freetype_args "-DCMAKE_C_FLAGS=-fvisibility=hidden")
endif()

declare_port(
  "https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.xz"
  freetype
  CMAKE
  BYPRODUCTS
    lib/libfreetype.a
  ARGS
    ${freetype_args}
)

add_library(freetype STATIC IMPORTED GLOBAL)

add_dependencies(freetype ${freetype})

if(WIN32)
  set(freetype_lib "${freetype_PREFIX}/lib/freetype.lib")
else()
  set(freetype_lib "${freetype_PREFIX}/lib/libfreetype.a")
endif()

set_target_properties(
  freetype
  PROPERTIES
  IMPORTED_LOCATION "${freetype_lib}"
)

file(MAKE_DIRECTORY "${freetype_PREFIX}/include")
file(MAKE_DIRECTORY "${freetype_PREFIX}/include/freetype2")

target_include_directories(
  freetype
  INTERFACE
    "${freetype_PREFIX}/include"
    "${freetype_PREFIX}/include/freetype2"
)
