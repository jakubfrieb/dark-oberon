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
 *  @file doevents.cpp
 *
 *  Methods for work with events.
 *
 *  @author Martin Kosalko
 *  @author Valeria Sventova
 *
 *  @date 2004
 */


//========================================================================
// Included files
//========================================================================
#include <stdio.h>

#include "doevents.h"
#include "dologs.h"
#include "doplayers.h"
#include "dopool.h"

//=========================================================================
// Global variables and functions
//=========================================================================

TPOOL<TEVENT> * pool_events;
TPOOL<TPATH_INFO> *pool_path_info;
TPOOL<TSEL_NODE> * pool_sel_node;
TPOOL<TNEAREST_INFO> *pool_nearest_info;
TQUEUE_EVENTS * queue_events;

/**
 *  Mutex to assure safe data sharing between graphic thread and update thread.
 */
SDL_mutex *delete_mutex = NULL;

//=========================================================================
// class TEVENT
//=========================================================================

/**
 *  Constructor. Only zeroize the values.
 */
TEVENT::TEVENT(void)
{
  // information values
  player_id = 0;
  unit_id = 0;
  priority = false;
  from_net = false;
  event = US_NONE;
  last_event = US_NONE;
  time_stamp = 0;
  request_id = 0;
  simple1 = simple2 = simple3 = simple4 = simple5 = simple6 = 0;
  int1 = int2 = 0;

  // queue
  queue_left = NULL;
  queue_right = NULL;
};


/**
 *  Clears all values of event.
 *  @param all clears pointers too.
 */
void TEVENT::Clear(bool all)
{
  TPOOL_ELEMENT::Clear(all);
  player_id = 0;
  unit_id = 0;
  priority = false;
  from_net = false;
  event = US_NONE;
  last_event = US_NONE;
  time_stamp = 0;
  request_id = 0;
  simple1 = simple2 = simple3 = simple4 = simple5 = simple6=0;
  int1 = int2 =0;
  
  if (all){
    queue_left = queue_right = NULL;
  }
}

/**
 *  Sets all properties of event.
 *  If n_request_id is 0, autogenerates new request id.
 *  If player with identificator n_player_id does not exists, request_id is set to 0
 */
void TEVENT::SetEventProps(int n_player_id, int n_unit_id, bool n_priority, double n_time_stamp, int n_event, int n_last_event, int n_request_id, T_SIMPLE n_simple1, T_SIMPLE n_simple2, T_SIMPLE n_simple3, T_SIMPLE n_simple4, T_SIMPLE n_simple5, T_SIMPLE n_simple6, intptr_t n_int1,intptr_t n_int2)
{
  // identification values
  player_id = n_player_id;
  unit_id = n_unit_id;
  from_net = false;
  
  // if event is in queue and priority is chnged -> event will be stored to another priority queue
  if ((queue_left || queue_right) && (priority != n_priority)) {
    queue_events->GetEvent(this);
    priority = n_priority;
    queue_events->PutEvent(this);
  }
  else
    priority = n_priority;

  #if DEBUG_EVENTS
    if ((queue_left || queue_right) && (time_stamp != n_time_stamp))
        Error(LogMsg("Chnged timestamp of event in queue!"));
    else
  #endif
      time_stamp = n_time_stamp;
  event = n_event;
  last_event = n_last_event;
  
  if (n_request_id != 0) // set given request_id
    request_id = n_request_id;
  else 
  {  // generating new request_id
    if (n_player_id >= player_array.GetCount()) request_id = 0;
    else request_id = players[n_player_id]->IncrementRequestCounter();
  }

  // data values
  simple1 = n_simple1;
  simple2 = n_simple2;
  simple3 = n_simple3;
  simple4 = n_simple4;
  simple5 = n_simple5;
  simple6 = n_simple6;
  
  int1 = n_int1;
  int2 = n_int2;
}

/**
 *  True for the requests a host sends to a unit of another player, to its owner
 *  (TPLAYER_UNIT::SendRequest() on a remote unit): a worker asks a source whether
 *  it can mine, and the source answers the worker. Every other request on the
 *  network is about the sender's own unit and is broadcast by its owner
 *  (SendNetEvent(..., all_players)); the relay drops it when it is sent to
 *  another player.
 */
bool TEVENT::IsCrossOwnerRequest(void)
{
  return event == RQ_CAN_MINE;
}

/**
 *  Linearize event to array of chars (prepare event for sending through
 *  network).
 *
 *  Wire format (network byte order, the same size on every platform):
 *  player_id u8, unit_id i32, time_stamp f64 (as u64 bits), event i32,
 *  last_event i32, request_id i32, simple1..6 u8, int1 i64, int2 i64.
 *
 *  @param char_event Input paramenter where linearized event will be stored
 *                    (at least GetLinearizedSize() bytes).
 *
 *  @return Count of characters written to char_event (size of linearized
 *          event).
 *
 *  @note Keep the field list in sync with DelinearizeEvent() and
 *        GetLinearizedSize() (received events must have exactly that size).
 */
int TEVENT::LinearizeEvent(char *char_event)
{
  T_BYTE *p = reinterpret_cast<T_BYTE *>(char_event);

  p[0] = static_cast<T_BYTE>(player_id);
  NetPutU32 (p + 1, static_cast<uint32_t>(unit_id));
  NetPutDouble (p + 5, time_stamp);
  NetPutU32 (p + 13, static_cast<uint32_t>(event));
  NetPutU32 (p + 17, static_cast<uint32_t>(last_event));
  NetPutU32 (p + 21, static_cast<uint32_t>(request_id));
  p[25] = simple1;
  p[26] = simple2;
  p[27] = simple3;
  p[28] = simple4;
  p[29] = simple5;
  p[30] = simple6;
  NetPutU64 (p + 31, static_cast<uint64_t>(static_cast<int64_t>(int1)));
  NetPutU64 (p + 39, static_cast<uint64_t>(static_cast<int64_t>(int2)));

  return GetLinearizedSize ();
}

/**
 *  Delinearize event - fill it with data from parameter @p char_event.
 *
 *  @param char_event    Linearized event where data are taken from.
 *  @param lin_event_len Size of linearized event.
 *
 *  @return false (event left unchanged) if @p lin_event_len is not exactly
 *          the size LinearizeEvent() produces, or int1/int2 do not fit
 *          intptr_t (32-bit hosts).
 */
bool TEVENT::DelinearizeEvent(char *char_event, int lin_event_len)
{
  if (lin_event_len != GetLinearizedSize ())
    return false;

  const T_BYTE *p = reinterpret_cast<const T_BYTE *>(char_event);

  const int64_t n_int1 = static_cast<int64_t>(NetGetU64 (p + 31));
  const int64_t n_int2 = static_cast<int64_t>(NetGetU64 (p + 39));
  if (static_cast<intptr_t>(n_int1) != n_int1 || static_cast<intptr_t>(n_int2) != n_int2)
    return false;

  player_id = p[0];
  unit_id = static_cast<int32_t>(NetGetU32 (p + 1));
  time_stamp = NetGetDouble (p + 5);
  event = static_cast<int32_t>(NetGetU32 (p + 13));
  last_event = static_cast<int32_t>(NetGetU32 (p + 17));
  request_id = static_cast<int32_t>(NetGetU32 (p + 21));
  simple1 = p[25];
  simple2 = p[26];
  simple3 = p[27];
  simple4 = p[28];
  simple5 = p[29];
  simple6 = p[30];
  int1 = static_cast<intptr_t>(n_int1);
  int2 = static_cast<intptr_t>(n_int2);

  priority = false;
  from_net = true;
  return true;
}

/**
 *  Size of the linearized event (the fields packed by LinearizeEvent()).
 */
int TEVENT::GetLinearizedSize(void)
{
  return 1 + 4 + 8 + 3 * 4 + 6 + 2 * 8;   // 47 bytes
}


//========================================================================
// class QUEUE_EVENTS
//========================================================================

/**
 *  Constructor. Zeroize the values and create mutex for later use.
 */
TQUEUE_EVENTS::TQUEUE_EVENTS()
{
  // create mutex
  if ((mutex = SDL_CreateMutex ()) == NULL) 
    Critical ("Could not create mutex");

  count = 0;    //initialization
  events = prior_events = NULL;  
}


/**
 *  Destructor. Clears and destroys mutex.
 */
TQUEUE_EVENTS::~TQUEUE_EVENTS()
{ 
  Clear ();

  SDL_DestroyMutex(mutex);
}


/**
 *  Clear structure.
 */
void TQUEUE_EVENTS::Clear()
{
  TEVENT *act_event, *delete_event;

  SDL_LockMutex (mutex);

  // delete priority gueue
  act_event = delete_event = prior_events;
  while(act_event)
  {
    act_event = act_event->GetNextQueueEvent();
    delete delete_event;    
    delete_event = act_event;
  }
  prior_events = NULL;

  // delete basic queue
  act_event = delete_event = events;
  while(act_event)
  {
    act_event = act_event->GetNextQueueEvent();
    delete delete_event;    
    delete_event = act_event;
  }
  events = NULL;

  count = 0;

  SDL_UnlockMutex (mutex);
}


/**
 *  Puts event given in parameter to queue according to its timestamp.
 *  Queue is implemented as both-directional linked list.
 */
void TQUEUE_EVENTS::PutEvent(TEVENT *event)
{  
  if (!event) return;

  TEVENT **list_begin, *act_event, *act_event_prev = NULL;

  SDL_LockMutex (mutex);

  // for sure
#if DEBUG_EVENTS
  Debug(LogMsg("To Q: P:%d U:%d E:%s RQ:%d X:%d Y:%d Z:%d R:%d TS:%f COUNT:%d", event->GetPlayerID(), event->GetUnitID(), EventToString(event->GetEvent()), event->GetRequestID(), event->simple1, event->simple2, event->simple3, event->simple4, event->GetTimeStamp(), count));

  if (event->GetNextQueueEvent() || event->GetPrevQueueEvent())
    Error("Putting event to queue, that is already in queue!");
#endif

  // set list
  if (event->GetPriority())
    list_begin = &prior_events;
  else
    list_begin = &events;

  // queue is empty
  if (!(*list_begin))
  {
    // add event to begin
    (*list_begin) = event;
  }

  // queue is not empty
  else
  {
    // go to right place in queue
    act_event = *list_begin;
    while (act_event && event->GetTimeStamp() > act_event->GetTimeStamp())
    {
      act_event_prev = act_event;
      act_event = act_event->GetNextQueueEvent();
    }

    // add event at the end of the queue
    if (!act_event)
    {
      act_event_prev->SetNextQueueEvent(event);
      event->SetPrevQueueEvent(act_event_prev); 
    }

    // add event before the first
    else if (act_event == *list_begin)
    {
      event->SetNextQueueEvent(act_event);
      act_event->SetPrevQueueEvent(event); 
      (*list_begin) = event;
    }

    // add event before act_event
    else
    {
      event->SetPrevQueueEvent(act_event_prev);
      event->SetNextQueueEvent(act_event);
      act_event->SetPrevQueueEvent(event);
      act_event_prev->SetNextQueueEvent(event);
    }    
  }

  // inc count
  count++;

#if DEBUG_EVENTS
  int c = 0;
  for (act_event = prior_events; act_event; act_event = act_event->GetNextQueueEvent())
    c++;
  for (act_event = events; act_event; act_event = act_event->GetNextQueueEvent())
    c++;

  if (c != count)
  {
    Error("Broken queue! - PutEvent()");
    LogQueue();
  }
#endif

  SDL_UnlockMutex (mutex);
}


/**
 *  Gets event from queue  according to given pointer.
 */
void TQUEUE_EVENTS::GetEvent(TEVENT *event)
{
  if (!event) return;

  TEVENT **list_begin;

  SDL_LockMutex (mutex);

  #if DEBUG_EVENTS
  Debug(LogMsg("Fr Q: P:%d U:%d E:%s RQ:%d X:%d Y:%d Z:%d R:%d TS:%f COUNT:%d", event->GetPlayerID(), event->GetUnitID(), EventToString(event->GetEvent()), event->GetRequestID(), event->simple1, event->simple2, event->simple3, event->simple4, event->GetTimeStamp(), count));
  #endif

  // set list
  if (event->GetPriority())
    list_begin = &prior_events;
  else
    list_begin = &events;
 
  // remove event from list
  if (event->GetNextQueueEvent()) 
    event->GetNextQueueEvent()->SetPrevQueueEvent(event->GetPrevQueueEvent());
  if (event->GetPrevQueueEvent())
    event->GetPrevQueueEvent()->SetNextQueueEvent(event->GetNextQueueEvent());   

  // if event is first
  if (event == *list_begin)
    *list_begin = (*list_begin)->GetNextQueueEvent();

  // clear event
  event->SetNextQueueEvent(NULL);
  event->SetPrevQueueEvent(NULL);

  // decrease events count
  count--;

#if DEBUG_EVENTS
  TEVENT *act_event;
  int c = 0;
  for (act_event = prior_events; act_event; act_event = act_event->GetNextQueueEvent())
    c++;
  for (act_event = events; act_event; act_event = act_event->GetNextQueueEvent())
    c++;

  if (c != count)
  {
    Error("Broken queue! - GetEvent()");
    LogQueue();
  }
#endif

  SDL_UnlockMutex (mutex);
}


/**
 *  Returns pointer to the event with lowest timestamp.
 */
TEVENT *TQUEUE_EVENTS::GetFirstEvent()
{
  TEVENT **list_begin, *first;

  SDL_LockMutex (mutex);

  // set list
  if (prior_events)
    list_begin = &prior_events;
  else if (events)
    list_begin = &events;
  else {
    SDL_UnlockMutex (mutex);
    return NULL;
  }

  first = *list_begin;
  (*list_begin) = first->GetNextQueueEvent();
  if (*list_begin)
    (*list_begin)->SetPrevQueueEvent(NULL); 
  
  first->SetNextQueueEvent(NULL);
  first->SetPrevQueueEvent(NULL);

#if DEBUG_EVENTS
  Debug(LogMsg("Fr Q: P:%d U:%d E:%s RQ:%d X:%d Y:%d Z:%d R:%d TS:%f COUNT:%d", first->GetPlayerID(), first->GetUnitID(), EventToString(first->GetEvent()), first->GetRequestID(), first->simple1, first->simple2, first->simple3, first->simple4, first->GetTimeStamp(), count));
#endif


  // decrease events count
  count--;


#if DEBUG_EVENTS
  TEVENT *act_event;
  int c = 0;
  for (act_event = prior_events; act_event; act_event = act_event->GetNextQueueEvent())
    c++;
  for (act_event = events; act_event; act_event = act_event->GetNextQueueEvent())
    c++;

  if (c != count)
  {
    Error("Broken queue! - GetFirstEvent()");
    LogQueue();
  }
#endif


  SDL_UnlockMutex (mutex);

  return first;
}


/**
 *  Returns timestamp of event with lowest timestamp. If there is no event in queue returns -1.
 */
double TQUEUE_EVENTS::GetFirstEventTimeStamp()
{ 
  double ts;
  TEVENT **list_begin;

  SDL_LockMutex (mutex);

  // set list
  if (prior_events)
    list_begin = &prior_events;
  else if (events)
    list_begin = &events;
  else {
    SDL_UnlockMutex (mutex);
    return -1;
  }

  ts = (*list_begin)->GetTimeStamp();

  SDL_UnlockMutex (mutex);
  
  return ts;
}


#if DEBUG
/**
 *  Writes whole message queue to log file.
 */
void TQUEUE_EVENTS::LogQueue(void)
{
  TEVENT *act_event;
  int c;

  SDL_LockMutex (mutex);

  Debug(LogMsg("*** TQUEUE_EVENTS - count: %d", count));

  // write priority queue
  Debug("*** Priority message queue ***");

  c = 0;
  for (act_event = prior_events; act_event; act_event = act_event->GetNextQueueEvent())
  {
    Debug(LogMsg("*** P:%d U:%d E:%s RQ:%d X:%d Y:%d Z:%d R:%d TS:%f", act_event->GetPlayerID(), act_event->GetUnitID(), EventToString(act_event->GetEvent()), act_event->GetRequestID(), act_event->simple1, act_event->simple2, act_event->simple3, act_event->simple4, act_event->GetTimeStamp()));
    c++;
  }
  Debug(LogMsg("*** count: %d", c));

  // write basic queue
  Debug("*** Basic message queue ******");

  c = 0;
  for (act_event = events; act_event; act_event = act_event->GetNextQueueEvent())
  {
    Debug(LogMsg("*** P:%d U:%d E:%s RQ:%d X:%d Y:%d Z:%d R:%d TS:%f", act_event->GetPlayerID(), act_event->GetUnitID(), EventToString(act_event->GetEvent()), act_event->GetRequestID(), act_event->simple1, act_event->simple2, act_event->simple3, act_event->simple4, act_event->GetTimeStamp()));
    c++;
  }
  Debug(LogMsg("*** count: %d", c));

  Debug("*** END **********************");

  SDL_UnlockMutex (mutex);
}
#endif


//=========================================================================
// END
//=========================================================================
// vim:ts=2:sw=2:et:

