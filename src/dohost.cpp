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
 *  @file dohost.cpp
 *
 *  Implementation of functions for THOST.
 *
 *  @author Marian Cerny
 *
 *  @date 2004
 */


//=========================================================================
// Included files
//=========================================================================

#include "cfg.h"
#include "doalloc.h"

#include <algorithm>
#include <string>

#include "build_info.h"
#include "dohost.h"
#include "donet.h"

using std::string;


//=========================================================================
// THOST
//=========================================================================

/**
 *  Constructor.
 *
 *  @param incoming_message_queue_size Size of queue for incoming messages.
 *  @param listen_port                 Port on which #listener should listen.
 */
THOST::THOST (int incoming_message_queue_size, in_port_t listen_port)
{
  remote_address.s_addr = 0;
  remote_port = 0;
  remote_conn = no_connection;

  Initialise (incoming_message_queue_size, listen_port);
}

/**
 *  Constructor of a host that connects to @p remote_address:@p remote_port
 *  first (throws TNET_LISTENER::ConnectingErrorException), the host of player 0.
 */
THOST::THOST (int incoming_message_queue_size, in_port_t listen_port,
              in_addr remote_address, in_port_t remote_port)
{
  int fd = TNET_LISTENER::DialSocket (remote_address, remote_port);

  this->remote_address = remote_address;
  this->remote_port = remote_port;

  Initialise (incoming_message_queue_size, listen_port);

  remote_conn = listener->AddDialedConnection (remote_address, remote_port, fd);
  AddRoute (remote_conn);   // player 0 (the hyper player) is on the remote host
}

void THOST::Initialise (int incoming_message_queue_size, in_port_t listen_port)
{
  routes_lock = SDL_CreateMutex ();
  if (routes_lock == NULL)
    Critical ("Could not create mutex");

  listener = NEW TNET_LISTENER (incoming_message_queue_size, listen_port);
  handler = NEW TNET_MESSAGE_HANDLER ();
  dispatcher = NEW TNET_DISPATCHER (listener->GetMessageQueue (), handler);

  listener->ConsumerIsAttached (dispatcher->GetThread ());
}

THOST::~THOST ()
{
  /* Stop the dispatcher before the connections go away: some handlers (time sync) send
   * replies without giant, so they do not see host set to NULL. */
  listener->GetMessageQueue ()->Die ();
  SDL_WaitThread (dispatcher->GetThread (), NULL);
  dispatcher->ThreadJoined ();
  listener->ConsumerIsAttached (NULL);

  /* Order in which objects are deleted must be: listener, dispatcher, handler!
   */
  delete listener;
  delete dispatcher;
  delete handler;

  SDL_DestroyMutex (routes_lock);
}

/**
 *  Queues @p msg for the connections it is for, and returns it to the pool:
 *  - a reply (TNET_MESSAGE::SetReplyConnection()) goes over its connection;
 *  - a message for all_players goes once over every connection of the route table;
 *  - otherwise over the connection of its destination player.
 *
 *  Never blocks (TNET_LISTENER::Send()); each connection gets its own copy.
 */
void THOST::SendMessage (TNET_MESSAGE *msg) {
  if (msg->GetConnection () != no_connection)
    listener->Send (msg->GetConnection (), msg);
  else if (msg->GetDest () == all_players) {
    std::vector<TNET_CONN_ID> conns = GetRoutedConnections ();
    for (unsigned i = 0; i < conns.size (); i++)
      listener->Send (conns[i], msg);
  } else {
    TNET_CONN_ID conn = GetRoute (msg->GetDest ());
    if (conn != no_connection)
      listener->Send (conn, msg);
    else
      NetDropWarning (LogMsg ("Dropping message (type %d) for player %d, who has no connection",
                              msg->GetType (), msg->GetDest ()));
  }

  pool_net_messages->PutToPool (msg);
}

/** Adds the route of the next player id (@p conn = no_connection for a player of this host);
 *  returns the id. */
T_BYTE THOST::AddRoute (TNET_CONN_ID conn) {
  SDL_LockMutex (routes_lock);
  routes.push_back (conn);
  T_BYTE id = routes.size () - 1;
  SDL_UnlockMutex (routes_lock);

  return id;
}

/** Returns the connection of player @p id (no_connection for a player of this host or an unknown id). */
TNET_CONN_ID THOST::GetRoute (int id) {
  SDL_LockMutex (routes_lock);
  TNET_CONN_ID conn = (id >= 0 && id < (int)routes.size ()) ? routes[id] : no_connection;
  SDL_UnlockMutex (routes_lock);

  return conn;
}

/** Player @p id has no connection any more (it ended); the ids stay valid. */
void THOST::ClearRoute (int id) {
  SDL_LockMutex (routes_lock);
  if (id >= 0 && id < (int)routes.size ())
    routes[id] = no_connection;
  SDL_UnlockMutex (routes_lock);
}

/**
 *  Removes player @p id (the ids of the next players go down by one). Its connection is shut
 *  down unless another player uses it.
 */
void THOST::RemoveRoute (int id) {
  SDL_LockMutex (routes_lock);

  TNET_CONN_ID conn = no_connection;
  if (id >= 0 && id < (int)routes.size ()) {
    conn = routes[id];
    routes.erase (routes.begin () + id);
  }

  if (std::find (routes.begin (), routes.end (), conn) != routes.end ())
    conn = no_connection;   // shared (no_connection is not shut down either)

  SDL_UnlockMutex (routes_lock);

  if (conn != no_connection)
    listener->Shutdown (conn);
}

/** Returns every connection of the route table once. */
std::vector<TNET_CONN_ID> THOST::GetRoutedConnections () {
  std::vector<TNET_CONN_ID> conns;

  SDL_LockMutex (routes_lock);
  for (unsigned i = 0; i < routes.size (); i++)
    if (routes[i] != no_connection && std::find (conns.begin (), conns.end (), routes[i]) == conns.end ())
      conns.push_back (routes[i]);
  SDL_UnlockMutex (routes_lock);

  return conns;
}

/** Shuts down the connections to all remote hosts (player ids stay valid, without connections). */
void THOST::DisconnectAll () {
  std::vector<TNET_CONN_ID> conns;

  SDL_LockMutex (routes_lock);
  conns.swap (routes);
  routes.resize (conns.size (), no_connection);
  SDL_UnlockMutex (routes_lock);

  for (unsigned i = 0; i < conns.size (); i++)
    listener->Shutdown (conns[i]);
}

/** Returns the player id (>= @p from) whose host is behind the connection @p conn, or -1. */
int THOST::GetPlayerIDByConnection (TNET_CONN_ID conn, int from) {
  int found = -1;

  SDL_LockMutex (routes_lock);
  for (int i = from; conn != no_connection && i >= 0 && i < (int)routes.size (); i++) {
    if (routes[i] == conn) {
      found = i;
      break;
    }
  }
  SDL_UnlockMutex (routes_lock);

  return found;
}

/**
 *  Sends chat message to all connected hosts.
 *
 *  @param chat_message Message content.
 */
void THOST::SendChatMessage (string chat_message) {
  TNET_MESSAGE *m = pool_net_messages->GetFromPool();
  m->Init_send(net_protocol_chat_message, 0);
  m->PackString (chat_message);

  SendMessage (m);
}

/**
 *  Sends chat message to all connected hosts.
 *
 *  @param chat_message Message content.
 */
void THOST::SendDisconnect (T_BYTE player_id) {
  TNET_MESSAGE *m = pool_net_messages->GetFromPool();
  m->Init_send(net_protocol_disconnect, 0);
  m->PackByte (player_id);

  SendMessage (m);
}

/**
 *  Appends the version trailer to a connect or hello message: u16 protocol
 *  version and the game version string.
 */
void THOST::PackVersion (TNET_MESSAGE *msg) {
  msg->PackU16 (NET_PROTOCOL_VERSION);
  msg->PackString (DO_VERSION_STRING);
}

/**
 *  Reads the version trailer of a connect or hello message (the rest of the
 *  message). Hosts before protocol version 2 send none.
 *
 *  @return Protocol version of the peer, 0 for a host without the trailer.
 *          @p version is the peer's game version ("" without the trailer).
 */
int THOST::ExtractVersion (TNET_MESSAGE *msg, string &version) {
  version.clear ();
  if (msg->Remaining () == 0)
    return 0;

  int protocol = msg->ExtractU16 ();
  version = msg->ExtractString ();
  msg->ExtractEnd ();
  return protocol;
}


//=========================================================================
// END
//=========================================================================
// vim:ts=2:sw=2:et:
