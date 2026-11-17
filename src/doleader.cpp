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
 *  @file doserver.cpp
 *
 *  Server.
 *
 *  @author Marian Cerny
 *
 *  @date 2003, 2004
 */


//=========================================================================
// Included files
//=========================================================================

#include "cfg.h"
#include "doalloc.h"
#include "dotime.h"

#include <string>

#include "build_info.h"
#include "doleader.h"
#include "dohost.h"
#include "doengine.h"
#include "doplayers.h"

using std::string;


//=========================================================================
// TLEADER
//=========================================================================

/**
 *  Constructor.
 *
 *  @param incoming_message_queue_size Size of the incoming message queue.
 *  @param listen_port                 Port on which listener should listen.
 */
TLEADER::TLEADER (int incoming_message_queue_size, in_port_t listen_port)
:THOST (incoming_message_queue_size, listen_port)
{
  type = ht_leader;
}

void TLEADER::ConnectFollower (in_addr remote_address, in_port_t remote_port, TNET_CONN_ID conn)
{
  T_BYTE dest = AddRoute (conn);
  GetListener ()->Admit (conn);

  TNET_MESSAGE *m = pool_net_messages->GetFromPool();
  m->Init_send(net_protocol_hello, 0);

  m->Pack (&remote_address, sizeof remote_address);   // in_addr is in network byte order
  m->PackU16 (remote_port);
  m->PackDouble (AppGetTimeSeconds());
  PackVersion (m);

  SendMessage (m, dest);
}

/** Time to show a refused host the reply before its connection is closed (seconds).
 *  Clients before protocol version 2 crash when the connection ends during their connect
 *  pings (about 8 s). */
static const int refused_close_delay = 15;

/**
 *  Refuses the host on the connection @p conn, whose connect request has protocol
 *  version @p protocol (0 = before version 2). It gets only a reply that it can
 *  show; it is never added to the players and nothing is broadcast about it. The
 *  connection is closed refused_close_delay later.
 */
void TLEADER::RefuseFollower (TNET_CONN_ID conn, int protocol) {
  if (protocol == 0) {
    /* An older client understands only the version 1 formats. A lobby-only player
       array (all multi-byte fields 0, the same in any byte order) shows it a map that
       does not exist, so Play stays disabled; the chat line is attributed to the
       player "Server" hosted on the leader. */
    TNET_MESSAGE *m = pool_net_messages->GetFromPool();
    m->Init_send(net_protocol_player_array, 0);
    m->PackDouble (0);                        // time: ignored when not in the future
    m->PackString (string ("UPDATE CLIENT TO ") + DO_VERSION_STRING);
    m->PackU32 (0);                           // map hash
    m->PackByte (2);
    for (int i = 0; i < 2; i++) {             // the hyper player and a human on the leader
      m->PackString ("Server");
      m->PackString ("");
      m->PackByte (i == 0);                   // computer
      m->PackByte (0);                        // not remote: no address follows
      m->PackByte (0);                        // start point
    }
    m->SetReplyConnection (conn);
    SendMessage (m);

    m = pool_net_messages->GetFromPool();
    m->Init_send(net_protocol_chat_message, 0);
    m->PackString (string ("Your Dark Oberon is too old for this host: update to ") + DO_VERSION_STRING);
    m->SetReplyConnection (conn);
    SendMessage (m);
  } else {
    /* A newer client reads the version trailer of the hello and shows the versions. */
    in_addr none;
    none.s_addr = 0;

    TNET_MESSAGE *m = pool_net_messages->GetFromPool();
    m->Init_send(net_protocol_hello, 0);
    m->Pack (&none, sizeof none);
    m->PackU16 (0);
    m->PackDouble (0);
    PackVersion (m);
    m->SetReplyConnection (conn);
    SendMessage (m);
  }

  GetListener ()->CloseAfter (conn, refused_close_delay);
}

/**
 *  Answers a ping or time sync request (@p type) with @p request_time received on the
 *  connection @p conn: the echoed request time and our clock.
 */
void TLEADER::SendTimeReply (T_BYTE type, double request_time, TNET_CONN_ID conn) {
  TNET_MESSAGE *m = pool_net_messages->GetFromPool();
  m->Init_send(type, 1);

  m->PackDouble (request_time);
  m->PackDouble (AppGetTimeSeconds());

  m->SetReplyConnection (conn);
  SendMessage (m);
}

/**
 *  Sends the player list to all followers. Keep in sync with PlayerArrayFits()
 *  and NewPlayerArrayBytes() (doengine.cpp), which compute the size of this
 *  message so that a roster change that would not fit is refused.
 */
void TLEADER::SendPlayerArray (string map_name, bool start_game) {
  TNET_MESSAGE *m = pool_net_messages->GetFromPool();
  m->Init_send(net_protocol_player_array, start_game);

  player_array.Lock ();

  m->PackDouble (AppGetTimeSeconds());

  m->PackString (map_name);

  m->PackU32 (map_info_list.map_ext_info.file_hash);

  /* Count of players. */
  m->PackByte (static_cast<T_BYTE>(player_array.GetCount ()));

  for (int i = 0; i < player_array.GetCount(); i++) {
    m->PackString (player_array.GetPlayerName (i));
    m->PackString (player_array.GetRaceIdName (i));

    m->PackByte (static_cast<T_BYTE>(player_array.IsComputer (i)));
    m->PackByte (static_cast<T_BYTE>(player_array.IsRemote (i)));

    m->PackByte (static_cast<T_BYTE>(player_array.GetStartPoint (i)));

    if (player_array.IsRemote (i)) {
      in_addr addr = player_array.GetAddress (i);
      in_port_t port = player_array.GetPort (i);

      m->Pack (&addr, sizeof (in_addr));
      m->PackU16 (port);
    }
  }

  player_array.Unlock ();

  /* Relay the followers' game messages from now on: each one sends them only after this
     message arrives, so the relay must be ready before it goes out. */
  if (start_game)
    SetInGame ();

  SendMessage (m);
}

void TLEADER::SendAllowProcessFunction () {
  TNET_MESSAGE *m = pool_net_messages->GetFromPool();
  m->Init_send(net_protocol_synchronise, 1);
  SendMessage (m);
}


//=========================================================================
// END
//=========================================================================
// vim:ts=2:sw=2:et:

