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
 *  @file dohost.h
 *
 *  Common functionality for all host types (client, server, leader, follower).
 *
 *  @author Marian Cerny
 *
 *  @date 2004
 */

#ifndef __dohost_h__
#define __dohost_h__


//=========================================================================
// Included files
//=========================================================================

#include "cfg.h"
#include "doalloc.h"

#include <atomic>
#include <string>
#include <vector>

#include "donet.h"


//=========================================================================
// Enumerations
//=========================================================================

/**
 *  All types of messages for network communication used by all host types.
 */
enum NetworkingProtocolMessage {
  net_protocol_chat_message = 0x01,   //!< Chat message.
  net_protocol_connect      = 0x02,   //!< Connect request.
  net_protocol_player_array = 0x03,   //!< Full info about players.
  net_protocol_hello        = 0x04,   //!< Hello message.
  net_protocol_change_race  = 0x05,   //!< Change race of a player.
  net_protocol_event        = 0x06,   //!< Event of unit.
  net_protocol_synchronise  = 0x07,   //!< Synchronisation.
  net_protocol_disconnect   = 0x08,   //!< Disconnect message.
  net_protocol_ping         = 0x09,   //!< Ping request.
  net_protocol_request_start = 0x0A,  //!< Follower asks leader to start the game.
  net_protocol_request_add_computer = 0x0B,  //!< Follower asks leader to add a CPU player.
  net_protocol_time_sync    = 0x0C,   //!< In-game clock sync (subtype 0 request, 1 reply).
  net_protocol_end
};

/** Version of the wire format. Hosts play together only when it matches: the
 *  follower sends it in connect, the leader in hello (THOST::PackVersion()). */
const int NET_PROTOCOL_VERSION = 2;


//=========================================================================
// THOST
//=========================================================================

const int all_players = 255;

/** A message the leader received from a follower, as the relay sees it (THOST::RouteRelay()). */
struct TRELAY_MESSAGE {
  T_BYTE type;
  T_BYTE dest;      //!< The header's dest byte as the follower sent it.
  int player_id;    //!< Event: its player id; disconnect: the departing player; otherwise -1.
  bool cross_owner; //!< Event: a request that may go to another player's unit (TEVENT::IsCrossOwnerRequest()).
};

/** What the leader does with a message received from a follower (THOST::RouteRelay()). */
struct TRELAY {
  bool local;          //!< Dispatch it on the leader.
  bool forward;        //!< Forward a copy to #to.
  TNET_CONN_ID to;     //!< The one connection to forward to; no_connection = every follower but the origin.
  int origin;          //!< Player id of the origin (the dest byte of the forwards), -1 = not a player.
  const char *drop;    //!< Why it is dropped (then neither local nor forward), else NULL.
  bool invalid;        //!< The drop counts towards eviction (net_max_invalid_messages): input
                       //!  no correct host sends, not a stale or misrouted message.
  bool departs;        //!< The origin's player leaves the game (his own disconnect).
};

/**
 *  Base class for a network communication participant (client, server). Its
 *  listener owns the connections; the route table maps player ids to them.
 *
 *  Route table: entry @c id is the connection of the host of player @c id
 *  (no_connection for a player of this host). Several players can share one
 *  connection (e.g. all players hosted on the leader). It is read and changed
 *  under a short lock of its own, which is never held while a message is
 *  queued, so any thread may send.
 *
 *  Relay (leader): every follower has one connection, to the leader; the leader forwards
 *  what one follower sends for the others (RouteRelay()) straight from the reader thread.
 */
class THOST {
public:
  /** Type of the host. */
  enum HostType {
    ht_leader,    //!< Leader (TLEADER).
    ht_follower,  //!< Follower (TFOLLOWER).
  };

public:
  THOST (int incoming_message_queue_size, in_port_t listen_port);

  THOST (int incoming_message_queue_size, in_port_t listen_port,
         in_addr remote_address, in_port_t remote_port);

  virtual ~THOST ();

  void Start ();
  void Stop ();

  void SendChatMessage (std::string chat_message, int sender);

  static void PackVersion (TNET_MESSAGE *msg);
  static int ExtractVersion (TNET_MESSAGE *msg, std::string &version);

  void SendDisconnect (T_BYTE player_id);

  void SendMessage (TNET_MESSAGE *msg);
  void SendToAll (TNET_MESSAGE *msg, TNET_CONN_ID except = no_connection, bool forward = false);

  /** Sends networking message to specified destination. If destination is
   *  all_players, the message is send to all connected hosts. */
  void SendMessage (TNET_MESSAGE *msg, T_BYTE destination) {
    msg->SetDest (destination);

    SendMessage (msg);
  }

  /** Returns type of the host. */
  HostType GetType ()
  { return type; }

  /** Returns listener. */
  TNET_LISTENER *GetListener ()
  { return listener; }

  /** Address of the host this one dialed at the start (the leader of a follower). */
  in_addr GetRemoteAddress () { return remote_address; }
  in_port_t GetRemotePort () { return remote_port; }
  /** The connection to it, fixed by Start() (no_connection for a leader, and while Start()
   *  adds it: its reader may already run then, but no other connection exists yet). */
  TNET_CONN_ID GetRemoteConnection () const { return remote_conn; }

  T_BYTE AddRoute (TNET_CONN_ID conn = no_connection);
  TNET_CONN_ID GetRoute (int id);
  void ClearRoute (int id);
  void RemoveRoute (int id);
  std::vector<TNET_CONN_ID> GetRoutedConnections ();

  void DisconnectAll ();

  int GetPlayerIDByConnection (TNET_CONN_ID conn, int from = 0);

  /** Leader: the game has started (the start player array is sent); in-game messages are
   *  relayed from now on. */
  void SetInGame () { in_game = true; }

  static TRELAY RouteRelay (const TRELAY_MESSAGE &m, TNET_CONN_ID from,
                            const std::vector<TNET_CONN_ID> &routes,
                            const std::vector<bool> &departed, bool in_game);
  TRELAY Relay (const TRELAY_MESSAGE &m, TNET_CONN_ID from);

  void RegisterExtendedFunction (T_BYTE type, void (*f)(TNET_MESSAGE *msg)) {
    handler->RegisterExtendedFunction (type, f);
  }

  /** Called in the dispatcher thread when a connection ended, after its messages. */
  void RegisterOnDisconnect (TNET_MESSAGE_HANDLER::DISCONNECT_HANDLER f) {
    handler->RegisterOnDisconnect (f);
  }

  /** Called in the reader threads for every received message, with this host as the
   *  context (TNET_LISTENER::RECEIVE_HANDLER). */
  void RegisterOnReceive (TNET_LISTENER::RECEIVE_HANDLER f) {
    listener->RegisterOnReceive (f, this);
  }

private:
  void Initialise (int incoming_message_queue_size, in_port_t listen_port);

  TNET_LISTENER *listener;        //!< Listener.
  TNET_MESSAGE_HANDLER *handler;  //!< Handler.
  TNET_DISPATCHER *dispatcher;    //!< Dispatcher.

  std::vector<TNET_CONN_ID> routes;   //!< Route table (see THOST), index = player id.
  std::vector<bool> departed;         //!< Index = player id: left the game (MarkDeparted()).
  SDL_mutex *routes_lock;             //!< Guards #routes and #departed.

  std::atomic<bool> in_game;          //!< See SetInGame().

  in_addr remote_address;
  in_port_t remote_port;
  int remote_fd;              //!< Dialed socket until Start() adds its connection, else -1.
  std::atomic<TNET_CONN_ID> remote_conn;   //!< See GetRemoteConnection().

protected:

  HostType type;    //!< Type of the host.
};


#endif

//=========================================================================
// END
//=========================================================================
// vim:ts=2:sw=2:et:

