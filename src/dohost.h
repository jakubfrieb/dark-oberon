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

/**
 *  Base class for a network communication participant (client, server). Its
 *  listener owns the connections; the route table maps player ids to them.
 *
 *  Route table: entry @c id is the connection of the host of player @c id
 *  (no_connection for a player of this host). Several players can share one
 *  connection (e.g. all players hosted on the leader). It is read and changed
 *  under a short lock of its own, which is never held while a message is
 *  queued, so any thread may send.
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

  void SendChatMessage (std::string chat_message);

  static void PackVersion (TNET_MESSAGE *msg);
  static int ExtractVersion (TNET_MESSAGE *msg, std::string &version);

  void SendDisconnect (T_BYTE player_id);

  void SendMessage (TNET_MESSAGE *msg);

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

  T_BYTE AddRoute (TNET_CONN_ID conn = no_connection);
  TNET_CONN_ID GetRoute (int id);
  void ClearRoute (int id);
  void RemoveRoute (int id);
  std::vector<TNET_CONN_ID> GetRoutedConnections ();

  void DisconnectAll ();

  int GetPlayerIDByConnection (TNET_CONN_ID conn, int from = 0);

  void RegisterExtendedFunction (T_BYTE type, void (*f)(TNET_MESSAGE *msg)) {
    handler->RegisterExtendedFunction (type, f);
  }

  void RegisterOnDisconnect (TNET_LISTENER::DISCONNECT_HANDLER f) {
    listener->RegisterOnDisconnect (f);
  }

private:
  void Initialise (int incoming_message_queue_size, in_port_t listen_port);

  TNET_LISTENER *listener;        //!< Listener.
  TNET_MESSAGE_HANDLER *handler;  //!< Handler.
  TNET_DISPATCHER *dispatcher;    //!< Dispatcher.

  std::vector<TNET_CONN_ID> routes;   //!< Route table (see THOST), index = player id.
  SDL_mutex *routes_lock;             //!< Guards #routes.

  in_addr remote_address;
  in_port_t remote_port;

protected:
  TNET_CONN_ID remote_conn;   //!< Connection dialed at the start (see GetRemoteAddress()).

  HostType type;    //!< Type of the host.
};


#endif

//=========================================================================
// END
//=========================================================================
// vim:ts=2:sw=2:et:

