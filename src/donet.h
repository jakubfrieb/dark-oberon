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
 *  @file donet.h
 *
 *  Header file for netwocrking.
 *
 *  @author Marian Cerny
 *
 *  @date 2003, 2004
 */

#ifndef __donet_h__
#define __donet_h__


//=========================================================================
// Included files
//=========================================================================

#include "cfg.h"
#include "doalloc.h"

#ifdef WINDOWS
 #undef APIENTRY
 #undef WINGDIAPI
 #include <winsock.h>
#else
 #include <arpa/inet.h>
 #include <netdb.h>
 #include <netinet/in.h>
 #include <sys/socket.h>
 #include <sys/types.h>
 #include <unistd.h>
#endif

#include <stdint.h>
#include <string>
#include <vector>

#include "dologs.h"
#include "dosimpletypes.h"
#include "dopool.h"
#include "doipc.h"


//=========================================================================
// Typedefs
//=========================================================================

/*
 * On Windows we need some typedefs for POSIX networking types used in project.
 */
#ifdef WINDOWS
typedef int socklen_t;
typedef unsigned short in_port_t;
#endif


//=========================================================================
// Constants
//=========================================================================

/** Maximum size of networking message including headers. */
const int max_net_message_size = 255;


//=========================================================================
// Wire encoding
//=========================================================================

/*
 * Multi-byte message fields are fixed-width and in network byte order (big
 * endian). They are written with shifts, so the encoding is the same on every
 * platform and word size (no htonll() on Windows or Haiku).
 */

inline void NetPutU16 (T_BYTE *p, uint16_t v)
{ p[0] = (T_BYTE)(v >> 8); p[1] = (T_BYTE)v; }

inline void NetPutU32 (T_BYTE *p, uint32_t v)
{ NetPutU16 (p, (uint16_t)(v >> 16)); NetPutU16 (p + 2, (uint16_t)v); }

inline void NetPutU64 (T_BYTE *p, uint64_t v)
{ NetPutU32 (p, (uint32_t)(v >> 32)); NetPutU32 (p + 4, (uint32_t)v); }

/** A double travels as the u64 of its IEEE 754 bits. */
inline void NetPutDouble (T_BYTE *p, double v)
{ uint64_t u; memcpy (&u, &v, sizeof u); NetPutU64 (p, u); }

inline uint16_t NetGetU16 (const T_BYTE *p)
{ return (uint16_t)((p[0] << 8) | p[1]); }

inline uint32_t NetGetU32 (const T_BYTE *p)
{ return ((uint32_t)NetGetU16 (p) << 16) | NetGetU16 (p + 2); }

inline uint64_t NetGetU64 (const T_BYTE *p)
{ return ((uint64_t)NetGetU32 (p) << 32) | NetGetU32 (p + 4); }

inline double NetGetDouble (const T_BYTE *p)
{ uint64_t u = NetGetU64 (p); double v; memcpy (&v, &u, sizeof v); return v; }


//=========================================================================
// Define macros
//=========================================================================

/**
 *  @def SOCKET_ERROR_MESSAGE(msg)
 *  Macro returning string describing last error with sockets.
 *
 *  @param msg String which will be prepended to the log message.
 *
 *  @note This has got different implementation on #UNIX and #WINDOWS. UNIX
 *        uses standard mechanism with errno. Windows uses function
 *        WSAGetLastError() to get last socket error code.
 */
#ifdef WINDOWS
# define SOCKET_ERROR_MESSAGE(msg) \
  (_snprintf (socket_error_message, MAX_LOG_MESSAGE_SIZE, "%s: WSAGetLastError = %d", (msg), WSAGetLastError()), \
   socket_error_message)
#else
# include <errno.h>
# define SOCKET_ERROR_MESSAGE(msg) \
  (snprintf (socket_error_message, MAX_LOG_MESSAGE_SIZE, "%s: %s", (msg), strerror (errno)), \
   socket_error_message)
#endif


//=========================================================================
// Variables
//=========================================================================

extern TLOG_MESSAGE socket_error_message;

/** Logs a warning about a dropped (malformed or unexpected) network message,
 *  rate limited so that a flood of bad messages cannot fill the log. */
void NetDropWarning (const char *text);

//=========================================================================
// TNET_ADDRESS
//=========================================================================

struct TNET_ADDRESS {
public:
  class ConnectingErrorException {
  public:
    ConnectingErrorException (bool is_refused = false) : refused (is_refused) {}

    bool refused;  //!< Nobody listens on the address (ECONNREFUSED).
  };

  TNET_ADDRESS (in_addr address, in_port_t port);
  TNET_ADDRESS (in_addr address, in_port_t port, int fd);
  TNET_ADDRESS ();
  ~TNET_ADDRESS ();

  in_addr GetAddress () { return address; }
  in_port_t GetPort ()  { return port; }

  int GetFileDescriptor ()
  { return fd; }

  void Disconnect ();

  bool IsEmpty ()
  { return empty; }

private:
  in_addr     address;
  in_port_t   port;
  int         fd;

  bool        connected;
  bool        empty;
};


//=========================================================================
// TNET_MESSAGE
//=========================================================================

/**
 *  Network message containing one event. One network packet can contain more
 *  messages.
 */
struct TNET_MESSAGE : public TPOOL_ELEMENT {
public:
  class SendError {};

  // XXX: 255
  TNET_MESSAGE();
  
  void Init_send (T_BYTE type, T_BYTE subtype, T_BYTE dest = 255);
  void Init_receive (in_addr address, in_port_t port, int fd, T_BYTE *data);

  T_BYTE GetSize ()     { return size; }
  T_BYTE GetType ()     { return type; }
  T_BYTE GetSubtype ()  { return subtype; }
  T_BYTE GetDest ()     { return dest; }

  void SetDest (T_BYTE dest)  { this->dest = dest; }

  /** Sends the message only over the connection @p fd (a reply to a message
   *  received on it), whatever the destination is. */
  void SetReplyConnection (int fd)  { this->fd = fd; }

  /** Puts @p size of bytes from memory at address @p data into the message. */
  void Pack (const void *data, int size)
  {
    if (this->size + size > max_net_message_size) {
      Critical ("Size of the message too big");
      throw 0;
    }
    memcpy (&buf[this->size - GetHeaderSize ()], data, size);
    this->size += size;
  }

  /** Puts a byte into the message. */
  void PackByte (T_BYTE byte)
  { 
    if (this->size + 1 > max_net_message_size) {
      Critical ("Size of the message too big");
      throw 0;
    }
    buf[this->size++ - GetHeaderSize ()] = byte;
  }

  /** Puts a string @p str into the message. */
  void PackString (std::string str)
  { Pack (str.c_str (), str.length () + 1); }

  /** Put fixed-width fields in network byte order (see NetPutU16()). */
  void PackU16 (uint16_t v)  { T_BYTE b[2]; NetPutU16 (b, v); Pack (b, sizeof b); }
  void PackU32 (uint32_t v)  { T_BYTE b[4]; NetPutU32 (b, v); Pack (b, sizeof b); }
  void PackDouble (double v) { T_BYTE b[8]; NetPutDouble (b, v); Pack (b, sizeof b); }

  /** Gets @p size of bytes from message into memory at address @p data.
   *  Reading past the end of the received payload zero-fills @p data and marks
   *  the message as malformed (see ExtractFailed()). */
  bool Extract (void *data, int size)
  {
    if (size < 0 || extract_p + size > GetPayloadSize ()) {
      if (size > 0) memset (data, 0, size);
      extract_error = true;
      return false;
    }
    memcpy (data, &buf[extract_p], size);
    extract_p += size;
    return true;
  }

  /** Gets a byte from the message (0 and malformed if there is none left). */
  T_BYTE ExtractByte ()
  {
    T_BYTE byte;
    Extract (&byte, 1);
    return byte;
  }

  /** Gets a NUL-terminated string from a message ("" and malformed if the
   *  terminator is missing). */
  std::string ExtractString ()
  {
    int left = GetPayloadSize () - extract_p;
    const char *char_p = reinterpret_cast<const char *>(&buf[extract_p]);
    const void *end = left > 0 ? memchr (char_p, '\0', left) : NULL;
    if (end == NULL) {
      extract_error = true;
      return std::string ();
    }
    extract_p += static_cast<const char *>(end) - char_p + 1;
    return char_p;
  }

  /** Get fixed-width fields in network byte order (0 and malformed if the
   *  message is too short). */
  uint16_t ExtractU16 ()  { T_BYTE b[2]; Extract (b, sizeof b); return NetGetU16 (b); }
  uint32_t ExtractU32 ()  { T_BYTE b[4]; Extract (b, sizeof b); return NetGetU32 (b); }
  double ExtractDouble () { T_BYTE b[8]; Extract (b, sizeof b); return NetGetDouble (b); }

  /** Count of payload bytes not extracted yet. */
  int Remaining () { return GetPayloadSize () - extract_p; }

  /** Marks the message as malformed when it has bytes after the parsed fields
   *  (every message has an exact length). */
  void ExtractEnd ()
  { if (Remaining () != 0) extract_error = true; }

  /** True when an Extract*() call ran past the end of the message. */
  bool ExtractFailed () { return extract_error; }

  /** Size of the received/packed payload (without headers). */
  int GetPayloadSize () { return size - GetHeaderSize (); }

  /** Finds out the header size of the message. */
  static int GetHeaderSize ()
  { return 4 * sizeof (T_BYTE); /* size, type, subtype, dest */ }

  /** Returns pointer to start of the header. */
  const char *GetHeaderStart () { return (const char *)&size; }

  in_addr GetAddress () { return address; }
  in_port_t GetPort ()  { return port; }
  int GetFileDescriptor () { return fd; }
  //! When the listener received the message (AppGetTimeSeconds), for `netstat`.
  double GetReceivedAt () { return received_at; }

  void Send (int fd);

private:
  /*
   * The order of the next variables MUST be as follows -
   *   size, type, subtype, dest, buf
   *
   * 'size' MUST be first variable!
   */
  T_BYTE size;     //!< Size of the message without headers.
  T_BYTE type;     //!< Type of the message.
  T_BYTE subtype;  //!< Subtype. Might not be used.

  T_BYTE dest;     //!< Player id or all_players.

  T_BYTE buf[max_net_message_size];   //!< The message itself.

  in_addr address;
  in_port_t port;
  int fd;          //!< Received on this connection / reply to it (-1 = use dest).
  double received_at;

  int extract_p;
  bool extract_error;  //!< An Extract*() call ran past the end of the message.
};


//=========================================================================
// TNET_MESSAGE_QUEUE
//=========================================================================

/**
 *  Message queue containing network messages. This queue is used in
 *  TNET_LISTENER, TNET_TALKER, TNET_DISPATCHER.
 *
 *  @note This class is thread safe.
 */
class TNET_MESSAGE_QUEUE {
public:
  /** Exception that is thrown when there is an error creating variables needed
   *  for multi-threading support. */
  class MutexException {};

  TNET_MESSAGE_QUEUE (int size);
  ~TNET_MESSAGE_QUEUE ();

  TNET_MESSAGE *GetMessage ();
  void PutMessage (TNET_MESSAGE *message);

  void Die ();

private:
  SDL_mutex *mutex;        //!< Mutex used for locking the queue.
  SDL_cond *is_not_empty;  //!< Conditional variable for signaling that the
                          //!  queue is not empty.
  SDL_cond *is_not_full;   //!< Conditional variable for signaling that the
                          //!  queue is not full.

  TNET_MESSAGE **message; //!< Dynamicaly created array of networking messages.
  int size;     //!< Size of the queue.
  int count;    //!< Actual count of messages in the queue.
  int head;     //!< Actual position of the head in the queue.

  bool dead;
};


//=========================================================================
// TNET_LISTENER
//=========================================================================

/**
 *  Class for listener, which is a thread receiving packets from network,
 *  unpacking them into messages and putting them into incoming message queue.
 */
class TNET_LISTENER {
public:
  class MutexException {};

  struct ACCEPT_DATA {
    ACCEPT_DATA (int fd, sockaddr_in address, TNET_LISTENER *self) {
      this->fd = fd;
      this->address = address;
      this->self = self;
    }

    int fd;
    sockaddr_in address;
    TNET_LISTENER *self;
  };

  TNET_LISTENER (int queue_size, in_port_t port);
  ~TNET_LISTENER ();

  void AddListenerByFileDescriptor (in_addr address, in_port_t port, int fd);

  /** Returns pointer to incoming message queue. */
  TNET_MESSAGE_QUEUE *GetMessageQueue ()
  { return incoming_messages; }

  /** Returns the port on which the listener listen. */
  in_port_t GetPort ()
  { return port; }

  void ConsumerIsAttached (SDL_Thread *thread);
  int GetListenersFileDescriptor (in_addr address, in_port_t port);
  void ReleaseConnection (int fd);
  bool IsConnectionOpen (int fd);
  void CloseWhenIdle (int fd, int seconds);

  /** Distinguishes listeners of different sessions in late disconnect callbacks. */
  unsigned GetGeneration ()
  { return generation; }

  typedef void (* DISCONNECT_HANDLER)(unsigned generation, int fd);
  void RegisterOnDisconnect (DISCONNECT_HANDLER f);

private:
  static int SDLCALL listener_thread_function (void *listener_class);
  static int SDLCALL listener_accept (void *d);

  SDL_Thread *thread;    //!< Listener thread handle.
  SDL_Thread *consumer_thread;

  /** A connection read by a subthread; the listener owns (closes) its socket. */
  struct CONNECTION {
    enum State {
      open,      //!< Being read.
      ended,     //!< Peer closed it; still open until ReleaseConnection (the fd stays unique).
      closed     //!< Socket closed; the fd number may belong to a newer connection.
    };

    int fd;
    in_addr address;
    in_port_t port;       //!< Remote port (host byte order).
    bool own;             //!< We dialed it (AddListenerByFileDescriptor), e.g. the leader link.
    bool claimed;         //!< Already assigned to a remote host.
    State state;
    SDL_Thread *thread;
  };

  void AddConnection (int fd, sockaddr_in address, bool own);
  void ConnectionEnded (int fd);
  static void CloseConnection (CONNECTION &c);

  std::vector<CONNECTION> connections;
  SDL_mutex *connections_lock;   //!< Guards #connections (the listener thread appends).

  int fd;   //!< File descriptior.
  in_port_t port;  //!< Port on which the listener is listening (0 = ephemeral).
  TNET_MESSAGE_QUEUE *incoming_messages;  //!< Message queue for incoming
                                          //!  network messages.

  DISCONNECT_HANDLER on_disconnect;
  unsigned generation;

  SDL_sem *bind_sem;       //!< Posted after bind succeeds or fails (port 0 = OS picks port).
  bool listener_bind_ok;   //!< Set in listener thread before posting bind_sem.
};


//=========================================================================
// TNET_TALKER
//=========================================================================

/**
 *  Class for talker, which is a thread waiting for new messages in outgoing
 *  message queue, packing them into packets and sending them through network
 *  to remote side.
 */
class TNET_TALKER {
public:
  class MutexException {};

  TNET_TALKER (int queue_size);
  TNET_TALKER (int queue_size, in_addr remote_address, in_port_t remote_port);
  ~TNET_TALKER ();

  void RemoveAllAddresses ();

  T_BYTE AddAddress (in_addr address, in_port_t port, int fd = -1);
  T_BYTE AddEmptyAddress ();

  void DisconnectAddress (int id);
  void RemoveAddress (int id);

  int FindAddressByFileDescriptor (int fd, int from = 0);

  /** Returns the number of addresses (one per player id). */
  int GetAddressCount ()
  { return (int)remote_address.size (); }

  /** Returns pointer to outgoing message queue. */
  TNET_MESSAGE_QUEUE *GetMessageQueue ()
  { return outgoing_messages; }

  in_addr GetRemoteAddress (int index)
  { return remote_address[index]->GetAddress (); }

  in_port_t GetRemotePort (int index)
  { return remote_address[index]->GetPort (); }

  int GetRemoteFiledescriptor (int index)
  { return remote_address[index]->GetFileDescriptor (); }

private:
  void Initialise (int queue_size);

  static int SDLCALL talker_thread_function (void *talker_class);

  SDL_Thread *thread;    //!< Talker thread handle.

  TNET_MESSAGE_QUEUE *outgoing_messages;  //!< Message queue for outgoing
                                          //!  network messages.

  std::vector<TNET_ADDRESS *> remote_address;
  std::vector<TNET_ADDRESS *> distinct_remote_addresses;

  void RemoveAddressFromDisctinctAddresses (in_addr address, in_port_t port);
};


//=========================================================================
// TNET_MESSAGE_HANDLER
//=========================================================================

/**
 *  Message handler calls for every message the right function, which is
 *  associated with that type of the message.
 */
class TNET_MESSAGE_HANDLER {
public:
  TNET_MESSAGE_HANDLER ();

  void HandleMessage (TNET_MESSAGE *msg);

  void RegisterExtendedFunction (T_BYTE type, void (*f)(TNET_MESSAGE *msg));

private:
  /** Array of pointers to callback functions. */
  void (*extended_function[MAX_T_BYTE + 1])(TNET_MESSAGE *);
}; 


//=========================================================================
// TNET_DISPATCHER
//=========================================================================

/**
 *  Class for dispatcher, which is a separate thread grabbing messages from
 *  incoming message queue and dispatching them to mailboxes (handlers)
 *  according to type of networking messages.
 */
class TNET_DISPATCHER {
public:
  TNET_DISPATCHER (TNET_MESSAGE_QUEUE *incoming_messages,
                   TNET_MESSAGE_HANDLER *handler);
  ~TNET_DISPATCHER ();

  /** Returns pointer to incoming message queue. */
  TNET_MESSAGE_QUEUE *GetMessageQueue ()
  { return incoming_messages; }

  /** Returns pointer to message handler. */
  TNET_MESSAGE_HANDLER *GetHandler ()
  { return handler; }

  /** Returns dispatcher's thread id. */
  SDL_Thread *GetThread () {
    return thread;
  }

  /** Called when someone else (the listener it consumes from) has already
   *  joined the thread with SDL_WaitThread(), which frees the handle. */
  void ThreadJoined () {
    thread = NULL;
  }

private:
  static int SDLCALL dispatcher_thread_function (void *dispatcher_class);

  SDL_Thread *thread;    //!< Dispatcher thread handle.

  TNET_MESSAGE_QUEUE *incoming_messages;  //!< Message queue for incoming
                                          //!  network messages.
  TNET_MESSAGE_HANDLER *handler;  //!< Message handler.
};


//=========================================================================
// TNET_RESOLVER
//=========================================================================

/**
 *  Class containing methods to resolve hostnames, find out local hostname,
 *  converting network address to ASCII representation, etc.
 *
 *  @note This class is NOT thread safe.
 */
class TNET_RESOLVER {
public:
  /** Exception throwed when there was an error with resolving hostname. */
  class ResolveException {};

  TNET_RESOLVER ();

  static std::string GetHostName ();

  in_addr Resolve (std::string hostname);

  static std::string NetworkToAscii (in_addr address);
  static in_addr AsciiToNetwork (std::string address);

private:
  TLOCK *mutex;
};


//=========================================================================
// Functions
//=========================================================================

bool init_sockets (void);
void end_sockets (void);

extern TPOOL<TNET_MESSAGE> * pool_net_messages;
#endif

//=========================================================================
// END
//=========================================================================
// vim:ts=2:sw=2:et:

