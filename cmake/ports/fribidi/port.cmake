include_guard(GLOBAL)

set(env)
set(path)

if(CMAKE_HOST_WIN32)
  find_path(
    msys2
    NAMES msys2.exe
    PATHS "C:/tools/msys64"
    REQUIRED
  )

  find_program(
    pkg-config
    NAMES pkg-config
    PATHS "${msys2}/usr/bin"
    REQUIRED
    NO_DEFAULT_PATH
  )

  list(APPEND path "${msys2}/usr/bin")
else()
  find_program(
    pkg-config
    NAMES pkg-config
    REQUIRED
  )
endif()

foreach(part "$ENV{PATH}")
  cmake_path(NORMAL_PATH part)
  list(APPEND path "${part}")
endforeach()

list(REMOVE_DUPLICATES path)

if(CMAKE_HOST_WIN32)
  list(TRANSFORM path REPLACE "([A-Z]):" "/\\1")
endif()

list(JOIN path ":" path)

list(APPEND env
  "PATH=${path}"
  "PKG_CONFIG=${pkg-config}"
)

declare_port(
  "https://github.com/fribidi/fribidi/releases/download/v1.0.16/fribidi-1.0.16.tar.xz"
  fribidi
  AUTOTOOLS
  BYPRODUCTS
    lib/libfribidi.a
  ARGS
    --disable-shared
    --enable-static
    --with-pic
    --disable-documentation
  ENV ${env}
)

add_library(fribidi STATIC IMPORTED GLOBAL)

add_dependencies(fribidi ${fribidi})

set(fribidi_lib "${fribidi_PREFIX}/lib/libfribidi.a")

set_target_properties(
  fribidi
  PROPERTIES
  IMPORTED_LOCATION "${fribidi_lib}"
)

file(MAKE_DIRECTORY "${fribidi_PREFIX}/include")

target_include_directories(
  fribidi
  INTERFACE "${fribidi_PREFIX}/include"
)
