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
 *  @file donet.cpp
 *
 *  Networking functions.
 *
 *  @author Marian Cerny
 *
 *  @date 2003, 2004
 */

#include "cfg.h"
#include "doalloc.h"

#include <cstdlib>
#include <string>
#include <vector>
#include <atomic>
#include <ctime>

#ifdef UNIX
# include <cerrno>
# include <signal.h>
#endif
#ifndef WINDOWS
# include <netinet/tcp.h>
#endif

#include "dologs.h"
#include "donet.h"
#include "doperf.h"
#include "dosimpletypes.h"
#include "dotime.h"
#include "utils.h"

using std::string;
using std::vector;


//=========================================================================
// Global Variables
//=========================================================================

/**
 *  String used for error message produced by macro SOCKET_ERROR_MESSAGE().
 */
thread_local TLOG_MESSAGE socket_error_message;

/** Warnings about dropped messages logged per window; the rest is counted. */
static const int net_drop_warnings_per_window = 20;
static const int net_drop_window_seconds = 60;
static std::atomic<long> net_drop_window (0);
static std::atomic<int> net_drop_count (0);

/**
 *  Logs a warning about a dropped network message. Anyone can connect to the
 *  game port, so after net_drop_warnings_per_window warnings in one window the
 *  other drops are only counted; the count is logged with the first drop of a
 *  later window.
 */
void NetDropWarning (const char *text)
{
  const std::string copy (text);   // LogMsg() output: the Warning() below may reuse its buffer
  const long window = static_cast<long> (time (NULL) / net_drop_window_seconds);
  long old_window = net_drop_window.load ();

  if (old_window != window && net_drop_window.compare_exchange_strong (old_window, window)) {
    const int not_logged = net_drop_count.exchange (0) - net_drop_warnings_per_window;
    if (not_logged > 0)
      Warning (LogMsg ("%d more dropped network messages were not logged", not_logged));
  }

  if (net_drop_count.fetch_add (1) < net_drop_warnings_per_window)
    Warning (copy.c_str ());
}

TPOOL<TNET_MESSAGE> * pool_net_messages;


//=========================================================================
// Socket functions
//=========================================================================

/**
 *  Game connections cross NATs and firewalls, which forget connections that stay quiet for a
 *  while. TCP keepalive keeps them alive and detects a dead peer in about a minute (30 s idle,
 *  then 6 probes 10 s apart) instead of the system default of hours. Where the fine-grained
 *  options are missing (Windows), only the system default keepalive is switched on.
 */
static void EnableKeepalive (int fd) {
  int on = 1;
  setsockopt (fd, SOL_SOCKET, SO_KEEPALIVE, (const char *)&on, sizeof (on));
#if defined(TCP_KEEPIDLE) && defined(TCP_KEEPINTVL) && defined(TCP_KEEPCNT)
  int idle = 30, interval = 10, count = 6;
  setsockopt (fd, IPPROTO_TCP, TCP_KEEPIDLE, (const char *)&idle, sizeof (idle));
  setsockopt (fd, IPPROTO_TCP, TCP_KEEPINTVL, (const char *)&interval, sizeof (interval));
  setsockopt (fd, IPPROTO_TCP, TCP_KEEPCNT, (const char *)&count, sizeof (count));
#endif
}

/**
 *  Game messages are small and latency matters more than their count: send each one at once
 *  instead of letting Nagle's algorithm hold it back until the previous one is acknowledged.
 */
static void EnableNoDelay (int fd) {
  int on = 1;
  setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof (on));
}

/**
 *  Initializes sockets, which are used for networking.
 *
 *  @return @c true when no error, @c false otherwise.
 *
 *  @note On Windows it calls WSAStartup() and must be called for every thread
 *  using sockets. On Unix it ignores the SIGPIPE signal; it should be called
 *  once but may be called repeatedly.
 */
bool init_sockets (void) {
#ifdef WINDOWS
  WSADATA data;

  /* Init Windows sockets. */
  if (WSAStartup (MAKEWORD (1,1), &data) != 0) {
    Critical ("Can not initialize sockets");
    return false;
  }

  /* Write status about the Windows sockets. */
  Info (LogMsg ("%s (version %d.%d) %s",
                data.szDescription,
                LOBYTE (data.wVersion),
                HIBYTE (data.wVersion),
                data.szSystemStatus));
#endif

#ifdef UNIX
  /* Avoid "Broken pipe". */
  signal (SIGPIPE, SIG_IGN);
#endif

  return true;
}

/**
 *  Ends the work with network sockets.
 *
 *  @note This function is actualy needed only for #WINDOWS.
 */
void end_sockets (void) 
{
#ifdef WINDOWS
  WSACleanup ();
#endif
}


//=========================================================================
// TNET_MESSAGE
//=========================================================================


TNET_MESSAGE::TNET_MESSAGE() {}

/**
 *  Creates a message with with type @p type and subtype @p subtype.
 */
void TNET_MESSAGE::Init_send (T_BYTE type, T_BYTE subtype, T_BYTE dest)
{
  // Length of the message header size
  this->size = GetHeaderSize ();

  this->type = type;
  this->subtype = subtype;
  this->dest = dest;
  this->conn = no_connection;
  connection_ended = false;

  extract_p = 0;
  extract_error = false;
}

/**
 *  Creates a message from a packed received from network.
 *
 *  @param address Address of the sender.
 *  @param port    Port of the sender.
 *  @param conn    Connection the message was received on.
 *  @param data    Data of the message including the header.
 */
void TNET_MESSAGE::Init_receive (in_addr address, in_port_t port, TNET_CONN_ID conn, T_BYTE *data) {
  // The first byte is the message size header (see TNET_LISTENER::reader_thread_function).
  // Copy only the bytes that are actually present, not the full max_net_message_size.
  T_BYTE msg_size = data[0];
  if (msg_size < GetHeaderSize () || msg_size > max_net_message_size) {
    Critical (LogMsg ("Init_receive: invalid message size %d", msg_size));
    throw 0;
  }
  memcpy (&this->size, data, msg_size);

  this->address = address;
  this->port = port;
  this->conn = conn;
  received_at = AppGetTimeSeconds ();
  connection_ended = false;

  extract_p = 0;
  extract_error = false;
}

/** Creates the notice that the connection @p conn ended (IsConnectionEnded()). */
void TNET_MESSAGE::Init_connection_ended (TNET_CONN_ID conn) {
  Init_send (0, 0);
  this->conn = conn;
  connection_ended = true;
}


//=========================================================================
// TNET_MESSAGE_QUEUE
//=========================================================================

/**
 *  Constructor which creates array for networking messages of size @p size. It
 *  also creates mutex and conditional variables.
 */
TNET_MESSAGE_QUEUE::TNET_MESSAGE_QUEUE (int size) {
  message = NEW TNET_MESSAGE *[size];
  
  for (int i = 0; i < size; i++)
    message[i] = NULL;

  this->size = size;
  count = 0;
  head = 0;

  dead = false;

  mutex = SDL_CreateMutex ();

  is_not_empty = SDL_CreateCond ();
  is_not_full  = SDL_CreateCond ();

  if (mutex == NULL || is_not_empty == NULL || is_not_full == NULL)
    throw MutexException ();
}

TNET_MESSAGE_QUEUE::~TNET_MESSAGE_QUEUE () {
  Die ();

  delete[] message;
  SDL_DestroyCond (is_not_full);
  SDL_DestroyCond (is_not_empty);
  SDL_DestroyMutex (mutex);
}

/**
 *  Gets one message from the queue. When there are no messages in the queue,
 *  this function blocks the actual thread and waits until a new message is
 *  inserted into the queue using PutMessage().
 */
TNET_MESSAGE *TNET_MESSAGE_QUEUE::GetMessage () {
  SDL_LockMutex (mutex);

  while (count == 0) {
    SDL_CondWait (is_not_empty, mutex);
  }

  if (dead) {
    SDL_UnlockMutex (mutex);
    return NULL;
  }

  TNET_MESSAGE *ret = message[head];
  message[head] = (TNET_MESSAGE *)1;
  head = (head + 1) % size;
  count--;

  SDL_UnlockMutex (mutex);

  SDL_CondSignal (is_not_full);

  return ret;
}

/**
 *  Inserts a new message into the queue. When the queue is full, this function
 *  blocks the actual thread and waits until a message is removed from the
 *  queue using GetMessage(). After Die() the message goes back to the pool.
 */
void TNET_MESSAGE_QUEUE::PutMessage (TNET_MESSAGE *message) {
  SDL_LockMutex (mutex);

#if DEBUG
  if (count == size)
    Debug ("MessageQueue limit achieved");
#endif

  while (count == size && !dead)
    SDL_CondWait (is_not_full, mutex);

  if (dead) {   // nobody takes it any more (Die())
    SDL_UnlockMutex (mutex);
    pool_net_messages->PutToPool (message);
    return;
  }

  int tail = (head + count) % size;
  this->message[tail] = message;
  count++;

  SDL_UnlockMutex (mutex);

  SDL_CondSignal (is_not_empty);
}

void TNET_MESSAGE_QUEUE::Die () {
  SDL_LockMutex (mutex);
  dead = true;
  count = 1;
  SDL_UnlockMutex (mutex);

  /* Wake up consumer. GetMessage will return NULL when dead. Producers blocked on a full
     queue give their message back (PutMessage()). */
  SDL_CondSignal (is_not_empty);
  SDL_CondBroadcast (is_not_full);
}


//=========================================================================
// TNET_LISTENER
//=========================================================================

/**
 *  Constructor which creates new queue for incoming messages and new thread
 *  for listener.
 *
 *  @param queue_size Size of the incoming message queue.
 *  @param port       Port on which the listener should listen.
 */
TNET_LISTENER::TNET_LISTENER (int queue_size, in_port_t port) {
  this->port = port;
  fd = -1;
  incoming_messages = NEW TNET_MESSAGE_QUEUE (queue_size);
  consumer_thread = NULL;

  on_receive = NULL;
  on_receive_context = NULL;
  connections_lock = SDL_CreateMutex ();
  bind_sem = SDL_CreateSemaphore (0);
  start_sem = SDL_CreateSemaphore (0);
  listener_bind_ok = false;

  if (bind_sem == NULL || start_sem == NULL)
    Critical ("Listener: SDL_CreateSemaphore failed");

  thread = SDL_CreateThread (listener_thread_function, "net_listen", this);

  if (thread == NULL) {
    SDL_DestroySemaphore (bind_sem);
    bind_sem = NULL;
    Critical ("Could not create listener thread");
  }

  SDL_SemWait (bind_sem);

  if (!listener_bind_ok) {
    SDL_SemPost (start_sem);
    SDL_WaitThread (thread, NULL);
    thread = NULL;
    SDL_DestroySemaphore (bind_sem);
    bind_sem = NULL;
    Critical ("Listener: could not bind (port in use or permission denied). "
              "If you host a server on the same machine, use a follower listen port of 0 (auto).");
  }
}

TNET_LISTENER::~TNET_LISTENER () {
  incoming_messages->Die ();

  /* Take the listening socket over (the thread sees -1) and close it: on Haiku only close()
     wakes up the blocked accept(). */
  SDL_LockMutex (connections_lock);
  int listening = fd;
  fd = -1;
  SDL_UnlockMutex (connections_lock);

  if (listening >= 0) {
    shutdown (listening, 2);
    do_close (listening);
  }
  SDL_SemPost (start_sem);   // never started: it sees the closed socket and finishes

  /* Wait until listener thread is dead. */
  if (thread)
    SDL_WaitThread (thread, NULL);

  /* No new connections now. Stop the readers and wait for them outside the lock (each one
     stops its writer and closes its socket), then forget the connections. */
  vector<SDL_Thread *> readers;

  SDL_LockMutex (connections_lock);
  for (unsigned i = 0; i < connections.size (); i++) {
    if (connections[i]->state == CONNECTION::open)
      shutdown (connections[i]->fd, 2);
    readers.push_back (connections[i]->reader);
  }
  SDL_UnlockMutex (connections_lock);

  for (unsigned i = 0; i < readers.size (); i++)
    SDL_WaitThread (readers[i], NULL);

  for (unsigned i = 0; i < connections.size (); i++)
    DeleteConnection (connections[i]);
  connections.clear ();

  /* Wait until consumer of incoming_messages is dead. */
  if (consumer_thread)
    SDL_WaitThread (consumer_thread, NULL);

  if (bind_sem) {
    SDL_DestroySemaphore (bind_sem);
    bind_sem = NULL;
  }
  SDL_DestroySemaphore (start_sem);

  delete incoming_messages;
  SDL_DestroyMutex (connections_lock);
}

/**
 *  Listener's thread function.
 *
 *  @param listener_class Pointer to class instance to which the thread
 *                        belongs.
 */
int SDLCALL TNET_LISTENER::listener_thread_function (void *listener_class) {
  struct sockaddr_in address;

  TNET_LISTENER *self = (TNET_LISTENER *)listener_class;

  (void)init_sockets (); /* Casting to void does not produce warning on UNIX. */

  int &fd = self->fd;

  if ((fd = socket (PF_INET, SOCK_STREAM, 0)) == -1) {
    Error (SOCKET_ERROR_MESSAGE ("Listener: Calling socket failed"));
    self->fd = -1;
    self->listener_bind_ok = false;
    SDL_SemPost (self->bind_sem);
    return 0;
  }

  address.sin_family = AF_INET;
  address.sin_port = htons (self->port);
  address.sin_addr.s_addr = INADDR_ANY;     /* local address */
  memset (address.sin_zero, '\0', 8);       /* clear the rest of the structure */

#ifdef WINDOWS
  char yes = 1;
#else
  int yes = 1;
#endif

  /* Avoid "Address already in use" error. */
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (yes)) == -1)
    Warning (SOCKET_ERROR_MESSAGE ("Listener: Error setting option REUSEADDR"));

  if (bind (fd, (struct sockaddr *)&address, sizeof (address)) == -1) {
    Error (SOCKET_ERROR_MESSAGE ("Listener: Error binding socket to address"));
    do_close (fd);
    self->fd = -1;
    self->listener_bind_ok = false;
    SDL_SemPost (self->bind_sem);
    return 0;
  }

  if (self->port == 0) {
    struct sockaddr_in sa;
    socklen_t slen = sizeof (sa);
    if (getsockname (fd, (struct sockaddr *)&sa, &slen) != -1)
      self->port = ntohs (sa.sin_port);
  }

  self->listener_bind_ok = true;
  SDL_SemPost (self->bind_sem);

  /* Accept only once the owner has registered its handlers (Start()). */
  SDL_SemWait (self->start_sem);

  if (!self->IsStopping ()) {
    Info (LogMsg ("Listener running on port %hu", self->port));
    listen (fd, 10);
  }

  /* The port is public: an error (out of descriptors in a connection flood, a connection reset
     before it was accepted) must not stop accepting players. Only the destructor stops it. */
  while (!self->IsStopping ()) {
    struct sockaddr_in remote_addr;
    socklen_t sockaddr_size = sizeof (remote_addr);
    int new_fd;

    if ((new_fd = accept (fd, (sockaddr *)&remote_addr, &sockaddr_size)) == -1) {
      if (!self->IsStopping ()) {
        NetDropWarning (SOCKET_ERROR_MESSAGE ("Listener: Error accepting connection"));
        SDL_Delay (10);
      }
      continue;
    }

    try {
      self->AddConnection (new_fd, remote_addr, false);
    } catch (MutexException &) {   // no threads left: AddConnection() has closed it
      NetDropWarning ("Listener: could not start the threads of a connection");
      SDL_Delay (10);
    }
  }

  SDL_LockMutex (self->connections_lock);
  if (fd >= 0) {   // not taken over by the destructor
    do_close (fd);
    fd = -1;
  }
  SDL_UnlockMutex (self->connections_lock);

  end_sockets ();

  Info ("Listener finished");
  return 0;
}

/** True when the last recv() failed because of the socket's receive timeout (SO_RCVTIMEO). */
static bool ReceiveTimedOut () {
#ifdef WINDOWS
  return WSAGetLastError () == WSAETIMEDOUT;
#else
  return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

/**
 *  Reader of a connection: receives its messages, passes each one to the receive handler and
 *  into the incoming message queue, until the connection ends. Then it ends it
 *  (ConnectionEnded()) and queues the notice of that behind the messages.
 */
int SDLCALL TNET_LISTENER::reader_thread_function (void *connection) {
  CONNECTION *c = static_cast<CONNECTION *>(connection);
  TNET_LISTENER *self = c->listener;

  /* Connections come and go rarely, so they are logged at Info level: when a player drops out,
     the log says who and why. */
  const char *peer = c->peer.c_str ();
  Info (LogMsg ("Listener: connection from %s", peer));

  int fd = c->fd;
  T_BYTE buf[max_net_message_size];
  T_BYTE *size = buf;
  int pos;
  int len;
  int invalid = 0;

  /* A connection that is not admitted in time ends (SetDeadline()): its socket has a receive
     timeout, and a peer that keeps sending is checked after every receive. A receive that times
     out after the connection was admitted (or got a later deadline) is repeated. Returns what
     recv() does, or -2 when the deadline has passed. */
  auto receive = [c, fd](T_BYTE *data, int bytes) {
    while (1) {
      int got = recv (fd, reinterpret_cast<char*>(data), bytes, 0);
      double deadline = c->deadline;
      if (deadline > 0 && AppGetRawSeconds () >= deadline)
        return -2;
      if (got != -1 || !ReceiveTimedOut ())
        return got;
    }
  };

  while (1) {
    if ((pos = receive (buf, 1)) == -2) {
      Info (LogMsg ("Listener: dropping connection from %s: it did not join in time", peer));
      break;
    }

    if (pos == -1) {
      /* Reported like a closed connection (ConnectionEnded below). */
      Warning (SOCKET_ERROR_MESSAGE (LogMsg ("Listener: connection from %s lost", peer)));
      break;
    }

    if (pos == 0) {
      Info (LogMsg ("Listener: %s closed the connection", peer));
      break;
    }

    /* The port is public: anything (port scanners, stray HTTP requests) can connect. A bad size
       byte or a connection that ends mid-message drops only that connection; passing the partial
       message on used to throw from Init_receive and terminate the whole server. */
    if (*size < TNET_MESSAGE::GetHeaderSize ()) {
      Warning (LogMsg ("Listener: dropping connection from %s: invalid message size %d",
                       peer, *size));
      break;
    }

    bool complete = true;
    do {
      if ((len = receive (buf + pos, *size - pos)) <= 0) {
        if (len == -2)
          Info (LogMsg ("Listener: dropping connection from %s: it did not join in time", peer));
        else if (len < 0)
          Warning (SOCKET_ERROR_MESSAGE (LogMsg ("Listener: connection from %s lost in the middle of a message", peer)));
        else
          Warning (LogMsg ("Listener: %s closed the connection in the middle of a message", peer));
        complete = false;
        break;
      }

      pos += len;
    } while (pos != *size);

    if (!complete)
      break;

    TNET_MESSAGE *msg = pool_net_messages->GetFromPool();
    msg->Init_receive(c->address, htons (c->port), c->id, buf);

    RECEIVE_HANDLER on_receive = self->on_receive;
    ReceiveResult result = on_receive ? on_receive (self->on_receive_context, msg) : receive_dispatch;

    if (result == receive_dispatch)
      self->incoming_messages->PutMessage (msg);
    else
      pool_net_messages->PutToPool (msg);

    if (result == receive_invalid && ++invalid >= net_max_invalid_messages) {
      Warning (LogMsg ("Listener: dropping connection from %s: %d invalid messages", peer, invalid));
      break;
    }
  }

  self->ConnectionEnded (c);

  TNET_MESSAGE *ended = pool_net_messages->GetFromPool();
  ended->Init_connection_ended (c->id);
  self->incoming_messages->PutMessage (ended);

  Debug ("Listener's subthread finished");
  return 0;
}

/** Most bytes the writer sends at once. */
static const int max_write_size = 16 * 1024;

/** Sends all @p data over the socket @p fd; false on an error. */
static bool SendAll (int fd, const string &data) {
  for (size_t pos = 0; pos < data.size (); ) {
    int len = send (fd, data.data () + pos, data.size () - pos, 0);
    if (len <= 0)
      return false;
    pos += len;
  }
  return true;
}

/**
 *  Writer of a connection: the only code that writes to its socket. It writes the queued
 *  messages in order until the connection ends; on an error it shuts the socket down, so
 *  the reader ends the connection.
 */
int SDLCALL TNET_LISTENER::writer_thread_function (void *connection) {
  CONNECTION *c = static_cast<CONNECTION *>(connection);

  SDL_LockMutex (c->lock);

  while (c->writing) {
    if (c->out.IsEmpty ()) {
      SDL_CondWait (c->has_data, c->lock);
      continue;
    }

    /* A backlog goes out in one write: one TCP segment per message would fill the socket's
       send buffer with per-segment overhead on a long round trip. The messages stay queued
       (and age) while they are written. */
    string data;
    int count = c->out.Peek (data, max_write_size);
    SDL_UnlockMutex (c->lock);
    bool ok = SendAll (c->fd, data);
    SDL_LockMutex (c->lock);

    double now = AppGetRawSeconds ();
    for (int i = 0; i < count; i++) {
      double forward;
      NetStatRecord (NS_WRITER, c->out.Pop (now, &forward));
      if (forward >= 0)
        NetStatRecord (NS_FORWARD, forward);
    }

    if (!ok && c->writing) {
      Warning (SOCKET_ERROR_MESSAGE (LogMsg ("Writer: error sending to %s, closing the connection",
                                             c->peer.c_str ())));
      c->writing = false;
      shutdown (c->fd, 2);
    }
  }

  SDL_UnlockMutex (c->lock);
  return 0;
}

/**
 *  Connects to @p address:@p port. Returns the socket for AddDialedConnection(), throws
 *  ConnectingErrorException.
 */
int TNET_LISTENER::DialSocket (in_addr address, in_port_t port) {
  sockaddr_in socket_address;

  socket_address.sin_family = AF_INET;
  socket_address.sin_addr = address;
  socket_address.sin_port = htons (port);
  memset (socket_address.sin_zero, '\0', 8);        /* clear the rest of the structure */

  int fd;
  if ((fd = socket (PF_INET, SOCK_STREAM, 0)) == -1) {
    Error (SOCKET_ERROR_MESSAGE ("Address: Calling socket failed"));
    throw ConnectingErrorException ();
  }

  if (connect (fd, (sockaddr *)&socket_address, sizeof (socket_address)) == -1) {
#ifdef WINDOWS
    bool refused = (WSAGetLastError () == WSAECONNREFUSED);
#else
    bool refused = (errno == ECONNREFUSED);
#endif
    Error (SOCKET_ERROR_MESSAGE ("Address: Calling connect failed"));
    do_close (fd);
    throw ConnectingErrorException (refused);
  }

  return fd;
}

/** Takes over the socket @p fd connected to @p address:@p port by DialSocket(). */
TNET_CONN_ID TNET_LISTENER::AddDialedConnection (in_addr address, in_port_t port, int fd) {
  struct sockaddr_in remote_addr;

  Debug ("adding by file descriptor");

  remote_addr.sin_family = AF_INET;
  remote_addr.sin_port = htons (port);
  remote_addr.sin_addr = address;
  memset (remote_addr.sin_zero, '\0', 8);       /* clear the rest of the structure */

  return AddConnection (fd, remote_addr, true);
}

/** Frees a connection whose threads have finished. */
void TNET_LISTENER::DeleteConnection (CONNECTION *c) {
  SDL_DestroyCond (c->has_data);
  SDL_DestroyMutex (c->lock);
  delete c;
}

/**
 *  The kernel's send buffer of a game socket is kept small, so that a peer that stops reading
 *  fills our outgoing queue, where its age is measured (TNET_OUT_QUEUE), within seconds instead
 *  of filling megabytes of an automatically grown buffer first. It must still hold the small
 *  segments in flight: at about 1700 messages/s (a big battle) and 100 ms round trip, 32 KB
 *  delayed the messages by tens of ms; 64 KB did not (tests/net/BASELINE.md, step 3).
 */
static void LimitSendBuffer (int fd) {
  int size = 64 * 1024;
  setsockopt (fd, SOL_SOCKET, SO_SNDBUF, (const char *)&size, sizeof (size));
}

/**
 *  Takes over the connected socket @p fd: starts its writer and reader. Returns its handle.
 */
TNET_CONN_ID TNET_LISTENER::AddConnection (int fd, sockaddr_in address, bool dialed) {
  static std::atomic<TNET_CONN_ID> last_id (no_connection);

  EnableKeepalive (fd);
  EnableNoDelay (fd);
  LimitSendBuffer (fd);

  /* Forget closed connections (port scans would grow the list), joining their readers. */
  vector<CONNECTION *> finished;

  int pending = 0;

  SDL_LockMutex (connections_lock);
  for (unsigned i = 0; i < connections.size (); ) {
    if (connections[i]->state == CONNECTION::closed) {
      finished.push_back (connections[i]);
      connections.erase (connections.begin () + i);
    } else {
      if (connections[i]->deadline > 0)
        pending++;
      i++;
    }
  }
  SDL_UnlockMutex (connections_lock);

  for (unsigned i = 0; i < finished.size (); i++) {
    SDL_WaitThread (finished[i]->reader, NULL);
    DeleteConnection (finished[i]);
  }

  if (!dialed && pending >= net_max_pending_connections) {
    NetDropWarning (LogMsg ("Listener: refusing a connection from %s: %d connections are waiting to join",
                            TNET_RESOLVER::NetworkToAscii (address.sin_addr).c_str (), pending));
    do_close (fd);
    return no_connection;
  }

  CONNECTION *c = NEW CONNECTION;
  c->listener = this;
  do
    c->id = ++last_id;
  while (c->id == no_connection);
  c->fd = fd;
  c->address = address.sin_addr;
  c->port = ntohs (address.sin_port);
  c->peer = LogMsg ("%s:%hu", TNET_RESOLVER::NetworkToAscii (address.sin_addr).c_str (), c->port);
  c->dialed = dialed;
  c->deadline = 0;
  c->closing = false;
  if (!dialed)
    SetDeadline (c, net_admission_timeout);
  c->state = CONNECTION::open;
  c->writing = true;
  c->lock = SDL_CreateMutex ();
  c->has_data = SDL_CreateCond ();

  SDL_LockMutex (connections_lock);
  c->writer = SDL_CreateThread (writer_thread_function, "net_write", c);
  c->reader = c->writer ? SDL_CreateThread (reader_thread_function, "net_read", c) : NULL;
  if (c->reader == NULL) {
    SDL_UnlockMutex (connections_lock);
    if (c->writer) {
      SDL_LockMutex (c->lock);
      c->writing = false;
      SDL_CondSignal (c->has_data);
      SDL_UnlockMutex (c->lock);
      SDL_WaitThread (c->writer, NULL);
    }
    do_close (fd);
    DeleteConnection (c);
    throw MutexException ();
  }
  connections.push_back (c);
  SDL_UnlockMutex (connections_lock);

  return c->id;
}

/** Returns the connection @p conn (also a closed one) or NULL. Called with connections_lock held. */
TNET_LISTENER::CONNECTION *TNET_LISTENER::FindConnection (TNET_CONN_ID conn) {
  for (unsigned i = 0; i < connections.size (); i++)
    if (connections[i]->id == conn)
      return connections[i];

  return NULL;
}

/**
 *  The reader of @p c stopped: stops the writer and closes the socket. The only place where
 *  a connection's socket is closed; nothing refers to it by its number, so the number can
 *  be reused at once.
 */
void TNET_LISTENER::ConnectionEnded (CONNECTION *c) {
  SDL_LockMutex (c->lock);
  c->writing = false;
  SDL_CondSignal (c->has_data);
  SDL_UnlockMutex (c->lock);

  shutdown (c->fd, 2);   // wakes up the writer if it is blocked in send()
  SDL_WaitThread (c->writer, NULL);
  c->writer = NULL;

  SDL_LockMutex (connections_lock);
  do_close (c->fd);
  c->state = CONNECTION::closed;
  SDL_UnlockMutex (connections_lock);
}

/**
 *  Queues a copy of @p msg for the connection @p conn. Never blocks: when its queue is full or
 *  its oldest message waits longer than net_out_queue_max_age, the connection is evicted (shut
 *  down, so it ends like a lost connection and the disconnect handler runs). The caller keeps
 *  @p msg. A @p forward (a received message passed on) is measured from when it was received
 *  (netstat `forward`).
 *
 *  @return true when the message was queued.
 */
bool TNET_LISTENER::Send (TNET_CONN_ID conn, TNET_MESSAGE *msg, bool forward) {
  bool queued = false;

  SDL_LockMutex (connections_lock);

  CONNECTION *c = FindConnection (conn);
  if (c != NULL && c->state == CONNECTION::open) {
    SDL_LockMutex (c->lock);

    if (c->writing) {
      double now = AppGetRawSeconds ();
      double received = forward ? now - (AppGetTimeSeconds () - msg->GetReceivedAt ()) : -1;
      TNET_OUT_QUEUE::PutResult result = c->out.Put (msg->GetHeaderStart (), msg->GetSize (), now, received);

      if (result == TNET_OUT_QUEUE::queued) {
        queued = true;
        SDL_CondSignal (c->has_data);
      } else {
        Warning (LogMsg ("Evicting %s: its outgoing queue is %s (%d bytes, the oldest message has waited %.0f ms)",
                         c->peer.c_str (), result == TNET_OUT_QUEUE::full ? "full" : "stalled",
                         c->out.GetBytes (), c->out.GetAge (now) * 1000));
        c->writing = false;
        SDL_CondSignal (c->has_data);
        shutdown (c->fd, 2);
      }
    }

    SDL_UnlockMutex (c->lock);
  }

  SDL_UnlockMutex (connections_lock);

  return queued;
}

/** Shuts the connection @p conn down: it ends like a lost connection (the disconnect handler runs). */
void TNET_LISTENER::Shutdown (TNET_CONN_ID conn) {
  SDL_LockMutex (connections_lock);

  CONNECTION *c = FindConnection (conn);
  if (c != NULL && c->state == CONNECTION::open) {
    SDL_LockMutex (c->lock);
    c->writing = false;
    SDL_CondSignal (c->has_data);
    SDL_UnlockMutex (c->lock);

    shutdown (c->fd, 2);
  }

  SDL_UnlockMutex (connections_lock);
}

/** Returns true until the connection @p conn has ended. */
bool TNET_LISTENER::IsConnectionOpen (TNET_CONN_ID conn) {
  SDL_LockMutex (connections_lock);
  CONNECTION *c = FindConnection (conn);
  bool open = c != NULL && c->state == CONNECTION::open;
  SDL_UnlockMutex (connections_lock);

  return open;
}

/**
 *  The connection @p c ends @p seconds from now (0 = never): its socket gets a receive timeout,
 *  and its reader checks the time after every receive. Called with connections_lock held, or
 *  before its reader starts.
 */
void TNET_LISTENER::SetDeadline (CONNECTION *c, int seconds) {
  c->deadline = seconds > 0 ? AppGetRawSeconds () + seconds : 0;

#ifdef WINDOWS
  DWORD timeout = seconds * 1000;
#else
  timeval timeout = { seconds, 0 };
#endif
  setsockopt (c->fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof (timeout));
}

/** The connection @p conn is a player's now: it no longer has to join in time (net_admission_timeout). */
void TNET_LISTENER::Admit (TNET_CONN_ID conn) {
  SDL_LockMutex (connections_lock);
  CONNECTION *c = FindConnection (conn);
  if (c != NULL && c->state == CONNECTION::open)
    SetDeadline (c, 0);
  SDL_UnlockMutex (connections_lock);
}

/**
 *  Lets the connection @p conn end in @p seconds unless it is admitted (a refused host: it gets
 *  time to show the reply before the connection closes). Only the first call counts: a host
 *  that keeps asking must not keep its connection open.
 */
void TNET_LISTENER::CloseAfter (TNET_CONN_ID conn, int seconds) {
  SDL_LockMutex (connections_lock);
  CONNECTION *c = FindConnection (conn);
  if (c != NULL && c->state == CONNECTION::open && !c->closing) {
    c->closing = true;
    SetDeadline (c, seconds);
  }
  SDL_UnlockMutex (connections_lock);
}

/** True once the destructor has taken the listening socket. */
bool TNET_LISTENER::IsStopping () {
  SDL_LockMutex (connections_lock);
  bool stopping = fd < 0;
  SDL_UnlockMutex (connections_lock);
  return stopping;
}

void TNET_LISTENER::ConsumerIsAttached (SDL_Thread *thread) {
  consumer_thread = thread;
}

/**
 *  Starts accepting connections. Call it after the handlers are registered (RegisterOnReceive()
 *  and the dispatcher's), so that nobody's message gets past them.
 */
void TNET_LISTENER::Start () {
  SDL_SemPost (start_sem);
}

/** Sets the receive handler (RECEIVE_HANDLER); NULL queues every message. */
void TNET_LISTENER::RegisterOnReceive (RECEIVE_HANDLER f, void *context) {
  on_receive_context = context;
  on_receive = f;
}


//=========================================================================
// TNET_OUT_QUEUE
//=========================================================================

TNET_OUT_QUEUE::TNET_OUT_QUEUE (int max_bytes, double max_age) {
  bytes = 0;
  this->max_bytes = max_bytes;
  this->max_age = max_age;
}

/**
 *  Queues a copy of @p size bytes at @p data at the time @p now. A message is never queued
 *  behind one older than the age limit (the connection is evicted instead).
 */
TNET_OUT_QUEUE::PutResult TNET_OUT_QUEUE::Put (const void *data, int size, double now,
                                               double forwarded_since) {
  if (GetAge (now) > max_age)
    return stalled;

  if (bytes + size > max_bytes)
    return full;

  packets.push_back (PACKET ());
  packets.back ().queued_at = now;
  packets.back ().forwarded_since = forwarded_since;
  packets.back ().data.assign (static_cast<const char *>(data), size);
  bytes += size;
  return queued;
}

/**
 *  Copies the oldest messages, at least one and at most @p max_bytes of them, into @p data
 *  (they stay queued until Pop()). Returns how many.
 */
int TNET_OUT_QUEUE::Peek (string &data, int max_bytes) const {
  int count = 0;

  data.clear ();
  for (std::deque<PACKET>::const_iterator i = packets.begin (); i != packets.end (); ++i, count++) {
    if (count > 0 && (int)(data.size () + i->data.size ()) > max_bytes)
      break;
    data += i->data;
  }

  return count;
}

double TNET_OUT_QUEUE::Pop (double now, double *forward_time) {
  double waited = GetAge (now);

  if (forward_time != NULL)
    *forward_time = packets.front ().forwarded_since >= 0 ? now - packets.front ().forwarded_since : -1;

  bytes -= packets.front ().data.size ();
  packets.pop_front ();
  return waited;
}


//=========================================================================
// TNET_DISPATCHER
//=========================================================================

TNET_DISPATCHER::TNET_DISPATCHER (TNET_MESSAGE_QUEUE *incoming_messages,
                                  TNET_MESSAGE_HANDLER *handler)
{
  this->incoming_messages = incoming_messages;
  this->handler = handler;

  thread = SDL_CreateThread (dispatcher_thread_function, "net_dispatch", this);

  if (thread == NULL) {
    Critical ("Could not create dispatcher thread");
  }
}

TNET_DISPATCHER::~TNET_DISPATCHER () {
  if (thread)
    SDL_WaitThread (thread, NULL);
}

int SDLCALL TNET_DISPATCHER::dispatcher_thread_function (void *dispatcher_class) {
  TNET_DISPATCHER *self = (TNET_DISPATCHER *)dispatcher_class;
  TNET_MESSAGE *msg;

  while ((msg = self->GetMessageQueue ()->GetMessage ()) != NULL)
    self->GetHandler ()->HandleMessage (msg);

  Info ("Dispatcher finished");
  return 0;
}


//=========================================================================
// TNET_MESSAGE_HANDLER
//=========================================================================

TNET_MESSAGE_HANDLER::TNET_MESSAGE_HANDLER () {
  for (int i = 0; i <= MAX_T_BYTE; i ++)
    extended_function[i] = NULL;
  on_disconnect = NULL;
}

void TNET_MESSAGE_HANDLER::HandleMessage (TNET_MESSAGE *msg) {
  if (msg->IsConnectionEnded ()) {
    DISCONNECT_HANDLER f = on_disconnect;
    if (f)
      f (msg->GetConnection ());
    pool_net_messages->PutToPool(msg);
    return;
  }

  if (extended_function[msg->GetType ()] == NULL) {
    NetDropWarning (LogMsg ("Dropping message of unknown type %d from %s", msg->GetType (),
                            TNET_RESOLVER::NetworkToAscii (msg->GetAddress ()).c_str ()));
    pool_net_messages->PutToPool(msg);
    return;
  }

  /* Call the registered callback function. */
  extended_function[msg->GetType ()] (msg);

  /* Handlers stop at the first out-of-bounds Extract*() and ignore the message. */
  if (msg->ExtractFailed ())
    NetDropWarning (LogMsg ("Dropped malformed message (type %d, size %d) from %s", msg->GetType (),
                            msg->GetSize (), TNET_RESOLVER::NetworkToAscii (msg->GetAddress ()).c_str ()));

  pool_net_messages->PutToPool(msg);
}

void TNET_MESSAGE_HANDLER::RegisterExtendedFunction (T_BYTE type,
                                              void (*f)(TNET_MESSAGE *msg))
{
  extended_function[type] = f;
}


//=========================================================================
// TNET_RESOLVER
//=========================================================================

TNET_RESOLVER::TNET_RESOLVER () {
  mutex = NEW TLOCK ();
}

/**
 *  Resolves the specified @p hostname.
 *
 *  @return Network address of the host. On any error the function throws
 *          ResolveException.
 *
 *  @note This function is thread safe, but might block for 30 seconds or so.
 */
in_addr TNET_RESOLVER::Resolve (string hostname) {
  mutex->Lock ();

  struct hostent *h;

  if ((h = gethostbyname (hostname.c_str ())) == NULL) {
#ifdef WINDOWS
    Error (LogMsg ("Talker: Error resolving address '%s': WSAGetLastError = %d", hostname.c_str (), WSAGetLastError ()));
#else
    Error (LogMsg ("Talker: Error resolving address '%s': %s", hostname.c_str (), hstrerror (h_errno)));
#endif
    throw ResolveException ();
  }

  in_addr ret = *(in_addr *)h->h_addr;

  mutex->Unlock ();

  return ret;
}

/**
 *  Returns hostname of local host.
 */
string TNET_RESOLVER::GetHostName () {
  const int max_hostname_length = 256;

  char hostname[max_hostname_length];

  if (gethostname (hostname, max_hostname_length) == -1)
    return "";
  else
    return hostname;
}

/**
 *  Converts network address to ASCII representation (IP address).
 */
string TNET_RESOLVER::NetworkToAscii (in_addr address) {
  /* Not inet_ntoa(): many network threads call this, and its buffer is static on some systems. */
  const uint32_t a = ntohl (address.s_addr);
  char text[16];
  snprintf (text, sizeof text, "%u.%u.%u.%u", a >> 24, (a >> 16) & 255, (a >> 8) & 255, a & 255);
  return text;
}

in_addr TNET_RESOLVER::AsciiToNetwork (string address) {
  struct in_addr ret;

#ifdef WINDOWS
  ret.S_un.S_addr = inet_addr(address.c_str ());
#else
  inet_aton (address.c_str (), &ret);
#endif

  return ret;
}


//=========================================================================
// END
//=========================================================================
// vim:ts=2:sw=2:et:

