#pragma once

#if (defined(_WIN32) || defined(_WIN64))

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <winsock2.h>
#include "Ws2tcpip.h"
#pragma comment(lib, "Ws2_32.lib")

#else /* Linux or MAC */

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

typedef int SOCKET;
#define INVALID_SOCKET -1

#endif
