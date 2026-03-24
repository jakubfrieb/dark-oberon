/*
 * -------------
 *  Dark Oberon
 * -------------
 * 
 * An advanced strategy game.
 *
 * Copyright (C) 2002 - 2005 Valeria Sventova, Jiri Krejsa, Peter Knut,
 *                           Martin Kosalko, Marian Cerny, Michal Kral
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (see docs/gpl.txt) as
 * published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 */

/**
 *  @file doipc.h
 *
 *  IPC - inter process communication (mutexes etc.).
 *
 *  @author Marian Cerny
 *
 *  @date 2005
 */

#ifndef __doipc_h__
#define __doipc_h__

//=========================================================================
// Included files
//=========================================================================

#include "cfg.h"

#include <string>
#include <list>

#include <glfw.h>


//=========================================================================
// TLOCK
//=========================================================================

struct TLOCK {
public:
  class MutexException {};

  TLOCK ();
  ~TLOCK ();

#if DEBUG
  void Lock ();
  void Unlock ();
#else
  void Lock () {
    glfwLockMutex (mutex);
  }

  void Unlock () {
    glfwUnlockMutex (mutex);
  }
#endif

private:
#if DEBUG
  GLFWthread locked_by;
  GLFWcond unlocked;
#endif
  GLFWmutex mutex;        //!< Mutex for atomicity of operations.
};


//=========================================================================
// TRECURSIVE_LOCK
//=========================================================================

struct TRECURSIVE_LOCK {
public:
  class MutexException {};

  TRECURSIVE_LOCK ();
  ~TRECURSIVE_LOCK ();

  void Lock ();
  void Unlock ();

private:
  GLFWthread locked_by;
  int locked_count;
  GLFWmutex mutex;        //!< Mutex for atomicity of operations.
};


//=========================================================================
// template TSAVE_LIST
//=========================================================================

template <class T>
class TSAVE_LIST {
public:
  class MutexException {};

  TSAVE_LIST () {
    mutex = glfwCreateMutex ();
    if (!mutex)
      throw MutexException ();
  }

  ~TSAVE_LIST () {
    glfwDestroyMutex (mutex);
  }

  void PushBack (T node) {
    glfwLockMutex (mutex);
    list.push_back(node);
    glfwUnlockMutex (mutex);
  }

  bool PopFront (T &node) {
    bool ret;

    glfwLockMutex (mutex);

    ret = !list.empty();
    if (ret) {
      node = *list.begin();
      list.pop_front();
    }

    glfwUnlockMutex (mutex);

    return ret;
  }

private:
  GLFWmutex mutex;        //!< Mutex for atomicity of operations.
  std::list<T> list;
};


//=========================================================================
// Global variables
//=========================================================================

extern TRECURSIVE_LOCK *giant;
extern TRECURSIVE_LOCK *process_mutex;


//=========================================================================
// Global functions
//=========================================================================

void init_giant ();


#endif  // __doplayers_h__

//=========================================================================
// END
//=========================================================================
// vim:ts=2:sw=2:et:

