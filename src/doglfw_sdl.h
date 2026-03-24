/*
 * GLFW 2.x API subset implemented with SDL2 (phase 3 migration).
 * Replaces <glfw.h> + libglfw2 for window, GL context, and input.
 */
#ifndef __doglfw_sdl_h__
#define __doglfw_sdl_h__

#include <SDL2/SDL.h>

#if defined(__APPLE_CC__) || defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GLFWCALL
#define GLFWCALL
#endif

#define GLFW_RELEASE            0
#define GLFW_PRESS              1

#define GLFW_KEY_UNKNOWN      -1
#define GLFW_KEY_SPACE        32
#define GLFW_KEY_SPECIAL      256
#define GLFW_KEY_ESC          (GLFW_KEY_SPECIAL+1)
#define GLFW_KEY_F1           (GLFW_KEY_SPECIAL+2)
#define GLFW_KEY_F2           (GLFW_KEY_SPECIAL+3)
#define GLFW_KEY_F3           (GLFW_KEY_SPECIAL+4)
#define GLFW_KEY_F4           (GLFW_KEY_SPECIAL+5)
#define GLFW_KEY_F5           (GLFW_KEY_SPECIAL+6)
#define GLFW_KEY_F6           (GLFW_KEY_SPECIAL+7)
#define GLFW_KEY_F7           (GLFW_KEY_SPECIAL+8)
#define GLFW_KEY_F8           (GLFW_KEY_SPECIAL+9)
#define GLFW_KEY_F9           (GLFW_KEY_SPECIAL+10)
#define GLFW_KEY_F10          (GLFW_KEY_SPECIAL+11)
#define GLFW_KEY_F11          (GLFW_KEY_SPECIAL+12)
#define GLFW_KEY_F12          (GLFW_KEY_SPECIAL+13)
#define GLFW_KEY_UP           (GLFW_KEY_SPECIAL+27)
#define GLFW_KEY_DOWN         (GLFW_KEY_SPECIAL+28)
#define GLFW_KEY_LEFT         (GLFW_KEY_SPECIAL+29)
#define GLFW_KEY_RIGHT        (GLFW_KEY_SPECIAL+30)
#define GLFW_KEY_LSHIFT       (GLFW_KEY_SPECIAL+31)
#define GLFW_KEY_RSHIFT       (GLFW_KEY_SPECIAL+32)
#define GLFW_KEY_LCTRL        (GLFW_KEY_SPECIAL+33)
#define GLFW_KEY_RCTRL        (GLFW_KEY_SPECIAL+34)
#define GLFW_KEY_LALT         (GLFW_KEY_SPECIAL+35)
#define GLFW_KEY_RALT         (GLFW_KEY_SPECIAL+36)
#define GLFW_KEY_TAB          (GLFW_KEY_SPECIAL+37)
#define GLFW_KEY_ENTER        (GLFW_KEY_SPECIAL+38)
#define GLFW_KEY_BACKSPACE    (GLFW_KEY_SPECIAL+39)
#define GLFW_KEY_INSERT       (GLFW_KEY_SPECIAL+40)
#define GLFW_KEY_DEL          (GLFW_KEY_SPECIAL+41)
#define GLFW_KEY_PAGEUP       (GLFW_KEY_SPECIAL+42)
#define GLFW_KEY_PAGEDOWN     (GLFW_KEY_SPECIAL+43)
#define GLFW_KEY_HOME         (GLFW_KEY_SPECIAL+44)
#define GLFW_KEY_END          (GLFW_KEY_SPECIAL+45)
#define GLFW_KEY_KP_0         (GLFW_KEY_SPECIAL+46)
#define GLFW_KEY_KP_1         (GLFW_KEY_SPECIAL+47)
#define GLFW_KEY_KP_2         (GLFW_KEY_SPECIAL+48)
#define GLFW_KEY_KP_3         (GLFW_KEY_SPECIAL+49)
#define GLFW_KEY_KP_4         (GLFW_KEY_SPECIAL+50)
#define GLFW_KEY_KP_5         (GLFW_KEY_SPECIAL+51)
#define GLFW_KEY_KP_6         (GLFW_KEY_SPECIAL+52)
#define GLFW_KEY_KP_7         (GLFW_KEY_SPECIAL+53)
#define GLFW_KEY_KP_8         (GLFW_KEY_SPECIAL+54)
#define GLFW_KEY_KP_9         (GLFW_KEY_SPECIAL+55)
#define GLFW_KEY_KP_DIVIDE    (GLFW_KEY_SPECIAL+56)
#define GLFW_KEY_KP_MULTIPLY  (GLFW_KEY_SPECIAL+57)
#define GLFW_KEY_KP_SUBTRACT  (GLFW_KEY_SPECIAL+58)
#define GLFW_KEY_KP_ADD       (GLFW_KEY_SPECIAL+59)
#define GLFW_KEY_KP_DECIMAL   (GLFW_KEY_SPECIAL+60)
#define GLFW_KEY_KP_EQUAL     (GLFW_KEY_SPECIAL+61)
#define GLFW_KEY_KP_ENTER     (GLFW_KEY_SPECIAL+62)
#define GLFW_KEY_LAST         GLFW_KEY_KP_ENTER

#define GLFW_MOUSE_BUTTON_1      0
#define GLFW_MOUSE_BUTTON_2      1
#define GLFW_MOUSE_BUTTON_3      2
#define GLFW_MOUSE_BUTTON_4      3
#define GLFW_MOUSE_BUTTON_5      4
#define GLFW_MOUSE_BUTTON_LEFT   GLFW_MOUSE_BUTTON_1
#define GLFW_MOUSE_BUTTON_RIGHT  GLFW_MOUSE_BUTTON_2
#define GLFW_MOUSE_BUTTON_MIDDLE GLFW_MOUSE_BUTTON_3

#define GLFW_WINDOW               0x00010001
#define GLFW_FULLSCREEN           0x00010002

#define GLFW_OPENED               0x00020001
#define GLFW_ACTIVE               0x00020002

#define GLFW_MOUSE_CURSOR         0x00030001
#define GLFW_STICKY_KEYS          0x00030002
#define GLFW_STICKY_MOUSE_BUTTONS 0x00030003

typedef struct {
  int Width, Height;
  int RedBits, BlueBits, GreenBits;
} GLFWvidmode;

typedef void (GLFWCALL *GLFWwindowsizefun)(int, int);
typedef void (GLFWCALL *GLFWwindowrefreshfun)(void);
typedef void (GLFWCALL *GLFWmousebuttonfun)(int, int);
typedef void (GLFWCALL *GLFWmouseposfun)(int, int);
typedef void (GLFWCALL *GLFWmousewheelfun)(int);
typedef void (GLFWCALL *GLFWkeyfun)(int, int);

int  glfwInit(void);
void glfwTerminate(void);

int  glfwOpenWindow(int width, int height, int redbits, int greenbits, int bluebits,
                    int alphabits, int depthbits, int stencilbits, int mode);
void glfwCloseWindow(void);
void glfwSetWindowTitle(const char *title);
void glfwGetWindowSize(int *width, int *height);
/** OpenGL drawable size in pixels (may differ from window size on HiDPI). */
void glfwGetFramebufferSize(int *width, int *height);
void glfwSetWindowSize(int width, int height);
void glfwRestoreWindow(void);

void glfwSwapBuffers(void);
void glfwSwapInterval(int interval);
int  glfwGetWindowParam(int param);

void glfwPollEvents(void);

void glfwEnable(int token);
void glfwDisable(int token);

int  glfwGetKey(int key);
int  glfwGetMouseButton(int button);
void glfwGetMousePos(int *xpos, int *ypos);
void glfwSetMousePos(int xpos, int ypos);
int  glfwGetMouseWheel(void);
void glfwSetMouseWheel(int pos);

void glfwSetWindowSizeCallback(GLFWwindowsizefun cbfun);
void glfwSetKeyCallback(GLFWkeyfun cbfun);
void glfwSetMouseButtonCallback(GLFWmousebuttonfun cbfun);
void glfwSetMousePosCallback(GLFWmouseposfun cbfun);
void glfwSetMouseWheelCallback(GLFWmousewheelfun cbfun);
void glfwSetWindowRefreshCallback(GLFWwindowrefreshfun cbfun);

int  glfwGetVideoModes(GLFWvidmode *list, int maxcount);

#ifdef __cplusplus
}
#endif

#endif
