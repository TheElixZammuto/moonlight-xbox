/** 
 @file  unix.c
 @brief ENet Unix system specific functions
*/
#ifndef _WIN32

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define ENET_BUILDING_LIB 1
#include "enet/enet.h"

#if defined(__APPLE__)
#ifndef HAS_POLL
#define HAS_POLL 1
#endif
#ifndef HAS_FCNTL
#define HAS_FCNTL 1
#endif
#ifndef HAS_INET_PTON
#define HAS_INET_PTON 1
#endif
#ifndef HAS_INET_NTOP
#define HAS_INET_NTOP 1
#endif
#ifndef HAS_MSGHDR_FLAGS
#define HAS_MSGHDR_FLAGS 1
#endif
#ifndef HAS_SOCKLEN_T
#define HAS_SOCKLEN_T 1
#endif
#ifndef HAS_GETADDRINFO
#define HAS_GETADDRINFO 1
#endif
#ifndef HAS_GETNAMEINFO
#define HAS_GETNAMEINFO 1
#endif
#elif defined(__vita__)
#ifdef HAS_POLL
#undef HAS_POLL
#endif
#ifdef HAS_FCNTL
#undef HAS_FCNTL
#endif
#ifdef HAS_IOCTL
#undef HAS_IOCTL
#endif
#ifndef HAS_INET_PTON
#define HAS_INET_PTON 1
#endif
#ifndef HAS_INET_NTOP
#define HAS_INET_NTOP 1
#endif
#ifdef HAS_MSGHDR_FLAGS
#undef HAS_MSGHDR_FLAGS
#endif
#ifndef HAS_SOCKLEN_T
#define HAS_SOCKLEN_T 1
#endif
#ifndef HAS_GETADDRINFO
#define HAS_GETADDRINFO 1
#endif
#ifndef HAS_GETNAMEINFO
#define HAS_GETNAMEINFO 1
#endif
#elif defined(__WIIU__)
#ifndef HAS_POLL
#define HAS_POLL 1
#endif
#ifndef HAS_FCNTL
#define HAS_FCNTL 1
#endif
#ifndef HAS_IOCTL
#define HAS_IOCTL 1
#endif
#ifndef HAS_INET_PTON
#define HAS_INET_PTON 1
#endif
#ifndef HAS_INET_NTOP
#define HAS_INET_NTOP 1
#endif
#ifndef HAS_SOCKLEN_T
#define HAS_SOCKLEN_T 1
#endif
#ifndef HAS_GETADDRINFO
#define HAS_GETADDRINFO 1
#endif
#ifndef HAS_GETNAMEINFO
#define HAS_GETNAMEINFO 1
#endif
#ifndef NO_MSGAPI
#define NO_MSGAPI 1
#endif
#else
#ifndef HAS_IOCTL
#define HAS_IOCTL 1
#endif
#ifndef HAS_POLL
#define HAS_POLL 1
#endif
#endif

#ifdef HAS_FCNTL
#include <fcntl.h>
#endif

#ifdef HAS_IOCTL
#include <sys/ioctl.h>
#endif

#ifdef HAS_POLL
#include <poll.h>
#endif

#if !defined(HAS_SOCKLEN_T) && !defined(__socklen_t_defined)
typedef int socklen_t;
#endif

#ifndef SOMAXCONN
#define SOMAXCONN 128
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static enet_uint32 timeBase = 0;

int
enet_initialize (void)
{
    return 0;
}

void
enet_deinitialize (void)
{
}

enet_uint32
enet_host_random_seed (void)
{
    struct timeval timeVal;
    
    gettimeofday (& timeVal, NULL);
    
    return (timeVal.tv_sec * 1000) ^ (timeVal.tv_usec / 1000);
}

enet_uint32
enet_time_get (void)
{
    struct timeval timeVal;

    gettimeofday (& timeVal, NULL);

    return timeVal.tv_sec * 1000 + timeVal.tv_usec / 1000 - timeBase;
}

void
enet_time_set (enet_uint32 newTimeBase)
{
    struct timeval timeVal;

    gettimeofday (& timeVal, NULL);
    
    timeBase = timeVal.tv_sec * 1000 + timeVal.tv_usec / 1000 - newTimeBase;
}

int
enet_address_equal (ENetAddress * address1, ENetAddress * address2)
{
    if (address1 -> address.ss_family != address2 -> address.ss_family)
      return 0;

    switch (address1 -> address.ss_family)
    {
    case AF_INET:
    {
        struct sockaddr_in *sin1, *sin2;
        sin1 = (struct sockaddr_in *) & address1 -> address;
        sin2 = (struct sockaddr_in *) & address2 -> address;
        return sin1 -> sin_port == sin2 -> sin_port &&
            sin1 -> sin_addr.s_addr == sin2 -> sin_addr.s_addr;
    }
#ifdef AF_INET6
    case AF_INET6:
    {
        struct sockaddr_in6 *sin6a, *sin6b;
        sin6a = (struct sockaddr_in6 *) & address1 -> address;
        sin6b = (struct sockaddr_in6 *) & address2 -> address;
        return sin6a -> sin6_port == sin6b -> sin6_port &&
            ! memcmp (& sin6a -> sin6_addr, & sin6b -> sin6_addr, sizeof (sin6a -> sin6_addr));
    }
#endif
    default:
    {
        return 0;
    }
    }
}

int
enet_address_set_port (ENetAddress * address, enet_uint16 port)
{
    if (address -> address.ss_family == AF_INET)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *) &address -> address;
        sin -> sin_port = ENET_HOST_TO_NET_16 (port);
        return 0;
    }
#ifdef AF_INET6
    else if (address -> address.ss_family == AF_INET6)
    {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) &address -> address;
        sin6 -> sin6_port = ENET_HOST_TO_NET_16 (port);
        return 0;
    }
#endif
    else
    {
        return -1;
    }
}

int
enet_address_set_address (ENetAddress * address, struct sockaddr * addr, socklen_t addrlen)
{
    if (addrlen > sizeof(struct sockaddr_storage))
      return -1;

    memcpy (&address->address, addr, addrlen);
    address->addressLength = addrlen;
    return 0;
}

int
enet_address_set_host (ENetAddress * address, const char * name)
{
    struct addrinfo hints, * resultList = NULL, * result = NULL;

    memset (& hints, 0, sizeof (hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_ADDRCONFIG;

    if (getaddrinfo (name, NULL, & hints, & resultList) != 0)
      return -1;

    for (result = resultList; result != NULL; result = result -> ai_next)
    {
        memcpy (& address -> address, result -> ai_addr, result -> ai_addrlen);
        address -> addressLength = result -> ai_addrlen;
        
        freeaddrinfo (resultList);
        
        return 0;
    }

    if (resultList != NULL)
      freeaddrinfo (resultList);

    return -1;
}

int
enet_socket_bind (ENetSocket socket, const ENetAddress * address)
{
    return bind (socket,
        (struct sockaddr *) & address -> address,
        address -> addressLength);
}

int
enet_socket_get_address (ENetSocket socket, ENetAddress * address)
{
    address -> addressLength = sizeof (address -> address);

    if (getsockname (socket, (struct sockaddr *) & address -> address, & address -> addressLength) == -1)
      return -1;

    return 0;
}

int 
enet_socket_listen (ENetSocket socket, int backlog)
{
    return listen (socket, backlog < 0 ? SOMAXCONN : backlog);
}

ENetSocket
enet_socket_create (int af, ENetSocketType type)
{
    return socket (af, type == ENET_SOCKET_TYPE_DATAGRAM ? SOCK_DGRAM : SOCK_STREAM, 0);
}

int
enet_socket_set_option (ENetSocket socket, ENetSocketOption option, int value)
{
    int result = -1;
    switch (option)
    {
        case ENET_SOCKOPT_NONBLOCK:
#ifdef HAS_FCNTL
            result = fcntl (socket, F_SETFL, (value ? O_NONBLOCK : 0) | (fcntl (socket, F_GETFL) & ~O_NONBLOCK));
#else
#ifdef HAS_IOCTL
            result = ioctl (socket, FIONBIO, & value);
#else
            result = setsockopt (socket, SOL_SOCKET, SO_NONBLOCK, (char *) & value, sizeof(int));
#endif
#endif
            break;

        case ENET_SOCKOPT_REUSEADDR:
            result = setsockopt (socket, SOL_SOCKET, SO_REUSEADDR, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_RCVBUF:
            result = setsockopt (socket, SOL_SOCKET, SO_RCVBUF, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_SNDBUF:
            result = setsockopt (socket, SOL_SOCKET, SO_SNDBUF, (char *) & value, sizeof (int));
            break;

#ifndef __WIIU__
        case ENET_SOCKOPT_RCVTIMEO:
        {
            struct timeval timeVal;
            timeVal.tv_sec = value / 1000;
            timeVal.tv_usec = (value % 1000) * 1000;
            result = setsockopt (socket, SOL_SOCKET, SO_RCVTIMEO, (char *) & timeVal, sizeof (struct timeval));
            break;
        }

        case ENET_SOCKOPT_SNDTIMEO:
        {
            struct timeval timeVal;
            timeVal.tv_sec = value / 1000;
            timeVal.tv_usec = (value % 1000) * 1000;
            result = setsockopt (socket, SOL_SOCKET, SO_SNDTIMEO, (char *) & timeVal, sizeof (struct timeval));
            break;
        }
#endif

        case ENET_SOCKOPT_NODELAY:
            result = setsockopt (socket, IPPROTO_TCP, TCP_NODELAY, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_QOS:
#ifdef SO_NET_SERVICE_TYPE
            // iOS/macOS
            value = value ? NET_SERVICE_TYPE_VO : NET_SERVICE_TYPE_BE;
            result = setsockopt (socket, SOL_SOCKET, SO_NET_SERVICE_TYPE, (char *) & value, sizeof (int));
#else
#ifdef IP_TOS
            // UNIX - IPv4
            value = value ? 46 << 2 : 0; // DSCP: Expedited Forwarding
            result = setsockopt (socket, IPPROTO_IP, IP_TOS, (char *) & value, sizeof (int));
#endif
#ifdef IPV6_TCLASS
            // UNIX - IPv6
            value = value ? 46 << 2: 0; // DSCP: Expedited Forwarding
            result = setsockopt (socket, IPPROTO_IPV6, IPV6_TCLASS, (char *) & value, sizeof (int));
#endif
#ifdef SO_PRIORITY
            // Linux
            value = value ? 6 : 0; // Max priority without NET_CAP_ADMIN
            result = setsockopt (socket, SOL_SOCKET, SO_PRIORITY, (char *) & value, sizeof (int));
#endif
#endif /* SO_NET_SERVICE_TYPE */
            break;

        default:
            break;
    }
    return result == -1 ? -1 : 0;
}

int
enet_socket_get_option (ENetSocket socket, ENetSocketOption option, int * value)
{
    int result = -1;
    socklen_t len;
    switch (option)
    {
        case ENET_SOCKOPT_ERROR:
            len = sizeof (int);
            result = getsockopt (socket, SOL_SOCKET, SO_ERROR, value, & len);
            break;

        default:
            break;
    }
    return result == -1 ? -1 : 0;
}

int
enet_socket_connect (ENetSocket socket, const ENetAddress * address)
{
    int result;

    result = connect (socket, (struct sockaddr *) & address -> address, address -> addressLength);
    if (result == -1 && errno == EINPROGRESS)
      return 0;

    return result;
}

ENetSocket
enet_socket_accept (ENetSocket socket, ENetAddress * address)
{
    int result;

    if (address != NULL)
      address -> addressLength = sizeof (address -> address);

    result = accept (socket, 
                     address != NULL ? (struct sockaddr *) & address -> address : NULL, 
                     address != NULL ? & address -> addressLength : NULL);
    
    if (result == -1)
      return ENET_SOCKET_NULL;

    return result;
}

int
enet_socket_shutdown (ENetSocket socket, ENetSocketShutdown how)
{
    return shutdown (socket, (int) how);
}

void
enet_socket_destroy (ENetSocket socket)
{
    if (socket != -1)
      close (socket);
}

int
enet_socket_send (ENetSocket socket,
                  const ENetAddress * address,
                  const ENetBuffer * buffers,
                  size_t bufferCount)
{
    int sentLength;
    
#ifdef NO_MSGAPI
    void* sendBuffer;
    size_t sendLength;
    
    if (bufferCount > 1)
    {
        size_t i;
        
        sendLength = 0;
        for (i = 0; i < bufferCount; i++)
        {
            sendLength += buffers[i].dataLength;
        }
        
        sendBuffer = malloc (sendLength);
        if (sendBuffer == NULL)
          return -1;
        
        sendLength = 0;
        for (i = 0; i < bufferCount; i++)
        {
            memcpy (& ((unsigned char *)sendBuffer)[sendLength], buffers[i].data, buffers[i].dataLength);
            sendLength += buffers[i].dataLength;
        }
    }
    else
    {
        sendBuffer = buffers[0].data;
        sendLength = buffers[0].dataLength;
    }
    
    sentLength = sendto (socket, sendBuffer, sendLength, MSG_NOSIGNAL,
        (struct sockaddr *) & address -> address, address -> addressLength);
        
    if (bufferCount > 1)
      free(sendBuffer);
#else
    struct msghdr msgHdr;

    memset (& msgHdr, 0, sizeof (struct msghdr));

    if (address != NULL)
    {
        msgHdr.msg_name = (void*) & address -> address;
        msgHdr.msg_namelen = address -> addressLength;
    }

    msgHdr.msg_iov = (struct iovec *) buffers;
    msgHdr.msg_iovlen = bufferCount;

    sentLength = sendmsg (socket, & msgHdr, MSG_NOSIGNAL);
#endif
    
    if (sentLength == -1)
    {
       if (errno == EWOULDBLOCK)
         return 0;

       return -1;
    }

    return sentLength;
}

int
enet_socket_receive (ENetSocket socket,
                     ENetAddress * address,
                     ENetBuffer * buffers,
                     size_t bufferCount)
{
    int recvLength;

#ifdef NO_MSGAPI
    // This will ONLY work with a single buffer!
    
    address -> addressLength = sizeof (address -> address);
    recvLength = recvfrom (socket, buffers[0].data, buffers[0].dataLength, MSG_NOSIGNAL,
        (struct sockaddr *) & address -> address, & address -> addressLength);
    
    if (recvLength == -1)
    {
       if (errno == EWOULDBLOCK)
         return 0;
     
       return -1;
    }
    
    return recvLength;
#else
    struct msghdr msgHdr;

    memset (& msgHdr, 0, sizeof (struct msghdr));

    if (address != NULL)
    {
        msgHdr.msg_name = & address -> address;
        msgHdr.msg_namelen = sizeof (address -> address);
    }

    msgHdr.msg_iov = (struct iovec *) buffers;
    msgHdr.msg_iovlen = bufferCount;

    recvLength = recvmsg (socket, & msgHdr, MSG_NOSIGNAL);

    if (recvLength == -1)
    {
       if (errno == EWOULDBLOCK)
         return 0;

       return -1;
    }
    
    if (address != NULL)
      address -> addressLength = msgHdr.msg_namelen;

#ifdef HAS_MSGHDR_FLAGS
    if (msgHdr.msg_flags & MSG_TRUNC)
      return -1;
#endif

    return recvLength;
#endif
}

int
enet_socketset_select (ENetSocket maxSocket, ENetSocketSet * readSet, ENetSocketSet * writeSet, enet_uint32 timeout)
{
    struct timeval timeVal;

    timeVal.tv_sec = timeout / 1000;
    timeVal.tv_usec = (timeout % 1000) * 1000;

    return select (maxSocket + 1, readSet, writeSet, NULL, & timeVal);
}

int
enet_socket_wait (ENetSocket socket, enet_uint32 * condition, enet_uint32 timeout)
{
#ifdef HAS_POLL
    struct pollfd pollSocket;
    int pollCount;
    
    pollSocket.fd = socket;
    pollSocket.events = 0;

    if (* condition & ENET_SOCKET_WAIT_SEND)
      pollSocket.events |= POLLOUT;

    if (* condition & ENET_SOCKET_WAIT_RECEIVE)
      pollSocket.events |= POLLIN;

    pollCount = poll (& pollSocket, 1, timeout);

    if (pollCount < 0)
    {
        if (errno == EINTR && * condition & ENET_SOCKET_WAIT_INTERRUPT)
        {
            * condition = ENET_SOCKET_WAIT_INTERRUPT;

            return 0;
        }

        return -1;
    }

    * condition = ENET_SOCKET_WAIT_NONE;

    if (pollCount == 0)
      return 0;

    if (pollSocket.revents & POLLOUT)
      * condition |= ENET_SOCKET_WAIT_SEND;
    
    if (pollSocket.revents & POLLIN)
      * condition |= ENET_SOCKET_WAIT_RECEIVE;

    return 0;
#else
    fd_set readSet, writeSet;
    struct timeval timeVal;
    int selectCount;

    timeVal.tv_sec = timeout / 1000;
    timeVal.tv_usec = (timeout % 1000) * 1000;

    FD_ZERO (& readSet);
    FD_ZERO (& writeSet);

    if (* condition & ENET_SOCKET_WAIT_SEND)
      FD_SET (socket, & writeSet);

    if (* condition & ENET_SOCKET_WAIT_RECEIVE)
      FD_SET (socket, & readSet);

    selectCount = select (socket + 1, & readSet, & writeSet, NULL, & timeVal);

    if (selectCount < 0)
    {
        if (errno == EINTR && * condition & ENET_SOCKET_WAIT_INTERRUPT)
        {
            * condition = ENET_SOCKET_WAIT_INTERRUPT;

            return 0;
        }
      
        return -1;
    }

    * condition = ENET_SOCKET_WAIT_NONE;

    if (selectCount == 0)
      return 0;

    if (FD_ISSET (socket, & writeSet))
      * condition |= ENET_SOCKET_WAIT_SEND;

    if (FD_ISSET (socket, & readSet))
      * condition |= ENET_SOCKET_WAIT_RECEIVE;

    return 0;
#endif
}

#endif

