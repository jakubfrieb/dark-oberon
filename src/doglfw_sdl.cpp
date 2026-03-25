/*
 * GLFW 2.x subset implemented with SDL2 (see doglfw_sdl.h).
 */

#include "doglfw_sdl.h"

#if !HEADLESS

static SDL_Window *g_win = NULL;
static SDL_GLContext g_ctx = NULL;

static GLFWwindowsizefun cb_size = NULL;
static GLFWkeyfun cb_key = NULL;
static GLFWmousebuttonfun cb_mouse_btn = NULL;
static GLFWmouseposfun cb_mouse_pos = NULL;
static GLFWmousewheelfun cb_mouse_wheel = NULL;
static GLFWwindowrefreshfun cb_refresh = NULL;

static int g_wheel_pos = 0;

/** Keep pointer inside the game window so multi-monitor moves do not lose the GL view. */
static void set_window_mouse_grab(SDL_bool on)
{
  if (!g_win)
    return;
#if SDL_VERSION_ATLEAST(2, 0, 18)
  SDL_SetWindowMouseGrab(g_win, on);
#else
  SDL_SetWindowGrab(g_win, on);
#endif
}

static int glfw_button_to_sdl(int glfw_b)
{
  switch (glfw_b) {
  case GLFW_MOUSE_BUTTON_LEFT: return SDL_BUTTON_LEFT;
  case GLFW_MOUSE_BUTTON_RIGHT: return SDL_BUTTON_RIGHT;
  case GLFW_MOUSE_BUTTON_MIDDLE: return SDL_BUTTON_MIDDLE;
  case GLFW_MOUSE_BUTTON_4: return SDL_BUTTON_X1;
  case GLFW_MOUSE_BUTTON_5: return SDL_BUTTON_X2;
  default: return 0;
  }
}

static int sdl_mouse_button_to_glfw(Uint8 btn)
{
  switch (btn) {
  case SDL_BUTTON_LEFT: return GLFW_MOUSE_BUTTON_LEFT;
  case SDL_BUTTON_RIGHT: return GLFW_MOUSE_BUTTON_RIGHT;
  case SDL_BUTTON_MIDDLE: return GLFW_MOUSE_BUTTON_MIDDLE;
  case SDL_BUTTON_X1: return GLFW_MOUSE_BUTTON_4;
  case SDL_BUTTON_X2: return GLFW_MOUSE_BUTTON_5;
  default: return (int)btn - 1;
  }
}

static int map_keysym_to_glfw(const SDL_Keysym *k)
{
  SDL_Scancode sc = k->scancode;
  SDL_Keycode sym = k->sym;

  switch (sc) {
  case SDL_SCANCODE_ESCAPE: return GLFW_KEY_ESC;
  case SDL_SCANCODE_UP: return GLFW_KEY_UP;
  case SDL_SCANCODE_DOWN: return GLFW_KEY_DOWN;
  case SDL_SCANCODE_LEFT: return GLFW_KEY_LEFT;
  case SDL_SCANCODE_RIGHT: return GLFW_KEY_RIGHT;
  case SDL_SCANCODE_LSHIFT: return GLFW_KEY_LSHIFT;
  case SDL_SCANCODE_RSHIFT: return GLFW_KEY_RSHIFT;
  case SDL_SCANCODE_LCTRL: return GLFW_KEY_LCTRL;
  case SDL_SCANCODE_RCTRL: return GLFW_KEY_RCTRL;
  case SDL_SCANCODE_LALT: return GLFW_KEY_LALT;
  case SDL_SCANCODE_RALT: return GLFW_KEY_RALT;
  case SDL_SCANCODE_TAB: return GLFW_KEY_TAB;
  case SDL_SCANCODE_RETURN: return GLFW_KEY_ENTER;
  case SDL_SCANCODE_BACKSPACE: return GLFW_KEY_BACKSPACE;
  case SDL_SCANCODE_INSERT: return GLFW_KEY_INSERT;
  case SDL_SCANCODE_DELETE: return GLFW_KEY_DEL;
  case SDL_SCANCODE_PAGEUP: return GLFW_KEY_PAGEUP;
  case SDL_SCANCODE_PAGEDOWN: return GLFW_KEY_PAGEDOWN;
  case SDL_SCANCODE_HOME: return GLFW_KEY_HOME;
  case SDL_SCANCODE_END: return GLFW_KEY_END;
  case SDL_SCANCODE_KP_0: return GLFW_KEY_KP_0;
  case SDL_SCANCODE_KP_1: return GLFW_KEY_KP_1;
  case SDL_SCANCODE_KP_2: return GLFW_KEY_KP_2;
  case SDL_SCANCODE_KP_3: return GLFW_KEY_KP_3;
  case SDL_SCANCODE_KP_4: return GLFW_KEY_KP_4;
  case SDL_SCANCODE_KP_5: return GLFW_KEY_KP_5;
  case SDL_SCANCODE_KP_6: return GLFW_KEY_KP_6;
  case SDL_SCANCODE_KP_7: return GLFW_KEY_KP_7;
  case SDL_SCANCODE_KP_8: return GLFW_KEY_KP_8;
  case SDL_SCANCODE_KP_9: return GLFW_KEY_KP_9;
  case SDL_SCANCODE_KP_DIVIDE: return GLFW_KEY_KP_DIVIDE;
  case SDL_SCANCODE_KP_MULTIPLY: return GLFW_KEY_KP_MULTIPLY;
  case SDL_SCANCODE_KP_MINUS: return GLFW_KEY_KP_SUBTRACT;
  case SDL_SCANCODE_KP_PLUS: return GLFW_KEY_KP_ADD;
  case SDL_SCANCODE_KP_DECIMAL: return GLFW_KEY_KP_DECIMAL;
  case SDL_SCANCODE_KP_ENTER: return GLFW_KEY_KP_ENTER;
  case SDL_SCANCODE_F1: return GLFW_KEY_F1;
  case SDL_SCANCODE_F2: return GLFW_KEY_F2;
  case SDL_SCANCODE_F3: return GLFW_KEY_F3;
  case SDL_SCANCODE_F4: return GLFW_KEY_F4;
  case SDL_SCANCODE_F5: return GLFW_KEY_F5;
  case SDL_SCANCODE_F6: return GLFW_KEY_F6;
  case SDL_SCANCODE_F7: return GLFW_KEY_F7;
  case SDL_SCANCODE_F8: return GLFW_KEY_F8;
  case SDL_SCANCODE_F9: return GLFW_KEY_F9;
  case SDL_SCANCODE_F10: return GLFW_KEY_F10;
  case SDL_SCANCODE_F11: return GLFW_KEY_F11;
  case SDL_SCANCODE_F12: return GLFW_KEY_F12;
  default: break;
  }

  if (sym >= SDLK_a && sym <= SDLK_z)
    return 'A' + (sym - SDLK_a);
  if (sym >= SDLK_0 && sym <= SDLK_9)
    return '0' + (sym - SDLK_0);

  if (sym != SDLK_UNKNOWN && sym > 0 && sym < 256)
    return (int)sym;

  return GLFW_KEY_UNKNOWN;
}

static SDL_Scancode glfw_key_to_scancode(int key)
{
  if (key >= 'A' && key <= 'Z')
    return (SDL_Scancode)(SDL_SCANCODE_A + (key - 'A'));
  if (key >= '0' && key <= '9')
    return (SDL_Scancode)(SDL_SCANCODE_0 + (key - '0'));

  switch (key) {
  case GLFW_KEY_ESC: return SDL_SCANCODE_ESCAPE;
  case GLFW_KEY_UP: return SDL_SCANCODE_UP;
  case GLFW_KEY_DOWN: return SDL_SCANCODE_DOWN;
  case GLFW_KEY_LEFT: return SDL_SCANCODE_LEFT;
  case GLFW_KEY_RIGHT: return SDL_SCANCODE_RIGHT;
  case GLFW_KEY_LSHIFT: return SDL_SCANCODE_LSHIFT;
  case GLFW_KEY_RSHIFT: return SDL_SCANCODE_RSHIFT;
  case GLFW_KEY_LCTRL: return SDL_SCANCODE_LCTRL;
  case GLFW_KEY_RCTRL: return SDL_SCANCODE_RCTRL;
  case GLFW_KEY_LALT: return SDL_SCANCODE_LALT;
  case GLFW_KEY_RALT: return SDL_SCANCODE_RALT;
  case GLFW_KEY_TAB: return SDL_SCANCODE_TAB;
  case GLFW_KEY_ENTER: return SDL_SCANCODE_RETURN;
  case GLFW_KEY_BACKSPACE: return SDL_SCANCODE_BACKSPACE;
  case GLFW_KEY_INSERT: return SDL_SCANCODE_INSERT;
  case GLFW_KEY_DEL: return SDL_SCANCODE_DELETE;
  case GLFW_KEY_PAGEUP: return SDL_SCANCODE_PAGEUP;
  case GLFW_KEY_PAGEDOWN: return SDL_SCANCODE_PAGEDOWN;
  case GLFW_KEY_HOME: return SDL_SCANCODE_HOME;
  case GLFW_KEY_END: return SDL_SCANCODE_END;
  case GLFW_KEY_KP_0: return SDL_SCANCODE_KP_0;
  case GLFW_KEY_KP_1: return SDL_SCANCODE_KP_1;
  case GLFW_KEY_KP_2: return SDL_SCANCODE_KP_2;
  case GLFW_KEY_KP_3: return SDL_SCANCODE_KP_3;
  case GLFW_KEY_KP_4: return SDL_SCANCODE_KP_4;
  case GLFW_KEY_KP_5: return SDL_SCANCODE_KP_5;
  case GLFW_KEY_KP_6: return SDL_SCANCODE_KP_6;
  case GLFW_KEY_KP_7: return SDL_SCANCODE_KP_7;
  case GLFW_KEY_KP_8: return SDL_SCANCODE_KP_8;
  case GLFW_KEY_KP_9: return SDL_SCANCODE_KP_9;
  case GLFW_KEY_KP_DIVIDE: return SDL_SCANCODE_KP_DIVIDE;
  case GLFW_KEY_KP_MULTIPLY: return SDL_SCANCODE_KP_MULTIPLY;
  case GLFW_KEY_KP_SUBTRACT: return SDL_SCANCODE_KP_MINUS;
  case GLFW_KEY_KP_ADD: return SDL_SCANCODE_KP_PLUS;
  case GLFW_KEY_KP_DECIMAL: return SDL_SCANCODE_KP_DECIMAL;
  case GLFW_KEY_KP_ENTER: return SDL_SCANCODE_KP_ENTER;
  case GLFW_KEY_F1: return SDL_SCANCODE_F1;
  case GLFW_KEY_F2: return SDL_SCANCODE_F2;
  case GLFW_KEY_F3: return SDL_SCANCODE_F3;
  case GLFW_KEY_F4: return SDL_SCANCODE_F4;
  case GLFW_KEY_F5: return SDL_SCANCODE_F5;
  case GLFW_KEY_F6: return SDL_SCANCODE_F6;
  case GLFW_KEY_F7: return SDL_SCANCODE_F7;
  case GLFW_KEY_F8: return SDL_SCANCODE_F8;
  case GLFW_KEY_F9: return SDL_SCANCODE_F9;
  case GLFW_KEY_F10: return SDL_SCANCODE_F10;
  case GLFW_KEY_F11: return SDL_SCANCODE_F11;
  case GLFW_KEY_F12: return SDL_SCANCODE_F12;
  case GLFW_KEY_SPACE: return SDL_SCANCODE_SPACE;
  case '`': return SDL_SCANCODE_GRAVE;
  case '-': return SDL_SCANCODE_MINUS;
  case '=': return SDL_SCANCODE_EQUALS;
  case '[': return SDL_SCANCODE_LEFTBRACKET;
  case ']': return SDL_SCANCODE_RIGHTBRACKET;
  case '\\': return SDL_SCANCODE_BACKSLASH;
  case ';': return SDL_SCANCODE_SEMICOLON;
  case '\'': return SDL_SCANCODE_APOSTROPHE;
  case ',': return SDL_SCANCODE_COMMA;
  case '.': return SDL_SCANCODE_PERIOD;
  case '/': return SDL_SCANCODE_SLASH;
  default: return SDL_SCANCODE_UNKNOWN;
  }
}

static void dispatch_window_size(int w, int h)
{
  if (cb_size && w > 0 && h > 0)
    cb_size(w, h);
}

static void process_event(const SDL_Event *e)
{
  switch (e->type) {
  case SDL_QUIT:
    if (g_win) {
      set_window_mouse_grab(SDL_FALSE);
      SDL_DestroyWindow(g_win);
    }
    g_win = NULL;
    if (g_ctx) {
      SDL_GL_DeleteContext(g_ctx);
      g_ctx = NULL;
    }
    break;

  case SDL_WINDOWEVENT:
    if (!g_win || e->window.windowID != SDL_GetWindowID(g_win))
      break;
    switch (e->window.event) {
    case SDL_WINDOWEVENT_CLOSE:
      if (g_ctx) {
        SDL_GL_DeleteContext(g_ctx);
        g_ctx = NULL;
      }
      set_window_mouse_grab(SDL_FALSE);
      SDL_DestroyWindow(g_win);
      g_win = NULL;
      break;
    case SDL_WINDOWEVENT_SIZE_CHANGED: {
      int w = 0, h = 0;
      SDL_GetWindowSize(g_win, &w, &h);
      dispatch_window_size(w, h);
      break;
    }
    case SDL_WINDOWEVENT_EXPOSED:
    case SDL_WINDOWEVENT_SHOWN:
      if (cb_refresh)
        cb_refresh();
      break;
    case SDL_WINDOWEVENT_FOCUS_GAINED:
      set_window_mouse_grab(SDL_TRUE);
      break;
    case SDL_WINDOWEVENT_FOCUS_LOST:
    case SDL_WINDOWEVENT_MINIMIZED:
      set_window_mouse_grab(SDL_FALSE);
      break;
    default:
      break;
    }
    break;

  case SDL_KEYDOWN:
  case SDL_KEYUP: {
    if (!cb_key || !g_win)
      break;
    int gk = map_keysym_to_glfw(&e->key.keysym);
    if (gk == GLFW_KEY_UNKNOWN)
      break;
    int act = (e->type == SDL_KEYDOWN) ? GLFW_PRESS : GLFW_RELEASE;
    cb_key(gk, act);
    break;
  }

  case SDL_MOUSEBUTTONDOWN:
  case SDL_MOUSEBUTTONUP:
    if (cb_mouse_btn && g_win &&
        e->button.windowID == SDL_GetWindowID(g_win)) {
      int b = sdl_mouse_button_to_glfw(e->button.button);
      int act = (e->type == SDL_MOUSEBUTTONDOWN) ? GLFW_PRESS : GLFW_RELEASE;
      cb_mouse_btn(b, act);
    }
    break;

  case SDL_MOUSEMOTION:
    if (cb_mouse_pos && g_win &&
        e->motion.windowID == SDL_GetWindowID(g_win)) {
      /* Top-left client coords; MousePosCallback does y_gl = scr_height - y. */
      cb_mouse_pos(e->motion.x, e->motion.y);
    }
    break;

  case SDL_MOUSEWHEEL:
    if (cb_mouse_wheel && g_win) {
      g_wheel_pos += e->wheel.y;
      cb_mouse_wheel(g_wheel_pos);
    }
    break;

  default:
    break;
  }
}

int glfwInit(void)
{
  return 1;
}

void glfwTerminate(void)
{
  glfwCloseWindow();
}

int glfwOpenWindow(int width, int height, int redbits, int greenbits, int bluebits,
                   int alphabits, int depthbits, int stencilbits, int mode)
{
  (void)redbits;
  (void)greenbits;
  (void)bluebits;
  (void)alphabits;

  glfwCloseWindow();

  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, alphabits > 0 ? alphabits : 0);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depthbits > 0 ? depthbits : 16);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, stencilbits);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

  Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
  if (mode == GLFW_FULLSCREEN)
    flags |= SDL_WINDOW_FULLSCREEN;

  g_win = SDL_CreateWindow("Dark Oberon", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                           width, height, flags);
  if (!g_win)
    return 0;

  g_ctx = SDL_GL_CreateContext(g_win);
  if (!g_ctx) {
    SDL_DestroyWindow(g_win);
    g_win = NULL;
    return 0;
  }

  SDL_GL_MakeCurrent(g_win, g_ctx);
  /* WM may pick a different size (fullscreen, constraints); keep config in sync. */
  int aw = 0, ah = 0;
  SDL_GetWindowSize(g_win, &aw, &ah);
  if (aw > 0 && ah > 0)
    dispatch_window_size(aw, ah);
  else
    dispatch_window_size(width, height);
  set_window_mouse_grab(SDL_TRUE);
  return 1;
}

void glfwCloseWindow(void)
{
  if (g_ctx) {
    SDL_GL_DeleteContext(g_ctx);
    g_ctx = NULL;
  }
  if (g_win) {
    set_window_mouse_grab(SDL_FALSE);
    SDL_DestroyWindow(g_win);
    g_win = NULL;
  }
}

void glfwSetWindowTitle(const char *title)
{
  if (g_win)
    SDL_SetWindowTitle(g_win, title);
}

void glfwGetWindowSize(int *width, int *height)
{
  if (!g_win) {
    if (width)
      *width = 0;
    if (height)
      *height = 0;
    return;
  }
  SDL_GetWindowSize(g_win, width, height);
}

void glfwGetFramebufferSize(int *width, int *height)
{
  if (!g_win) {
    if (width)
      *width = 0;
    if (height)
      *height = 0;
    return;
  }
  SDL_GL_GetDrawableSize(g_win, width, height);
}

void glfwSetWindowSize(int width, int height)
{
  if (!g_win)
    return;
  SDL_SetWindowSize(g_win, width, height);
  int w = 0, h = 0;
  SDL_GetWindowSize(g_win, &w, &h);
  dispatch_window_size(w, h);
}

void glfwRestoreWindow(void)
{
  if (g_win)
    SDL_RestoreWindow(g_win);
}

void glfwSwapBuffers(void)
{
  if (g_win && g_ctx)
    SDL_GL_SwapWindow(g_win);
}

void glfwSwapInterval(int interval)
{
  SDL_GL_SetSwapInterval(interval ? 1 : 0);
}

int glfwGetWindowParam(int param)
{
  if (!g_win)
    return (param == GLFW_OPENED) ? 0 : 0;
  if (param == GLFW_OPENED)
    return 1;
  if (param == GLFW_ACTIVE)
    return (SDL_GetWindowFlags(g_win) & SDL_WINDOW_INPUT_FOCUS) ? 1 : 0;
  return 0;
}

void glfwPollEvents(void)
{
  SDL_Event e;
  while (SDL_PollEvent(&e))
    process_event(&e);
}

void glfwEnable(int token)
{
  if (token == GLFW_MOUSE_CURSOR)
    SDL_ShowCursor(SDL_ENABLE);
}

void glfwDisable(int token)
{
  if (token == GLFW_MOUSE_CURSOR) {
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetCursor(NULL);
  }
  /* Sticky keys / mouse: not emulated; polling matches typical GLFW usage here */
}

int glfwGetKey(int key)
{
  const Uint8 *st = SDL_GetKeyboardState(NULL);
  if (!st)
    return GLFW_RELEASE;

  if (key == '+') {
    return (st[SDL_SCANCODE_EQUALS] && (SDL_GetModState() & KMOD_SHIFT)) ? GLFW_PRESS : GLFW_RELEASE;
  }
  if (key == '=') {
    return (st[SDL_SCANCODE_EQUALS] && !(SDL_GetModState() & KMOD_SHIFT)) ? GLFW_PRESS : GLFW_RELEASE;
  }

  SDL_Scancode sc = glfw_key_to_scancode(key);
  if (sc == SDL_SCANCODE_UNKNOWN)
    return GLFW_RELEASE;
  return st[sc] ? GLFW_PRESS : GLFW_RELEASE;
}

int glfwGetMouseButton(int button)
{
  int sb = glfw_button_to_sdl(button);
  if (!sb)
    return GLFW_RELEASE;
  Uint32 m = SDL_GetMouseState(NULL, NULL);
  return (m & SDL_BUTTON(sb)) ? GLFW_PRESS : GLFW_RELEASE;
}

void glfwGetMousePos(int *xpos, int *ypos)
{
  int x = 0, y = 0;
  if (g_win)
    SDL_GetMouseState(&x, &y);
  if (xpos)
    *xpos = x;
  if (ypos)
    *ypos = y;
}

void glfwSetMousePos(int xpos, int ypos)
{
  if (g_win)
    SDL_WarpMouseInWindow(g_win, xpos, ypos);
}

int glfwGetMouseWheel(void)
{
  return g_wheel_pos;
}

void glfwSetMouseWheel(int pos)
{
  g_wheel_pos = pos;
}

void glfwSetWindowSizeCallback(GLFWwindowsizefun cbfun)
{
  cb_size = cbfun;
}

void glfwSetKeyCallback(GLFWkeyfun cbfun)
{
  cb_key = cbfun;
}

void glfwSetMouseButtonCallback(GLFWmousebuttonfun cbfun)
{
  cb_mouse_btn = cbfun;
}

void glfwSetMousePosCallback(GLFWmouseposfun cbfun)
{
  cb_mouse_pos = cbfun;
}

void glfwSetMouseWheelCallback(GLFWmousewheelfun cbfun)
{
  cb_mouse_wheel = cbfun;
}

void glfwSetWindowRefreshCallback(GLFWwindowrefreshfun cbfun)
{
  cb_refresh = cbfun;
}

int glfwGetVideoModes(GLFWvidmode *list, int maxcount)
{
  if (!list || maxcount <= 0)
    return 0;

  int n = SDL_GetNumDisplayModes(0);
  if (n < 0)
    n = 0;

  int out = 0;
  for (int i = 0; i < n && out < maxcount; i++) {
    SDL_DisplayMode dm;
    if (SDL_GetDisplayMode(0, i, &dm) != 0)
      continue;
    if (dm.w <= 0 || dm.h <= 0)
      continue;

    bool dup = false;
    for (int j = 0; j < out; j++) {
      if (list[j].Width == dm.w && list[j].Height == dm.h) {
        dup = true;
        break;
      }
    }
    if (dup)
      continue;

    list[out].Width = dm.w;
    list[out].Height = dm.h;
    /* Same field order as legacy GLFW (RedBits, BlueBits, GreenBits). */
    list[out].RedBits = 8;
    list[out].BlueBits = 8;
    list[out].GreenBits = 8;
    out++;
  }

  return out;
}

#else /* HEADLESS */

int glfwInit(void) { return 1; }
void glfwTerminate(void) {}
int glfwOpenWindow(int, int, int, int, int, int, int, int, int) { return 0; }
void glfwCloseWindow(void) {}
void glfwSetWindowTitle(const char *) {}
void glfwGetWindowSize(int *width, int *height) {
  if (width) *width = 0;
  if (height) *height = 0;
}
void glfwGetFramebufferSize(int *width, int *height) {
  if (width) *width = 0;
  if (height) *height = 0;
}
void glfwSetWindowSize(int, int) {}
void glfwRestoreWindow(void) {}
void glfwSwapBuffers(void) {}
void glfwSwapInterval(int) {}
int glfwGetWindowParam(int) { return 0; }
void glfwPollEvents(void) {}
void glfwEnable(int) {}
void glfwDisable(int) {}
int glfwGetKey(int) { return 0; }
int glfwGetMouseButton(int) { return 0; }
void glfwGetMousePos(int *xpos, int *ypos) {
  if (xpos) *xpos = 0;
  if (ypos) *ypos = 0;
}
void glfwSetMousePos(int, int) {}
int glfwGetMouseWheel(void) { return 0; }
void glfwSetMouseWheel(int) {}
void glfwSetWindowSizeCallback(GLFWwindowsizefun) {}
void glfwSetKeyCallback(GLFWkeyfun) {}
void glfwSetMouseButtonCallback(GLFWmousebuttonfun) {}
void glfwSetMousePosCallback(GLFWmouseposfun) {}
void glfwSetMouseWheelCallback(GLFWmousewheelfun) {}
void glfwSetWindowRefreshCallback(GLFWwindowrefreshfun) {}
int glfwGetVideoModes(GLFWvidmode *, int) { return 0; }

#endif /* HEADLESS */
