/** 
 @file  win32.c
 @brief ENet Win32 system specific functions
*/
#ifdef _WIN32

#define ENET_BUILDING_LIB 1
#include "enet/enet.h"
#include <windows.h>
#include <mmsystem.h>
#include <qos2.h>
#include <timeapi.h>

static enet_uint32 timeBase = 0;
static HANDLE qosHandle = INVALID_HANDLE_VALUE;
static BOOL qosAddedFlow;

static HMODULE QwaveLibraryHandle;

int
enet_initialize (void)
{
    WORD versionRequested = MAKEWORD (2, 0);
    WSADATA wsaData;
   
    if (WSAStartup (versionRequested, & wsaData))
       return -1;

    if (LOBYTE (wsaData.wVersion) != 2||
        HIBYTE (wsaData.wVersion) != 0)
    {
       WSACleanup ();
       
       return -1;
    }


    

    return 0;
}

void
enet_deinitialize (void)
{
    WSACleanup ();
}

enet_uint32
enet_host_random_seed (void)
{
    return (enet_uint32)GetTickCount64();
}

enet_uint32
enet_time_get (void)
{
    return (enet_uint32)GetTickCount64() - timeBase;
}

void
enet_time_set (enet_uint32 newTimeBase)
{
    timeBase = (enet_uint32)GetTickCount64() - newTimeBase;
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
    else if (address -> address.ss_family == AF_INET6)
    {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) &address -> address;
        sin6 -> sin6_port = ENET_HOST_TO_NET_16 (port);
        return 0;
    }
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
            sin1 -> sin_addr.S_un.S_addr == sin2 -> sin_addr.S_un.S_addr;
    }
    case AF_INET6:
    {
        struct sockaddr_in6 *sin6a, *sin6b;
        sin6a = (struct sockaddr_in6 *) & address1 -> address;
        sin6b = (struct sockaddr_in6 *) & address2 -> address;
        return sin6a -> sin6_port == sin6b -> sin6_port &&
            ! memcmp (& sin6a -> sin6_addr, & sin6b -> sin6_addr, sizeof (sin6a -> sin6_addr));
    }
    default:
    {
        return 0;
    }
    }
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
    return listen (socket, backlog < 0 ? SOMAXCONN : backlog) == SOCKET_ERROR ? -1 : 0;
}

ENetSocket
enet_socket_create (int af, ENetSocketType type)
{
    return socket (af, type == ENET_SOCKET_TYPE_DATAGRAM ? SOCK_DGRAM : SOCK_STREAM, 0);
}

int
enet_socket_set_option (ENetSocket socket, ENetSocketOption option, int value)
{
    int result = SOCKET_ERROR;
    switch (option)
    {
        case ENET_SOCKOPT_NONBLOCK:
        {
            u_long nonBlocking = (u_long) value;
            result = ioctlsocket (socket, FIONBIO, & nonBlocking);
            break;
        }

        case ENET_SOCKOPT_REUSEADDR:
            result = setsockopt (socket, SOL_SOCKET, SO_REUSEADDR, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_RCVBUF:
            result = setsockopt (socket, SOL_SOCKET, SO_RCVBUF, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_SNDBUF:
            result = setsockopt (socket, SOL_SOCKET, SO_SNDBUF, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_RCVTIMEO:
            result = setsockopt (socket, SOL_SOCKET, SO_RCVTIMEO, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_SNDTIMEO:
            result = setsockopt (socket, SOL_SOCKET, SO_SNDTIMEO, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_NODELAY:
            result = setsockopt (socket, IPPROTO_TCP, TCP_NODELAY, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_QOS:
        {
            result = 0;
            break;
        }

        default:
            break;
    }
    return result == SOCKET_ERROR ? -1 : 0;
}

int
enet_socket_get_option (ENetSocket socket, ENetSocketOption option, int * value)
{
    int result = SOCKET_ERROR, len;
    switch (option)
    {
        case ENET_SOCKOPT_ERROR:
            len = sizeof(int);
            result = getsockopt (socket, SOL_SOCKET, SO_ERROR, (char *) value, & len);
            break;

        default:
            break;
    }
    return result == SOCKET_ERROR ? -1 : 0;
}

int
enet_socket_connect (ENetSocket socket, const ENetAddress * address)
{
    int result;

    result = connect (socket, (struct sockaddr *) & address -> address, address -> addressLength);
    if (result == SOCKET_ERROR && WSAGetLastError () != WSAEWOULDBLOCK)
      return -1;

    return 0;
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
    return shutdown (socket, (int) how) == SOCKET_ERROR ? -1 : 0;
}

void
enet_socket_destroy (ENetSocket socket)
{
    if (socket != INVALID_SOCKET)
      closesocket (socket);
}

int
enet_socket_send (ENetSocket socket,
                  const ENetAddress * address,
                  const ENetBuffer * buffers,
                  size_t bufferCount)
{
    DWORD sentLength;
    
    if (WSASendTo (socket, 
                   (LPWSABUF) buffers,
                   (DWORD) bufferCount,
                   & sentLength,
                   0,
                   address != NULL ? (struct sockaddr *) & address -> address : NULL,
                   address != NULL ? address -> addressLength : 0,
                   NULL,
                   NULL) == SOCKET_ERROR)
    {
       if (WSAGetLastError () == WSAEWOULDBLOCK)
         return 0;

       return -1;
    }

    return (int) sentLength;
}

int
enet_socket_receive (ENetSocket socket,
                     ENetAddress * address,
                     ENetBuffer * buffers,
                     size_t bufferCount)
{
    DWORD flags = 0,
          recvLength;
    
    if (address != NULL)
      address -> addressLength = sizeof (address -> address);

    if (WSARecvFrom (socket,
                     (LPWSABUF) buffers,
                     (DWORD) bufferCount,
                     & recvLength,
                     & flags,
                     address != NULL ? (struct sockaddr *) & address -> address : NULL,
                     address != NULL ? & address -> addressLength : NULL,
                     NULL,
                     NULL) == SOCKET_ERROR)
    {
       switch (WSAGetLastError ())
       {
       case WSAEWOULDBLOCK:
       case WSAECONNRESET:
          return 0;
       }

       return -1;
    }

    if (flags & MSG_PARTIAL)
      return -1;

    return (int) recvLength;
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
      return -1;

    * condition = ENET_SOCKET_WAIT_NONE;

    if (selectCount == 0)
      return 0;

    if (FD_ISSET (socket, & writeSet))
      * condition |= ENET_SOCKET_WAIT_SEND;
    
    if (FD_ISSET (socket, & readSet))
      * condition |= ENET_SOCKET_WAIT_RECEIVE;

    return 0;
} 

#endif

