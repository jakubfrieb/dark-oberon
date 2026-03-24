//========================================================================
// App time (GLFW 2/3): single place for glfwGetTime during migration.
//========================================================================
#pragma once

#include <glfw.h>

inline double AppTimeSeconds() { return glfwGetTime(); }
