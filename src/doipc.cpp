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
 *  @file doipc.cpp
 *
 *  IPC - inter process communication (mutexes etc.).
 *
 *  @author Marian Cerny
 *
 *  @date 2005
 */


//=========================================================================
// Included files
//=========================================================================

#include "cfg.h"

#include "doipc.h"
#include "dologs.h"

//=========================================================================
// TLOCK
//=========================================================================

TLOCK::TLOCK ()
{
  mutex = SDL_CreateMutex ();

  /* Throws an exception when mutex was not created. */
  if (!mutex)
    throw MutexException ();

#if DEBUG
  unlocked = SDL_CreateCond ();

  /* Throws an exception when mutex was not created. */
  if (!unlocked)
    throw MutexException ();

  locked_by = 0;
#endif
}


TLOCK::~TLOCK ()
{
  SDL_DestroyMutex (mutex);

#if DEBUG
  SDL_DestroyCond (unlocked);
#endif
}


#if DEBUG
/**
 *  Locks the object to get exclusive access to it.
 */
void TLOCK::Lock ()
{
  SDL_threadID myself = SDL_ThreadID ();

  SDL_LockMutex (mutex);

  if (locked_by == myself) {
    /* Mutex is already locked by myself, you should consider using
     * TRECURSIVE_LOCK instead. */
    throw MutexException ();
  }

  /* wait until I can enter. */
  while (locked_by != 0) {
    SDL_CondWait (unlocked, mutex);
  }

  locked_by = myself;

  SDL_UnlockMutex (mutex);
}
#endif

#if DEBUG
/**
 *  Locks the object to get exclusive access to it.
 */
void TLOCK::Unlock ()
{
  SDL_LockMutex (mutex);

  SDL_threadID myself = SDL_ThreadID ();

  if (locked_by == 0) {
    /* Mutex is not locked! Fix your bug. */
    throw MutexException ();
  }

  if (locked_by != myself) {
    /* Hey, hey! You shouldn't be unlocking this when you din't lock it. */
    throw MutexException ();
  }

  locked_by = 0;

  SDL_CondSignal (unlocked);

  SDL_UnlockMutex (mutex);
}
#endif


//=========================================================================
// TRECURSIVE_LOCK
//=========================================================================

/**
 *  Creates mutex needed for proper manipulation with object.
 *
 *  @throw MutexException { Error creating mutex. }
 */
TRECURSIVE_LOCK::TRECURSIVE_LOCK ()
{
  mutex = SDL_CreateMutex ();

  /* Throws an exception when mutex was not created. */
  if (!mutex)
    throw MutexException ();

  locked_by = 0;
  locked_count = 0;
}


TRECURSIVE_LOCK::~TRECURSIVE_LOCK ()
{
  SDL_DestroyMutex (mutex);
}

/**
 *  Locks the object to get exclusive access to it. Lock() can be called more
 *  than once by one thread, it is implemented as a recursive lock. Unlock()
 *  then must be called the same count as Lock().
 */
void TRECURSIVE_LOCK::Lock ()
{
#if DEBUG
  if (!mutex)
    throw MutexException ();
#endif

  SDL_threadID myself = SDL_ThreadID ();

  if (locked_by != myself)
  {
    SDL_LockMutex (mutex);
    locked_by = myself;
  }

  locked_count++;
}


/**
 *  Unlocks the object.
 *
 *  @see Lock
 */
void TRECURSIVE_LOCK::Unlock ()
{
  SDL_threadID myself = SDL_ThreadID ();

  if (locked_by != myself) {
    throw MutexException ();
  }

  locked_count--;

  if (!locked_count) {
    locked_by = 0;
    SDL_UnlockMutex (mutex);
  }
}


//=========================================================================
// Global variables
//=========================================================================

TRECURSIVE_LOCK *giant;
TRECURSIVE_LOCK *process_mutex;


//=========================================================================
// Global functions
//=========================================================================

void init_giant () {
  giant = NEW TRECURSIVE_LOCK ();
}

//=========================================================================
// END
//=========================================================================
// vim:ts=2:sw=2:et:
