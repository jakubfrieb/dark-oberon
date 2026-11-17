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
#include "utils.h"

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
  remote_fd = -1;
  remote_conn = no_connection;

  Initialise (incoming_message_queue_size, listen_port);
}

/**
 *  Constructor of a host that connects to @p remote_address:@p remote_port
 *  first (throws TNET_LISTENER::ConnectingErrorException), the host of player 0.
 *  The connection is read from Start() on.
 */
THOST::THOST (int incoming_message_queue_size, in_port_t listen_port,
              in_addr remote_address, in_port_t remote_port)
{
  remote_fd = TNET_LISTENER::DialSocket (remote_address, remote_port);
  remote_conn = no_connection;

  this->remote_address = remote_address;
  this->remote_port = remote_port;

  Initialise (incoming_message_queue_size, listen_port);
}

void THOST::Initialise (int incoming_message_queue_size, in_port_t listen_port)
{
  routes_lock = SDL_CreateMutex ();
  if (routes_lock == NULL)
    Critical ("Could not create mutex");
  in_game = false;

  listener = NEW TNET_LISTENER (incoming_message_queue_size, listen_port);
  handler = NEW TNET_MESSAGE_HANDLER ();
  dispatcher = NEW TNET_DISPATCHER (listener->GetMessageQueue (), handler);

  listener->ConsumerIsAttached (dispatcher->GetThread ());
}

/**
 *  Starts receiving: accepts connections and reads the dialed one, which becomes the route of
 *  player 0 (the hyper player is on the remote host). Call it once, after the handlers are
 *  registered (RegisterOnReceive(), RegisterOnDisconnect(), RegisterExtendedFunction()), so no
 *  message gets past them.
 */
void THOST::Start ()
{
  /* The dialed connection is read before its handle is known here: until then it is the only
     connection (accepting starts after it), see GetRemoteConnection(). */
  if (remote_fd >= 0) {
    int fd = remote_fd;
    remote_fd = -1;
    remote_conn = listener->AddDialedConnection (remote_address, remote_port, fd);
    AddRoute (remote_conn);
  }

  listener->Start ();
}

/**
 *  Stops the dispatcher, then the connections (their reader threads call the receive handler).
 *  The destructor of a derived host calls it first, so neither runs while that host is
 *  partly destroyed. It can be called more than once.
 */
void THOST::Stop ()
{
  if (listener == NULL)
    return;

  /* Stop the dispatcher before the connections go away: some handlers (time sync) send
   * replies without giant, so they do not see host set to NULL. */
  listener->GetMessageQueue ()->Die ();
  SDL_WaitThread (dispatcher->GetThread (), NULL);
  dispatcher->ThreadJoined ();
  listener->ConsumerIsAttached (NULL);

  delete listener;
  listener = NULL;
}

THOST::~THOST ()
{
  Stop ();

  /* The listener goes first (Stop()), then the dispatcher and the handler. */
  delete dispatcher;
  delete handler;

  if (remote_fd >= 0)   // never started
    do_close (remote_fd);

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
  else if (msg->GetDest () == all_players)
    SendToAll (msg);
  else {
    TNET_CONN_ID conn = GetRoute (msg->GetDest ());
    if (conn != no_connection)
      listener->Send (conn, msg);
    else
      NetDropWarning (LogMsg ("Dropping message (type %d) for player %d, who has no connection",
                              msg->GetType (), msg->GetDest ()));
  }

  pool_net_messages->PutToPool (msg);
}

/**
 *  Queues a copy of @p msg (which the caller keeps) once for every connection of the route
 *  table but @p except, whatever its destination is. A @p forward is measured as one
 *  (TNET_LISTENER::Send()).
 */
void THOST::SendToAll (TNET_MESSAGE *msg, TNET_CONN_ID except, bool forward) {
  std::vector<TNET_CONN_ID> conns = GetRoutedConnections ();
  for (unsigned i = 0; i < conns.size (); i++)
    if (conns[i] != except)
      listener->Send (conns[i], msg, forward);
}

/** Adds the route of the next player id (@p conn = no_connection for a player of this host);
 *  returns the id. */
T_BYTE THOST::AddRoute (TNET_CONN_ID conn) {
  SDL_LockMutex (routes_lock);
  routes.push_back (conn);
  departed.push_back (false);
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

/** Player @p id has no connection any more (it ended) and has left; the ids stay valid. */
void THOST::ClearRoute (int id) {
  SDL_LockMutex (routes_lock);
  if (id >= 0 && id < (int)routes.size ()) {
    routes[id] = no_connection;
    departed[id] = true;
  }
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
    departed.erase (departed.begin () + id);
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
 *  Decides what the leader does with a message received from a follower on the connection
 *  @p from (docs/NETWORK_RELAY_PLAN.md, section 5), from the route table @p routes, the
 *  players who left (@p departed) and whether the game has started. The caller has checked
 *  the payload (exact length, ValidateNetEvent()). Identity comes only from @p from: the
 *  origin is the first player routed to it (a follower hosts one human), and the dest byte
 *  only tells a broadcast from a request.
 *
 *  - Not from a player yet (before the dispatcher admitted it, refused hosts): only connect
 *    and ping, local only, the handlers check them; anything else counts as invalid (a
 *    follower sends nothing else before its hello, so a connect pipelined with game or
 *    departure messages cannot pass them before it is admitted).
 *  - Event, only in the game:
 *    - broadcast (a unit state or a request about the sender's own unit): its player must
 *      be the origin's; dispatched locally (the leader keeps replicas) and forwarded to
 *      every follower but the origin;
 *    - request to the owner of a unit (dest = that owner), only of the kinds sent to another
 *      player's unit (TRELAY_MESSAGE::cross_owner): local when the owner is on the leader,
 *      else forwarded to the owner's connection only.
 *  - Chat: local and to every follower but the origin, the origin id in the dest byte.
 *  - Disconnect: only of the origin's own player, local only: the leader announces the
 *    departure itself (ProcessDisconnect()). In the game the player has left from now on
 *    (#departs), so nothing he sends after it is relayed.
 *  - Everything else (lobby, ping, time sync, ...): local only.
 *
 *  A misrouted event is dropped but does not count as invalid: a replica that sends for a
 *  unit it does not own is a bug of an honest host, and evicting it would end the game for
 *  that player. Events of a player who has left (ClearRoute(), #departs) are stale.
 */
TRELAY THOST::RouteRelay (const TRELAY_MESSAGE &m, TNET_CONN_ID from,
                          const std::vector<TNET_CONN_ID> &routes,
                          const std::vector<bool> &departed, bool in_game) {
  TRELAY r = { true, false, no_connection, -1, NULL, false, false };

  const int count = (int)routes.size ();
  for (int i = 0; from != no_connection && i < count; i++)
    if (routes[i] == from) {
      r.origin = i;
      break;
    }

  const bool known = m.player_id >= 0 && m.player_id < count;
  const bool own = known && routes[m.player_id] == from;

  if (r.origin == -1) {
    if (m.type != net_protocol_connect && m.type != net_protocol_ping) {
      r.drop = "not a player";
      r.invalid = true;
    }
  } else switch (m.type) {
  case net_protocol_event:
    if (!in_game)
      r.drop = "event before the game";
    else if (!known)
      r.drop = "player";
    else if (departed[m.player_id])
      r.drop = "player has left";
    else if (m.dest == all_players) {
      if (!own)
        r.drop = "broadcast for another player";
      else
        r.forward = true;
    } else {
      const TNET_CONN_ID owner = routes[m.player_id];
      if (!m.cross_owner || m.dest != m.player_id)
        r.drop = "event for another player";
      else if (owner == from)
        r.drop = "request for an own unit";
      else if (owner != no_connection) {
        r.local = false;
        r.forward = true;
        r.to = owner;
      }
    }
    break;

  case net_protocol_chat_message:
    r.forward = true;
    break;

  case net_protocol_disconnect:
    if (!own) {
      r.drop = "departure of another player";
      r.invalid = true;
    } else
      r.departs = in_game;
    break;

  default:
    break;
  }

  if (r.drop != NULL)
    r.local = r.forward = false;

  return r;
}

/** RouteRelay() over the current route table; marks a departure (TRELAY::departs) in the same hold. */
TRELAY THOST::Relay (const TRELAY_MESSAGE &m, TNET_CONN_ID from) {
  SDL_LockMutex (routes_lock);

  TRELAY r = RouteRelay (m, from, routes, departed, in_game);
  if (r.departs)
    departed[m.player_id] = true;

  SDL_UnlockMutex (routes_lock);

  return r;
}

/**
 *  Sends chat message to all connected hosts.
 *
 *  @param chat_message Message content.
 *  @param sender       Player id of the sender: the dest byte carries it (the leader
 *                      replaces it with the origin when it forwards a follower's chat).
 */
void THOST::SendChatMessage (string chat_message, int sender) {
  TNET_MESSAGE *m = pool_net_messages->GetFromPool();
  m->Init_send(net_protocol_chat_message, 0, static_cast<T_BYTE>(sender));
  m->PackString (chat_message);

  SendToAll (m);
  pool_net_messages->PutToPool (m);
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
