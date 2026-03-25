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

#ifdef UNIX
# include <signal.h>
#endif

#include "dologs.h"
#include "donet.h"
#include "dosimpletypes.h"
#include "utils.h"

using std::string;
using std::vector;


//=========================================================================
// Global Variables
//=========================================================================

/**
 *  String used for error message produced by macro SOCKET_ERROR_MESSAGE().
 */
TLOG_MESSAGE socket_error_message;

TPOOL<TNET_MESSAGE> * pool_net_messages;


//=========================================================================
// Socket functions
//=========================================================================

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
// TNET_ADDRESS
//=========================================================================

TNET_ADDRESS::TNET_ADDRESS (in_addr address, in_port_t port) {
  this->address = address;
  this->port = port;

  empty = false;
  connected = false;

  sockaddr_in socket_address;

  socket_address.sin_family = AF_INET;
  socket_address.sin_addr = address;
  socket_address.sin_port = htons (port);
  memset (socket_address.sin_zero, '\0', 8);        /* clear the rest of the structure */

  if ((fd = socket (PF_INET, SOCK_STREAM, 0)) == -1) {
    Error (SOCKET_ERROR_MESSAGE ("Address: Calling socket failed"));
    throw ConnectingErrorException ();
  }

  if (connect (fd, (sockaddr *)&socket_address, sizeof (socket_address)) == -1) {
    Error (SOCKET_ERROR_MESSAGE ("Address: Calling connect failed"));
    throw ConnectingErrorException ();
  }

  connected = true;
}

/**
 *  Adds address, but does not open new connection; instead it assignes a
 *  filedescriptor @p fd which must be already connected.
 */
TNET_ADDRESS::TNET_ADDRESS (in_addr address, in_port_t port, int fd) {
  this->address = address;
  this->port = port;
  this->fd = fd;

  connected = true;
  empty = false;
}

/**
 *  Adds empty (not initialised) address.
 */
TNET_ADDRESS::TNET_ADDRESS () {
  address = TNET_RESOLVER::AsciiToNetwork ("0.0.0.0");
  port = 0;
  fd = -1;

  connected = false;
  empty = true;
}

TNET_ADDRESS::~TNET_ADDRESS () {
  Disconnect ();
}

void TNET_ADDRESS::Disconnect () {
  if (connected) {
    shutdown (fd, 2);
    do_close (fd);
  }

  connected = false;
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

  extract_p = 0;
}

/**
 *  Creates a message from a packed received from network.
 *
 *  @param address Address of the sender.
 *  @param port    Port of the sender.
 *  @param fd      File descriptor of received message.
 *  @param data    Data of the message including the header.
 */
void TNET_MESSAGE::Init_receive (in_addr address, in_port_t port, int fd, T_BYTE *data) {
  /*** FIXME: tu kopirovat iba velkost spravy ***/
  memcpy (&this->size, data, max_net_message_size);
  this->address = address;
  this->port = port;
  this->fd = fd;

  extract_p = 0;
}

void TNET_MESSAGE::Send (int fd) {
  int len;
  int pos = 0;

  const char *start = GetHeaderStart ();
  int size = GetSize ();

  do {
    if ((len = send (fd, start + pos, size - pos, 0)) == -1) {
      Debug (SOCKET_ERROR_MESSAGE ("Error sending message"));
      shutdown (fd, 2);
      do_close (fd);
      throw TNET_MESSAGE::SendError ();
    }

    pos += len;
  } while (pos != size);
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
 *  queue using GetMessage().
 */
void TNET_MESSAGE_QUEUE::PutMessage (TNET_MESSAGE *message) {
  SDL_LockMutex (mutex);

#if DEBUG
  if (count == size)
    Debug ("MessageQueue limit achieved");
#endif

  while (count == size)
    SDL_CondWait (is_not_full, mutex);

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

  /* Wake up consumer. GetMessage will return NULL when dead. */
  SDL_CondSignal (is_not_empty);
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

  on_disconnect = NULL;
  bind_sem = SDL_CreateSemaphore (0);
  listener_bind_ok = false;

  if (bind_sem == NULL)
    Critical ("Listener: SDL_CreateSemaphore failed");

  thread = SDL_CreateThread (listener_thread_function, "net_listen", this);

  if (thread == NULL) {
    SDL_DestroySemaphore (bind_sem);
    bind_sem = NULL;
    Critical ("Could not create listener thread");
  }

  SDL_SemWait (bind_sem);

  if (!listener_bind_ok) {
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

  if (fd >= 0) {
    shutdown (fd, 2);
    do_close (fd);
    fd = -1;
  }

  /* Wait until listener thread is dead. */
  if (thread)
    SDL_WaitThread (thread, NULL);

  /* Wait until consumer of incoming_messages is dead. */
  if (consumer_thread)
    SDL_WaitThread (consumer_thread, NULL);

  if (bind_sem) {
    SDL_DestroySemaphore (bind_sem);
    bind_sem = NULL;
  }

  delete incoming_messages;
}

/**
 *  Listener's thread function.
 *
 *  @param listener_class Pointer to class instance to which the thread
 *                        belongs.
 */
int SDLCALL TNET_LISTENER::listener_thread_function (void *listener_class) {
  struct sockaddr_in address;
  socklen_t sockaddr_size = sizeof (address);

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

  Info (LogMsg ("Listener running on port %hu", self->port));

  listen (fd, 10);

  while (1) {
    struct sockaddr_in remote_addr;
    int new_fd;

    if ((new_fd = accept (fd, (sockaddr *)&remote_addr, &sockaddr_size)) == -1) {
      Debug (SOCKET_ERROR_MESSAGE ("Listener: Error accepting connection"));
      break;
    }

    self->subthread_fd.push_back (new_fd);
    self->subthread_address.push_back (remote_addr.sin_addr);

    TNET_LISTENER::ACCEPT_DATA *data = NEW TNET_LISTENER::ACCEPT_DATA (new_fd,
        remote_addr, self);

    SDL_Thread *t;
    t = SDL_CreateThread (listener_accept, "net_accept", data);

    if (t == NULL)
      throw MutexException ();

    self->subthread_thread.push_back (t);
  }

  do_close (fd);

  /* Shutdown all running listener threads. */
  for (unsigned i = 0; i < self->subthread_thread.size (); i++) {
    shutdown (self->subthread_fd[i], 2);
    SDL_WaitThread (self->subthread_thread[i], NULL);
  }

  end_sockets ();

  Info ("Listener finished");
  return 0;
}

int SDLCALL TNET_LISTENER::listener_accept (void *d) {
  TNET_LISTENER::ACCEPT_DATA *data = static_cast<TNET_LISTENER::ACCEPT_DATA *>(d);

  Debug (LogMsg ("Got connection from: %s", inet_ntoa (data->address.sin_addr)));

  int fd = data->fd;
  TNET_LISTENER *self = data->self;
  T_BYTE buf[max_net_message_size];
  T_BYTE *size = buf;
  int pos;
  int len;

  while (1) {
    if ((pos = recv (fd, reinterpret_cast<char*>(buf), 1, 0)) == -1) {
      Error (SOCKET_ERROR_MESSAGE ("Listener: recv failed"));
      return 0;
    }

    if (pos == 0) {
      Debug ("Listener: Remote host closed connection");
      do_close (fd);
      break;
    }

    do {
      if ((len = recv (fd, reinterpret_cast<char*>(buf + pos), *size - pos, 0)) <= 0) {
        Error (SOCKET_ERROR_MESSAGE ("Listener: recv failed"));
        break;
      }

      pos += len;
    } while (pos != *size);

    TNET_MESSAGE *msg = pool_net_messages->GetFromPool();
    msg->Init_receive(data->address.sin_addr, data->address.sin_port, fd, buf);
    self->incoming_messages->PutMessage (msg);
  }

  if (self->on_disconnect)
    self->on_disconnect (data->address.sin_addr, data->address.sin_port);

  Debug ("Listener's subthread finished");
  return 0;
}

void TNET_LISTENER::AddListenerByFileDescriptor (in_addr address, in_port_t port, int fd) {
  struct sockaddr_in remote_addr;

  Debug ("pridavam podla filedscriptoru");

  remote_addr.sin_family = AF_INET;
  remote_addr.sin_port = htons (port);
  remote_addr.sin_addr = address;
  memset (remote_addr.sin_zero, '\0', 8);       /* clear the rest of the structure */

  TNET_LISTENER::ACCEPT_DATA *data = NEW TNET_LISTENER::ACCEPT_DATA (fd, remote_addr, this);

  SDL_Thread *t;
  t = SDL_CreateThread (listener_accept, "net_accept", data);

  if (t == NULL)
    throw MutexException ();

  subthread_fd.push_back (fd);
  subthread_address.push_back (remote_addr.sin_addr);
  subthread_thread.push_back (t);
}

void TNET_LISTENER::ConsumerIsAttached (SDL_Thread *thread) {
  consumer_thread = thread;
}

int TNET_LISTENER::GetListenersFileDescriptor (in_addr address) {
  for (unsigned i = 0; i < subthread_address.size (); i++) {
    if (TNET_RESOLVER::NetworkToAscii (subthread_address[i]) == TNET_RESOLVER::NetworkToAscii (address))
      return subthread_fd[i];
  }

  return -1;
}

void TNET_LISTENER::RegisterOnDisconnect (void (* f)(in_addr, in_port_t)) {
  on_disconnect = f;
}

//=========================================================================
// TNET_TALKER
//=========================================================================

TNET_TALKER::TNET_TALKER (int queue_size) {
  Initialise (queue_size);
}

/**
 *  Constructor, which creates new queue for outgoing messages and new thread
 *  for talker.
 *
 *  @param queue_size     Size of the outgoing message queue.
 *  @param remote_address Remote address.
 *  @param remote_port    Remote port.
 */
TNET_TALKER::TNET_TALKER (int queue_size, in_addr remote_address,
                          in_port_t remote_port)
{
  Initialise (queue_size);

  AddAddress (remote_address, remote_port);
}

void TNET_TALKER::Initialise (int queue_size) {
  outgoing_messages = NEW TNET_MESSAGE_QUEUE (queue_size);

  thread = SDL_CreateThread (talker_thread_function, "net_talker", this);

  if (thread == NULL)
    throw MutexException ();
}

/**
 *  Destructor.
 */
TNET_TALKER::~TNET_TALKER () {
  outgoing_messages->Die ();
  RemoveAllAddresses ();

  /* Wait until consumer of outgoing_messages is dead. */
  if (thread)
    SDL_WaitThread (thread, NULL);

  delete outgoing_messages;
}

/**
 *  Talker's thread function.
 *
 *  @param talker_class Pointer to class instance to which the thread belongs.
 */
int SDLCALL TNET_TALKER::talker_thread_function (void *talker_class) {
  int fd;   /* file descriptor */

  TNET_TALKER *self = (TNET_TALKER *)talker_class;

  (void)init_sockets (); /* Casting to void does not produce warning on UNIX. */

  if ((fd = socket (PF_INET, SOCK_DGRAM, 0)) == -1) {
    Error (SOCKET_ERROR_MESSAGE ("Talker: Calling socket failed"));
    return 0;
  }

  Info ("Talker running");

  TNET_MESSAGE *msg;

  while ((msg = self->outgoing_messages->GetMessage ()) != NULL) {
    /*
     * Find out, if the message is destined for all remote addresses, or for
     * one only (e.g. only for one player).
     */
    if (msg->GetDest () == 255) {
      // XXX: namiesto 255 to chce konstantu

      for (unsigned i = 0; i < self->distinct_remote_addresses.size (); i++) {
        try {
          int fd = self->distinct_remote_addresses[i]->GetFileDescriptor ();
          msg->Send (fd);
        } catch (TNET_MESSAGE::SendError &) {
          self->distinct_remote_addresses[i]->Disconnect ();
        }
      }
    } else {
      try {
        int fd = self->remote_address[msg->GetDest ()]->GetFileDescriptor ();
        msg->Send (fd);
      } catch (TNET_MESSAGE::SendError &) {
        self->remote_address[msg->GetDest ()]->Disconnect ();
      }
    }

    pool_net_messages->PutToPool(msg);
  }

  do_close (fd);
  end_sockets ();

  Info ("Talker finished");
  return 0;
}

void TNET_TALKER::RemoveAllAddresses () {
  for (unsigned i = 0; i < remote_address.size (); i++)
    delete remote_address[i];

  remote_address.clear ();
  distinct_remote_addresses.clear ();
}

T_BYTE TNET_TALKER::AddAddress (in_addr address, in_port_t port, int fd) {
  TNET_ADDRESS *found_address = NULL;

  for (unsigned i = 0; i < distinct_remote_addresses.size (); i++) {
    if ((TNET_RESOLVER::NetworkToAscii (distinct_remote_addresses[i]->GetAddress ())
          == TNET_RESOLVER::NetworkToAscii (address))
        && distinct_remote_addresses[i]->GetPort () == port)
    {
      found_address = distinct_remote_addresses[i];
      break;
    }
  }

  if (found_address == NULL) {
    TNET_ADDRESS *net_addr;

    if (fd == -1)
      net_addr = NEW TNET_ADDRESS (address, port);
    else
      net_addr = NEW TNET_ADDRESS (address, port, fd);

    remote_address.push_back (net_addr);
    distinct_remote_addresses.push_back (net_addr);
  } else {
    TNET_ADDRESS *net_addr = NEW TNET_ADDRESS (address, port, found_address->GetFileDescriptor ());
    remote_address.push_back (net_addr);
  }

  return remote_address.size () - 1;
}

T_BYTE TNET_TALKER::AddEmptyAddress () {
  remote_address.push_back (NEW TNET_ADDRESS ());

  return remote_address.size () - 1;
}

void TNET_TALKER::DisconnectAddress (int id) {
  in_addr address = remote_address[id]->GetAddress ();
  in_port_t port  = remote_address[id]->GetPort ();

  TNET_ADDRESS *old = remote_address[id];
  remote_address[id] = new TNET_ADDRESS (); // assign empty address

  RemoveAddressFromDisctinctAddresses (address, port);

  delete old;
}

void TNET_TALKER::DisconnectAddress (in_addr address, in_port_t port) {
  for (unsigned i = 0; i < remote_address.size (); i++) {
    if ((TNET_RESOLVER::NetworkToAscii (remote_address[i]->GetAddress ())
          == TNET_RESOLVER::NetworkToAscii (address))
        && remote_address[i]->GetPort () == port)
    {
      DisconnectAddress (i);
    }
  }
}

void TNET_TALKER::RemoveAddress (int id) {
  if (!remote_address[id]->IsEmpty ()) {
    in_addr address = remote_address[id]->GetAddress ();
    in_port_t port  = remote_address[id]->GetPort ();

    RemoveAddressFromDisctinctAddresses (address, port);
  }

  TNET_ADDRESS *old = remote_address[id];
  remote_address.erase (remote_address.begin() + id);

  delete old;
}

void TNET_TALKER::RemoveAddressFromDisctinctAddresses (in_addr address,
                                                       in_port_t port)
{
  for (unsigned i = 0; i < distinct_remote_addresses.size (); i++) {
    if ((TNET_RESOLVER::NetworkToAscii (distinct_remote_addresses[i]->GetAddress ())
          == TNET_RESOLVER::NetworkToAscii (address))
        && distinct_remote_addresses[i]->GetPort () == port)
    {
      distinct_remote_addresses.erase (distinct_remote_addresses.begin() + i);
      break;
    }
  }
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
}

void TNET_MESSAGE_HANDLER::HandleMessage (TNET_MESSAGE *msg) {
  if (extended_function[msg->GetType ()] == NULL) {
    Critical (LogMsg ("No handler specified for message type %d", msg->GetType ()));
    return;
  }

  /* Call the registered callback function. */
  extended_function[msg->GetType ()] (msg);

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
  mutex = new TLOCK ();
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
  return inet_ntoa (address);
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

