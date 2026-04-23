/**
 * bare-mpv - Bare native addon for libmpv video playback
 * Enables universal codec support (AC3, DTS, etc.) on Pear desktop
 */

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>

#include <bare.h>
#include <js.h>

#include "vendor/mpv/include/mpv/client.h"
#include "vendor/mpv/include/mpv/render.h"
#include "vendor/mpv/include/mpv/render_gl.h"

// Handle wrapper for mpv_handle
typedef struct {
  mpv_handle *mpv;
} bare_mpv_handle_t;

// Type tag distinguishing SW and GL render context wrappers. Checked on
// entry to every renderFrame/renderUpdate/renderFree-style function so a
// context of one kind can never be fed to the other kind's entry points.
typedef enum {
  BARE_MPV_RENDER_TAG_SW = 1,
  BARE_MPV_RENDER_TAG_GL = 2,
} bare_mpv_render_tag_t;

// Handle wrapper for software mpv_render_context
typedef struct {
  bare_mpv_render_tag_t tag;  // always BARE_MPV_RENDER_TAG_SW
  mpv_render_context *ctx;
  int width;
  int height;
  uint8_t *buffer;  // RGBA pixel buffer
} bare_mpv_render_t;

// Handle wrapper for OpenGL mpv_render_context. get_proc_address_ref
// holds a persistent reference to the JS callback used by libmpv's
// get_proc_address trampoline. env is reused in the trampoline; this is
// safe because render_gl.h requires all mpv_render_* calls for a GL
// context to be made on the thread that owns the GL context — here, the
// same JS thread that created the context.
typedef struct {
  bare_mpv_render_tag_t tag;  // always BARE_MPV_RENDER_TAG_GL
  mpv_render_context *ctx;
  js_env_t *env;
  js_ref_t *get_proc_address_ref;
} bare_mpv_render_gl_t;

// Create mpv instance
static js_value_t *
bare_mpv_create(js_env_t *env, js_callback_info_t *info) {
  int err;

  mpv_handle *mpv = mpv_create();
  if (!mpv) {
    js_throw_error(env, NULL, "Failed to create mpv instance");
    return NULL;
  }

  // Create external arraybuffer to hold handle
  js_value_t *result;
  bare_mpv_handle_t *handle;
  err = js_create_arraybuffer(env, sizeof(bare_mpv_handle_t), (void**)&handle, &result);
  if (err != 0) {
    mpv_destroy(mpv);
    return NULL;
  }

  handle->mpv = mpv;
  return result;
}

// Initialize mpv instance
static js_value_t *
bare_mpv_initialize(js_env_t *env, js_callback_info_t *info) {
  int err;
  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  if (err != 0) return NULL;

  // Get handle from arraybuffer
  bare_mpv_handle_t *handle;
  size_t len;
  err = js_get_arraybuffer_info(env, argv[0], (void**)&handle, &len);
  if (err != 0) return NULL;

  // Set some default options for embedded playback
  mpv_set_option_string(handle->mpv, "vo", "libmpv");  // Use libmpv render API
  mpv_set_option_string(handle->mpv, "hwdec", "auto-safe"); // Prefer safe hardware decoding
  mpv_set_option_string(handle->mpv, "keep-open", "yes"); // Don't close on EOF

  int status = mpv_initialize(handle->mpv);

  js_value_t *result;
  js_create_int32(env, status, &result);
  return result;
}

// Destroy mpv instance
static js_value_t *
bare_mpv_destroy(js_env_t *env, js_callback_info_t *info) {
  int err;
  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  if (err != 0) return NULL;

  bare_mpv_handle_t *handle;
  size_t len;
  err = js_get_arraybuffer_info(env, argv[0], (void**)&handle, &len);
  if (err != 0) return NULL;

  if (handle->mpv) {
    mpv_terminate_destroy(handle->mpv);
    handle->mpv = NULL;
  }

  return NULL;
}

// Execute mpv command (e.g., loadfile, seek, etc.)
static js_value_t *
bare_mpv_command(js_env_t *env, js_callback_info_t *info) {
  int err;
  size_t argc = 2;
  js_value_t *argv[2];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  if (err != 0) return NULL;

  // Get handle
  bare_mpv_handle_t *handle;
  size_t len;
  err = js_get_arraybuffer_info(env, argv[0], (void**)&handle, &len);
  if (err != 0) return NULL;

  // Get command array
  uint32_t cmd_len;
  err = js_get_array_length(env, argv[1], &cmd_len);
  if (err != 0) return NULL;

  // Build command array for mpv
  std::vector<const char*> cmd_args(cmd_len + 1);
  std::vector<std::string> cmd_strings(cmd_len);

  for (uint32_t i = 0; i < cmd_len; i++) {
    js_value_t *elem;
    js_get_element(env, argv[1], i, &elem);

    size_t str_len;
    js_get_value_string_utf8(env, elem, NULL, 0, &str_len);
    cmd_strings[i].resize(str_len + 1);
    js_get_value_string_utf8(env, elem, (utf8_t*)&cmd_strings[i][0], str_len + 1, NULL);
    cmd_args[i] = cmd_strings[i].c_str();
  }
  cmd_args[cmd_len] = NULL;

  int status = mpv_command(handle->mpv, cmd_args.data());

  js_value_t *result;
  js_create_int32(env, status, &result);
  return result;
}

// Get property (returns double for numeric, string for string properties)
static js_value_t *
bare_mpv_get_property(js_env_t *env, js_callback_info_t *info) {
  int err;
  size_t argc = 2;
  js_value_t *argv[2];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  if (err != 0) return NULL;

  bare_mpv_handle_t *handle;
  size_t len;
  err = js_get_arraybuffer_info(env, argv[0], (void**)&handle, &len);
  if (err != 0) return NULL;

  // Get property name
  size_t name_len;
  js_get_value_string_utf8(env, argv[1], NULL, 0, &name_len);
  std::string name(name_len + 1, '\0');
  js_get_value_string_utf8(env, argv[1], (utf8_t*)&name[0], name_len + 1, NULL);

  // Try to get as double first (common for time-pos, duration, etc.)
  double value;
  int status = mpv_get_property(handle->mpv, name.c_str(), MPV_FORMAT_DOUBLE, &value);

  if (status >= 0) {
    js_value_t *result;
    js_create_double(env, value, &result);
    return result;
  }

  // Try as flag (bool)
  int flag;
  status = mpv_get_property(handle->mpv, name.c_str(), MPV_FORMAT_FLAG, &flag);
  if (status >= 0) {
    js_value_t *result;
    js_get_boolean(env, flag != 0, &result);
    return result;
  }

  // Try as string
  char *str = NULL;
  status = mpv_get_property(handle->mpv, name.c_str(), MPV_FORMAT_STRING, &str);
  if (status >= 0 && str) {
    js_value_t *result;
    js_create_string_utf8(env, (const utf8_t*)str, strlen(str), &result);
    mpv_free(str);
    return result;
  }

  // Return undefined if property not available
  js_value_t *undefined;
  js_get_undefined(env, &undefined);
  return undefined;
}

// Set property
static js_value_t *
bare_mpv_set_property(js_env_t *env, js_callback_info_t *info) {
  int err;
  size_t argc = 3;
  js_value_t *argv[3];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  if (err != 0) return NULL;

  bare_mpv_handle_t *handle;
  size_t len;
  err = js_get_arraybuffer_info(env, argv[0], (void**)&handle, &len);
  if (err != 0) return NULL;

  // Get property name
  size_t name_len;
  js_get_value_string_utf8(env, argv[1], NULL, 0, &name_len);
  std::string name(name_len + 1, '\0');
  js_get_value_string_utf8(env, argv[1], (utf8_t*)&name[0], name_len + 1, NULL);

  // Check value type and set accordingly
  js_value_type_t value_type;
  js_typeof(env, argv[2], &value_type);

  int status = -1;

  if (value_type == js_number) {
    double value;
    js_get_value_double(env, argv[2], &value);
    status = mpv_set_property(handle->mpv, name.c_str(), MPV_FORMAT_DOUBLE, &value);
  } else if (value_type == js_boolean) {
    bool value;
    js_get_value_bool(env, argv[2], &value);
    int flag = value ? 1 : 0;
    status = mpv_set_property(handle->mpv, name.c_str(), MPV_FORMAT_FLAG, &flag);
  } else if (value_type == js_string) {
    size_t str_len;
    js_get_value_string_utf8(env, argv[2], NULL, 0, &str_len);
    std::string str(str_len + 1, '\0');
    js_get_value_string_utf8(env, argv[2], (utf8_t*)&str[0], str_len + 1, NULL);
    status = mpv_set_property_string(handle->mpv, name.c_str(), str.c_str());
  }

  js_value_t *result;
  js_create_int32(env, status, &result);
  return result;
}

// Create software render context
static js_value_t *
bare_mpv_render_create(js_env_t *env, js_callback_info_t *info) {
  int err;
  size_t argc = 3;
  js_value_t *argv[3];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  if (err != 0) return NULL;

  bare_mpv_handle_t *mpv_handle;
  size_t len;
  err = js_get_arraybuffer_info(env, argv[0], (void**)&mpv_handle, &len);
  if (err != 0) return NULL;

  int32_t width, height;
  js_get_value_int32(env, argv[1], &width);
  js_get_value_int32(env, argv[2], &height);

  // Create render context with software renderer
  mpv_render_param params[] = {
    {MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_SW},
    {MPV_RENDER_PARAM_INVALID, NULL}
  };

  mpv_render_context *render_ctx = NULL;
  int status = mpv_render_context_create(&render_ctx, mpv_handle->mpv, params);

  if (status < 0) {
    js_throw_error(env, NULL, "Failed to create render context");
    return NULL;
  }

  // Create handle with buffer for rendered frames
  js_value_t *result;
  bare_mpv_render_t *handle;
  err = js_create_arraybuffer(env, sizeof(bare_mpv_render_t), (void**)&handle, &result);
  if (err != 0) {
    mpv_render_context_free(render_ctx);
    return NULL;
  }

  handle->tag = BARE_MPV_RENDER_TAG_SW;
  handle->ctx = render_ctx;
  handle->width = width;
  handle->height = height;
  handle->buffer = (uint8_t*)malloc(width * height * 4);  // RGBA

  return result;
}

// Render frame to buffer
static js_value_t *
bare_mpv_render_frame(js_env_t *env, js_callback_info_t *info) {
  int err;
  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  if (err != 0) return NULL;

  bare_mpv_render_t *handle;
  size_t len;
  err = js_get_arraybuffer_info(env, argv[0], (void**)&handle, &len);
  if (err != 0) return NULL;

  if (len < sizeof(bare_mpv_render_t) || handle->tag != BARE_MPV_RENDER_TAG_SW) {
    js_throw_error(env, NULL, "renderFrame expects a software render context");
    return NULL;
  }

  if (!handle->ctx || !handle->buffer) {
    js_value_t *null_val;
    js_get_null(env, &null_val);
    return null_val;
  }

  int w = handle->width;
  int h = handle->height;

  // Set up render parameters for software rendering
  int pitch = w * 4;  // RGBA stride
  int size[2] = {w, h};

  mpv_render_param render_params[] = {
    {MPV_RENDER_PARAM_SW_SIZE, size},
    {MPV_RENDER_PARAM_SW_FORMAT, (void*)"rgba"},
    {MPV_RENDER_PARAM_SW_STRIDE, &pitch},
    {MPV_RENDER_PARAM_SW_POINTER, handle->buffer},
    {MPV_RENDER_PARAM_INVALID, NULL}
  };

  int status = mpv_render_context_render(handle->ctx, render_params);

  if (status < 0) {
    js_value_t *null_val;
    js_get_null(env, &null_val);
    return null_val;
  }

  // Create Uint8Array view of the buffer
  size_t buffer_size = w * h * 4;
  js_value_t *arraybuffer;
  void *data;
  err = js_create_arraybuffer(env, buffer_size, &data, &arraybuffer);
  if (err != 0) return NULL;

  memcpy(data, handle->buffer, buffer_size);

  js_value_t *uint8array;
  err = js_create_typedarray(env, js_uint8array, buffer_size, arraybuffer, 0, &uint8array);
  if (err != 0) return arraybuffer;

  return uint8array;
}

// Free render context
static js_value_t *
bare_mpv_render_free(js_env_t *env, js_callback_info_t *info) {
  int err;
  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  if (err != 0) return NULL;

  bare_mpv_render_t *handle;
  size_t len;
  err = js_get_arraybuffer_info(env, argv[0], (void**)&handle, &len);
  if (err != 0) return NULL;

  if (len < sizeof(bare_mpv_render_t) || handle->tag != BARE_MPV_RENDER_TAG_SW) {
    js_throw_error(env, NULL, "renderFree expects a software render context");
    return NULL;
  }

  if (handle->ctx) {
    mpv_render_context_free(handle->ctx);
    handle->ctx = NULL;
  }

  if (handle->buffer) {
    free(handle->buffer);
    handle->buffer = NULL;
  }

  return NULL;
}

// Check if new frame is available
static js_value_t *
bare_mpv_render_update(js_env_t *env, js_callback_info_t *info) {
  int err;
  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  if (err != 0) return NULL;

  bare_mpv_render_t *handle;
  size_t len;
  err = js_get_arraybuffer_info(env, argv[0], (void**)&handle, &len);
  if (err != 0) return NULL;

  if (len < sizeof(bare_mpv_render_t) || handle->tag != BARE_MPV_RENDER_TAG_SW) {
    js_throw_error(env, NULL, "renderUpdate expects a software render context");
    return NULL;
  }

  uint64_t flags = mpv_render_context_update(handle->ctx);
  bool needs_render = (flags & MPV_RENDER_UPDATE_FRAME) != 0;

  js_value_t *result;
  js_get_boolean(env, needs_render, &result);
  return result;
}

// ---------------------------------------------------------------------------
// OpenGL render backend
// ---------------------------------------------------------------------------
//
// Coexists with the SW path. libmpv is driven via MPV_RENDER_API_TYPE_OPENGL
// and renders directly into an FBO owned by the caller. The caller is
// responsible for making its GL context current on the JS thread before
// calling any of the renderCreateGL / renderFrameGL / renderUpdateGL /
// renderFreeGL functions — we don't link against any GL library and all
// GL entry points are resolved lazily via the JS-provided get_proc_address
// callback.

// Trampoline invoked by libmpv whenever it needs to resolve a GL function
// pointer. Called on the same thread that called into mpv_render_*, which
// per render_gl.h is the thread where the caller's GL context is current —
// for us, always the JS thread. Synchronous js_call_function is therefore
// safe.
static void *
bare_mpv_gl_resolve_proc(js_env_t *env, js_ref_t *cb_ref, const char *name) {
  int err;

  js_value_t *fn;
  err = js_get_reference_value(env, cb_ref, &fn);
  if (err != 0) return NULL;

  js_value_t *global;
  err = js_get_global(env, &global);
  if (err != 0) return NULL;

  js_value_t *arg;
  err = js_create_string_utf8(env, (const utf8_t*)name, (size_t)-1, &arg);
  if (err != 0) return NULL;

  js_value_t *js_argv[1] = {arg};
  js_value_t *js_result;
  err = js_call_function(env, global, fn, 1, js_argv, &js_result);
  if (err != 0) return NULL;

  // Accept either BigInt or Number. Pointer-sized on 64-bit requires
  // BigInt; plain Number is safe on 32-bit and for addresses fitting in
  // 2^53 on 64-bit (which covers any real GL function pointer in
  // practice, but BigInt is still preferred on 64-bit).
  bool is_bi = false;
  err = js_is_bigint(env, js_result, &is_bi);
  if (err != 0) return NULL;

  if (is_bi) {
    int64_t v;
    bool lossless;
    err = js_get_value_bigint_int64(env, js_result, &v, &lossless);
    if (err != 0) return NULL;
    return (void*)(intptr_t)v;
  }

  js_value_type_t t;
  err = js_typeof(env, js_result, &t);
  if (err != 0) return NULL;

  if (t == js_number) {
    int64_t v;
    err = js_get_value_int64(env, js_result, &v);
    if (err != 0) return NULL;
    return (void*)(intptr_t)v;
  }

  // null / undefined / other types: return NULL so libmpv can detect
  // the missing function and fall back or fail gracefully.
  return NULL;
}

static void *
bare_mpv_gl_get_proc_address(void *ctx, const char *name) {
  bare_mpv_render_gl_t *handle = (bare_mpv_render_gl_t*)ctx;
  if (!handle || !handle->env || !handle->get_proc_address_ref) return NULL;

  js_env_t *env = handle->env;

  js_handle_scope_t *scope;
  if (js_open_handle_scope(env, &scope) != 0) return NULL;

  void *result_ptr = bare_mpv_gl_resolve_proc(env, handle->get_proc_address_ref, name);

  js_close_handle_scope(env, scope);
  return result_ptr;
}

// Create OpenGL render context
static js_value_t *
bare_mpv_render_create_gl(js_env_t *env, js_callback_info_t *info) {
  int err;
  size_t argc = 2;
  js_value_t *argv[2];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  if (err != 0) return NULL;

  if (argc < 2) {
    js_throw_error(env, NULL, "renderCreateGL requires mpv handle and getProcAddress callback");
    return NULL;
  }

  bare_mpv_handle_t *mpv_handle;
  size_t len;
  err = js_get_arraybuffer_info(env, argv[0], (void**)&mpv_handle, &len);
  if (err != 0) return NULL;

  // Validate the callback is a function before taking a reference
  js_value_type_t cb_type;
  err = js_typeof(env, argv[1], &cb_type);
  if (err != 0) return NULL;
  if (cb_type != js_function) {
    js_throw_error(env, NULL, "renderCreateGL: getProcAddress must be a function");
    return NULL;
  }

  // Allocate wrapper first so the trampoline has a stable ctx pointer
  // even across the js_create_reference call below.
  js_value_t *result;
  bare_mpv_render_gl_t *handle;
  err = js_create_arraybuffer(env, sizeof(bare_mpv_render_gl_t), (void**)&handle, &result);
  if (err != 0) return NULL;

  handle->tag = BARE_MPV_RENDER_TAG_GL;
  handle->ctx = NULL;
  handle->env = env;
  handle->get_proc_address_ref = NULL;

  err = js_create_reference(env, argv[1], 1, &handle->get_proc_address_ref);
  if (err != 0) {
    js_throw_error(env, NULL, "renderCreateGL: failed to retain getProcAddress callback");
    return NULL;
  }

  mpv_opengl_init_params gl_init;
  gl_init.get_proc_address = bare_mpv_gl_get_proc_address;
  gl_init.get_proc_address_ctx = handle;

  mpv_render_param params[] = {
    {MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL},
    {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init},
    {MPV_RENDER_PARAM_INVALID, NULL}
  };

  mpv_render_context *render_ctx = NULL;
  int status = mpv_render_context_create(&render_ctx, mpv_handle->mpv, params);

  if (status < 0) {
    js_delete_reference(env, handle->get_proc_address_ref);
    handle->get_proc_address_ref = NULL;
    js_throw_error(env, NULL, "Failed to create GL render context");
    return NULL;
  }

  handle->ctx = render_ctx;
  return result;
}

// Render frame to caller-owned GL FBO
static js_value_t *
bare_mpv_render_frame_gl(js_env_t *env, js_callback_info_t *info) {
  int err;
  size_t argc = 6;
  js_value_t *argv[6];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  if (err != 0) return NULL;

  if (argc < 5) {
    js_throw_error(env, NULL, "renderFrameGL requires ctx, fbo, width, height, internalFormat, flipY");
    return NULL;
  }

  bare_mpv_render_gl_t *handle;
  size_t len;
  err = js_get_arraybuffer_info(env, argv[0], (void**)&handle, &len);
  if (err != 0) return NULL;

  if (len < sizeof(bare_mpv_render_gl_t) || handle->tag != BARE_MPV_RENDER_TAG_GL) {
    js_throw_error(env, NULL, "renderFrameGL expects a GL render context");
    return NULL;
  }

  if (!handle->ctx) {
    js_value_t *result;
    js_create_int32(env, -1, &result);
    return result;
  }

  int32_t fbo, width, height, internal_format;
  bool flip_y = true;
  js_get_value_int32(env, argv[1], &fbo);
  js_get_value_int32(env, argv[2], &width);
  js_get_value_int32(env, argv[3], &height);
  js_get_value_int32(env, argv[4], &internal_format);
  if (argc >= 6) {
    js_get_value_bool(env, argv[5], &flip_y);
  }

  mpv_opengl_fbo gl_fbo;
  gl_fbo.fbo = fbo;
  gl_fbo.w = width;
  gl_fbo.h = height;
  gl_fbo.internal_format = internal_format;

  int flip = flip_y ? 1 : 0;

  mpv_render_param render_params[] = {
    {MPV_RENDER_PARAM_OPENGL_FBO, &gl_fbo},
    {MPV_RENDER_PARAM_FLIP_Y, &flip},
    {MPV_RENDER_PARAM_INVALID, NULL}
  };

  int status = mpv_render_context_render(handle->ctx, render_params);

  js_value_t *result;
  js_create_int32(env, status, &result);
  return result;
}

// Free GL render context
static js_value_t *
bare_mpv_render_free_gl(js_env_t *env, js_callback_info_t *info) {
  int err;
  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  if (err != 0) return NULL;

  bare_mpv_render_gl_t *handle;
  size_t len;
  err = js_get_arraybuffer_info(env, argv[0], (void**)&handle, &len);
  if (err != 0) return NULL;

  if (len < sizeof(bare_mpv_render_gl_t) || handle->tag != BARE_MPV_RENDER_TAG_GL) {
    js_throw_error(env, NULL, "renderFreeGL expects a GL render context");
    return NULL;
  }

  if (handle->ctx) {
    mpv_render_context_free(handle->ctx);
    handle->ctx = NULL;
  }

  if (handle->get_proc_address_ref) {
    js_delete_reference(env, handle->get_proc_address_ref);
    handle->get_proc_address_ref = NULL;
  }

  return NULL;
}

// Check if a new GL frame is available
static js_value_t *
bare_mpv_render_update_gl(js_env_t *env, js_callback_info_t *info) {
  int err;
  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  if (err != 0) return NULL;

  bare_mpv_render_gl_t *handle;
  size_t len;
  err = js_get_arraybuffer_info(env, argv[0], (void**)&handle, &len);
  if (err != 0) return NULL;

  if (len < sizeof(bare_mpv_render_gl_t) || handle->tag != BARE_MPV_RENDER_TAG_GL) {
    js_throw_error(env, NULL, "renderUpdateGL expects a GL render context");
    return NULL;
  }

  uint64_t flags = mpv_render_context_update(handle->ctx);
  bool needs_render = (flags & MPV_RENDER_UPDATE_FRAME) != 0;

  js_value_t *result;
  js_get_boolean(env, needs_render, &result);
  return result;
}

// Drain pending libmpv client events so playback state/rendering can advance.
static js_value_t *
bare_mpv_process_events(js_env_t *env, js_callback_info_t *info) {
  int err;
  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  if (err != 0) return NULL;

  bare_mpv_handle_t *handle;
  size_t len;
  err = js_get_arraybuffer_info(env, argv[0], (void**)&handle, &len);
  if (err != 0) return NULL;

  int32_t processed = 0;

  while (handle->mpv) {
    mpv_event *event = mpv_wait_event(handle->mpv, 0);
    if (!event || event->event_id == MPV_EVENT_NONE) break;
    processed++;
  }

  js_value_t *result;
  js_create_int32(env, processed, &result);
  return result;
}

// Module exports
static js_value_t *
bare_mpv_exports(js_env_t *env, js_value_t *exports) {
  int err;

#define EXPORT_FUNCTION(name, fn) \
  do { \
    js_value_t *func; \
    err = js_create_function(env, #name, -1, fn, NULL, &func); \
    if (err == 0) js_set_named_property(env, exports, #name, func); \
  } while(0)

  EXPORT_FUNCTION(create, bare_mpv_create);
  EXPORT_FUNCTION(initialize, bare_mpv_initialize);
  EXPORT_FUNCTION(destroy, bare_mpv_destroy);
  EXPORT_FUNCTION(command, bare_mpv_command);
  EXPORT_FUNCTION(getProperty, bare_mpv_get_property);
  EXPORT_FUNCTION(setProperty, bare_mpv_set_property);
  EXPORT_FUNCTION(renderCreate, bare_mpv_render_create);
  EXPORT_FUNCTION(renderFrame, bare_mpv_render_frame);
  EXPORT_FUNCTION(renderFree, bare_mpv_render_free);
  EXPORT_FUNCTION(renderUpdate, bare_mpv_render_update);
  EXPORT_FUNCTION(renderCreateGL, bare_mpv_render_create_gl);
  EXPORT_FUNCTION(renderFrameGL, bare_mpv_render_frame_gl);
  EXPORT_FUNCTION(renderFreeGL, bare_mpv_render_free_gl);
  EXPORT_FUNCTION(renderUpdateGL, bare_mpv_render_update_gl);
  EXPORT_FUNCTION(processEvents, bare_mpv_process_events);

#undef EXPORT_FUNCTION

  return exports;
}

BARE_MODULE(bare_mpv, bare_mpv_exports)
