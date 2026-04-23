/**
 * bare-mpv - High-level JavaScript API for libmpv video playback
 * Enables universal codec support (AC3, DTS, etc.) on Pear desktop
 */

const binding = require('./binding')

class MpvPlayer {
  constructor() {
    this._handle = binding.create()
    this._renderCtx = null
    this._renderCtxGL = null
    this._width = 0
    this._height = 0
    this._initialized = false
  }

  _processEvents() {
    if (!this._handle) return 0
    if (typeof binding.processEvents !== 'function') return 0
    return binding.processEvents(this._handle)
  }

  /**
   * Initialize the mpv player
   * @returns {number} Status code (0 = success)
   */
  initialize() {
    if (this._initialized) return 0
    const status = binding.initialize(this._handle)
    this._initialized = status === 0
    return status
  }

  /**
   * Load and play a video file
   * @param {string} url - URL or path to the video file
   */
  loadFile(url) {
    if (!this._initialized) this.initialize()
    const status = binding.command(this._handle, ['loadfile', url])
    this._processEvents()
    return status
  }

  /**
   * Start/resume playback
   */
  play() {
    const status = binding.setProperty(this._handle, 'pause', false)
    this._processEvents()
    return status
  }

  /**
   * Pause playback
   */
  pause() {
    const status = binding.setProperty(this._handle, 'pause', true)
    this._processEvents()
    return status
  }

  /**
   * Stop playback
   */
  stop() {
    const status = binding.command(this._handle, ['stop'])
    this._processEvents()
    return status
  }

  /**
   * Seek to absolute position
   * @param {number} seconds - Position in seconds
   */
  seek(seconds) {
    const status = binding.command(this._handle, ['seek', String(seconds), 'absolute'])
    this._processEvents()
    return status
  }

  /**
   * Seek relative to current position
   * @param {number} seconds - Offset in seconds (positive = forward, negative = backward)
   */
  seekRelative(seconds) {
    const status = binding.command(this._handle, ['seek', String(seconds), 'relative'])
    this._processEvents()
    return status
  }

  /**
   * Get current playback position in seconds
   */
  get currentTime() {
    this._processEvents()
    const val = binding.getProperty(this._handle, 'time-pos')
    return typeof val === 'number' ? val : 0
  }

  /**
   * Get video duration in seconds
   */
  get duration() {
    this._processEvents()
    const val = binding.getProperty(this._handle, 'duration')
    return typeof val === 'number' ? val : 0
  }

  /**
   * Check if playback is paused
   */
  get paused() {
    this._processEvents()
    return binding.getProperty(this._handle, 'pause') === true
  }

  /**
   * Get/set volume (0-100)
   */
  get volume() {
    this._processEvents()
    const val = binding.getProperty(this._handle, 'volume')
    return typeof val === 'number' ? val : 100
  }

  set volume(value) {
    binding.setProperty(this._handle, 'volume', Math.max(0, Math.min(100, value)))
  }

  /**
   * Get/set mute state
   */
  get muted() {
    this._processEvents()
    return binding.getProperty(this._handle, 'mute') === true
  }

  set muted(value) {
    binding.setProperty(this._handle, 'mute', !!value)
  }

  /**
   * Get video width
   */
  get videoWidth() {
    this._processEvents()
    const val = binding.getProperty(this._handle, 'width')
    return typeof val === 'number' ? val : 0
  }

  /**
   * Get video height
   */
  get videoHeight() {
    this._processEvents()
    const val = binding.getProperty(this._handle, 'height')
    return typeof val === 'number' ? val : 0
  }

  /**
   * Check if video has ended
   */
  get ended() {
    this._processEvents()
    return binding.getProperty(this._handle, 'eof-reached') === true
  }

  /**
   * Initialize software renderer at specified dimensions
   * @param {number} width - Render width in pixels
   * @param {number} height - Render height in pixels
   */
  initRender(width, height) {
    if (this._renderCtxGL) {
      console.warn('bare-mpv: initRender called while a GL render context exists; freeing the GL context first')
      binding.renderFreeGL(this._renderCtxGL)
      this._renderCtxGL = null
    }
    if (this._renderCtx) {
      binding.renderFree(this._renderCtx)
      this._renderCtx = null
    }
    this._width = width
    this._height = height
    this._renderCtx = binding.renderCreate(this._handle, width, height)
    return this._renderCtx !== null
  }

  /**
   * Check if a new frame is available for rendering
   * @returns {boolean} True if frame should be re-rendered
   */
  needsRender() {
    if (!this._renderCtx) return false
    this._processEvents()
    return binding.renderUpdate(this._renderCtx)
  }

  /**
   * Render current frame to RGBA pixel buffer
   * @returns {Uint8Array|null} RGBA pixel data (width * height * 4 bytes)
   */
  renderFrame() {
    if (!this._renderCtx) return null
    this._processEvents()
    return binding.renderFrame(this._renderCtx)
  }

  /**
   * Initialize the hardware-accelerated OpenGL renderer.
   *
   * The caller is responsible for making their GL context current on the
   * JS thread before calling this, and for keeping it current for every
   * subsequent renderFrameGL()/needsRenderGL() call. mpv resolves GL
   * entry points lazily via the provided callback.
   *
   * @param {(name: string) => number | bigint} getProcAddress - Returns
   *   the address of a GL function as a pointer-valued integer (number
   *   or bigint). Prefer bigint on 64-bit targets.
   * @returns {boolean} True if the context was created successfully.
   */
  initRenderGL(getProcAddress) {
    if (typeof getProcAddress !== 'function') {
      throw new TypeError('initRenderGL: getProcAddress must be a function')
    }
    if (this._renderCtx) {
      console.warn('bare-mpv: initRenderGL called while a software render context exists; freeing the software context first')
      binding.renderFree(this._renderCtx)
      this._renderCtx = null
    }
    if (this._renderCtxGL) {
      binding.renderFreeGL(this._renderCtxGL)
      this._renderCtxGL = null
    }
    this._renderCtxGL = binding.renderCreateGL(this._handle, getProcAddress)
    return this._renderCtxGL !== null
  }

  /**
   * Render current frame into a caller-owned OpenGL FBO.
   *
   * @param {object} opts
   * @param {number} opts.fbo - Target FBO name (0 = default framebuffer).
   * @param {number} opts.width - FBO width in pixels.
   * @param {number} opts.height - FBO height in pixels.
   * @param {number} [opts.internalFormat=0x8058] - GL internal format
   *   (default GL_RGBA8). Pass 0 if unknown.
   * @param {boolean} [opts.flipY=true] - Flip Y axis (needed for the
   *   default framebuffer, which has a flipped coordinate system).
   * @returns {number} mpv status code (0 = success, <0 = error).
   */
  renderFrameGL({ fbo, width, height, internalFormat = 0x8058, flipY = true }) {
    if (!this._renderCtxGL) return -1
    this._processEvents()
    return binding.renderFrameGL(this._renderCtxGL, fbo | 0, width | 0, height | 0, internalFormat | 0, !!flipY)
  }

  /**
   * Check if a new GL frame is available for rendering.
   * @returns {boolean}
   */
  needsRenderGL() {
    if (!this._renderCtxGL) return false
    this._processEvents()
    return binding.renderUpdateGL(this._renderCtxGL)
  }

  /**
   * Get the render dimensions
   */
  get renderWidth() {
    return this._width
  }

  get renderHeight() {
    return this._height
  }

  /**
   * Destroy the player and free resources
   */
  destroy() {
    if (this._renderCtx) {
      binding.renderFree(this._renderCtx)
      this._renderCtx = null
    }
    if (this._renderCtxGL) {
      binding.renderFreeGL(this._renderCtxGL)
      this._renderCtxGL = null
    }
    if (this._handle) {
      binding.destroy(this._handle)
      this._handle = null
    }
    this._initialized = false
  }

  /**
   * Set an mpv property
   * @param {string} name - Property name
   * @param {*} value - Property value
   */
  setProperty(name, value) {
    const status = binding.setProperty(this._handle, name, value)
    this._processEvents()
    return status
  }

  /**
   * Get an mpv property
   * @param {string} name - Property name
   * @returns {*} Property value
   */
  getProperty(name) {
    this._processEvents()
    return binding.getProperty(this._handle, name)
  }

  /**
   * Execute an mpv command
   * @param {string[]} args - Command arguments
   */
  command(args) {
    const status = binding.command(this._handle, args)
    this._processEvents()
    return status
  }
}

module.exports = { MpvPlayer }
