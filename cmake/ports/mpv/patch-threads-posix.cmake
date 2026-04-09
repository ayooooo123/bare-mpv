# Patch mpv's threads-posix.h to handle iOS pthread_setname_np (1-arg, not 2-arg)
# iOS uses pthread_setname_np(name) but HAVE_GLIBC_THREAD_NAME gets incorrectly
# set to true in cross-build meson detection, causing a compile error.

if(NOT EXISTS "${FILE}")
  message(FATAL_ERROR "File not found: ${FILE}")
endif()

file(READ "${FILE}" content)

# Check if already patched
string(FIND "${content}" "!defined(__APPLE__)" already_patched)
if(already_patched GREATER -1)
  message(STATUS "threads-posix.h already patched, skipping")
  return()
endif()

# Apply the patch: replace the HAVE_GLIBC_THREAD_NAME guard
string(REPLACE
  "#if HAVE_GLIBC_THREAD_NAME\n    if (pthread_setname_np(pthread_self(), name) == ERANGE) {"
  "#if HAVE_GLIBC_THREAD_NAME && !defined(__APPLE__)\n    if (pthread_setname_np(pthread_self(), name) == ERANGE) {"
  content "${content}"
)

# Add the Apple/mac-thread-name fallback branch after the glibc block
string(REPLACE
  "    }\n#elif HAVE_BSD_THREAD_NAME\n    pthread_set_name_np(pthread_self(), name);\n#elif HAVE_MAC_THREAD_NAME\n    pthread_setname_np(name);"
  "    }\n#elif (HAVE_GLIBC_THREAD_NAME && defined(__APPLE__)) || HAVE_MAC_THREAD_NAME\n    pthread_setname_np(name);\n#elif HAVE_BSD_THREAD_NAME\n    pthread_set_name_np(pthread_self(), name);"
  content "${content}"
)

file(WRITE "${FILE}" "${content}")
message(STATUS "Patched threads-posix.h for iOS pthread_setname_np compatibility")
