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
 *  @file doclient.cpp
 *
 *  Client side of the client-server communication.
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
#include "dotime.h"

#include <algorithm>
#include <string>
#include <string.h>

#include "dofollower.h"
#include "donet.h"

using std::string;


//=========================================================================
// TFOLLOWER
//=========================================================================

/**
 *  Constructor.
 *
 *  @param incoming_message_queue_size Size of the incoming message queue.
 *  @param listen_port                 The port on which #listener should
 *                                     listen.
 *  @param outgoing_message_queue_size Size of the outgoing message queue.
 *  @param remote_address              Address of the leader.
 *  @param remote_port                 Port of the leader.
 */
TFOLLOWER::TFOLLOWER (int incoming_message_queue_size, in_port_t listen_port,
                      int outgoing_message_queue_size, in_addr remote_address,
                      in_port_t remote_port)
:THOST (incoming_message_queue_size, listen_port, outgoing_message_queue_size,
        remote_address, remote_port)
{
  type = ht_follower;
  have_my_address = false;
  // Initialise to a well-known sentinel so that callers who forget to check
  // HasMyAddress() at least see a deterministic 0.0.0.0:0 instead of garbage.
  memset (&my_address, 0, sizeof my_address);
  my_port = 0;

  admitted = false;
  leader_fd = GetTalker ()->GetRemoteFiledescriptor (0);
  time_samples_total = 0;
  time_sync_sent = 0;
}

/**
 *  Connects the follower to the leader.
 *
 *  @param player_name Name of the player to connect.
 */
void TFOLLOWER::Connect (string player_name) {
  in_port_t listener_port = GetListener ()-> GetPort ();

  TNET_MESSAGE *m = pool_net_messages->GetFromPool();
  m->Init_send(net_protocol_connect, 0);
  m->PackU16 (listener_port);
  m->PackString (player_name);
  PackVersion (m);

  ping_request_time = AppGetTimeSeconds();

  /* Send message to leader. */
  SendMessage (m, 0);
}

/**
 *  Sends a message to change race of a player.
 *
 *  @param index       Index of the player.
 *  @param player_name Name of the player to connect.
 *  @param new_race    Id name of the new race.
 */
void TFOLLOWER::ChangeRace (int index, string player_name, string new_race) {
  TNET_MESSAGE *m = pool_net_messages->GetFromPool();
  m->Init_send(net_protocol_change_race, 0);
  m->PackByte (static_cast<T_BYTE>(index));
  m->PackString (player_name);
  m->PackString (new_race);

  /* Send message to leader. */
  SendMessage (m, 0);
}

void TFOLLOWER::SetMyAddress (in_addr address, in_port_t port) {
  my_address = address;
  my_port = port;

  have_my_address = true;
}

void TFOLLOWER::SendPingRequest () {
  TNET_MESSAGE *msg = pool_net_messages->GetFromPool();
  msg->Init_send(net_protocol_ping, 0);

  ping_request_time = AppGetTimeSeconds();
  msg->PackDouble (ping_request_time);

  SendMessage (msg, 0);
}

/** Interval of in-game time sync requests (seconds). */
static const double time_sync_period = 2.0;

/**
 *  Adds a sample: round trip @p rtt and @p offset (leader's clock minus ours, both
 *  seconds), and returns the correction to apply to our clock now.
 */
double TCLOCK_FILTER::AddSample (double rtt, double offset) {
  samples[next].rtt = rtt;
  samples[next].offset = offset;
  next = (next + 1) % samples_count;
  if (used < samples_count)
    used++;

  int best = 0;
  for (int i = 1; i < used; i++)
    if (samples[i].rtt < samples[best].rtt)
      best = i;

  double step = std::max (-max_step, std::min (max_step, samples[best].offset));

  for (int i = 0; i < used; i++)   // the offsets are relative to our (corrected) clock
    samples[i].offset -= step;

  this->offset = samples[best].offset;
  return step;
}

/**
 *  Called every simulation step during a game: asks the leader for its time every
 *  time_sync_period. The request carries our raw clock, the leader echoes it.
 */
void TFOLLOWER::TimeSyncTick () {
  double now = AppGetRawSeconds ();
  if (now - time_sync_sent < time_sync_period)
    return;
  time_sync_sent = now;

  TNET_MESSAGE *msg = pool_net_messages->GetFromPool();
  msg->Init_send(net_protocol_time_sync, 0);
  msg->PackDouble (now);
  SendMessage (msg, 0);
}

/**
 *  Corrects our clock by a time sync reply (TCLOCK_FILTER). Like the connect-time
 *  ping, the reply is assumed to take half the round trip time.
 *
 *  @param request_time AppGetRawSeconds() when the request was sent (echoed).
 *  @param leader_time  Leader's clock when it answered.
 */
void TFOLLOWER::ProcessTimeSyncReply (double request_time, double leader_time) {
  double rtt = AppGetRawSeconds () - request_time;

  if (!(rtt >= 0 && rtt < 2 * time_sync_period)) {   // not one of our recent requests
    NetDropWarning (LogMsg ("Ignoring time sync reply (round trip %.3f s)", rtt));
    return;
  }

  double step = clock_filter.AddSample (rtt, leader_time + rtt / 2 - AppGetTimeSeconds ());
  AppAdjustTimeSeconds (step);

  /* The first sample and then one a minute: the clock offset over a long game. */
  if (time_samples_total++ % 30 == 0)
    Info (LogMsg ("Time sync: round trip %.2f ms, clock offset to the leader %.2f ms (corrected %.2f ms)",
                  rtt * 1000, (clock_filter.GetOffset () + step) * 1000, step * 1000));
}

void TFOLLOWER::SendSynchronise () {
  TNET_MESSAGE *msg = pool_net_messages->GetFromPool();
  msg->Init_send(net_protocol_synchronise, 0);
  SendMessage (msg, 0);
}

void TFOLLOWER::SendRequestStartGame () {
  TNET_MESSAGE *m = pool_net_messages->GetFromPool();
  m->Init_send(net_protocol_request_start, 0);
  SendMessage (m, 0);
}

void TFOLLOWER::SendRequestAddComputer () {
  TNET_MESSAGE *m = pool_net_messages->GetFromPool();
  m->Init_send(net_protocol_request_add_computer, 0);
  SendMessage (m, 0);
}

//=========================================================================
// END
//=========================================================================
// vim:ts=2:sw=2:et:

