#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>
#include <cstdint>
typedef int SOCKET;
typedef struct { int x; } WSADATA;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define WSAENOTSOCK 10038
#define WSANOTINITIALISED 10093
#define MAKEWORD(a,b) ((a)|((b)<<8))
inline int WSAStartup(int, WSADATA*) { return 0; }
inline int WSACleanup() { return 0; }
inline int WSAGetLastError() { return WSAENOTSOCK; }
inline int closesocket(int s) { return close(s); }
inline void Sleep(unsigned) {}
extern unsigned long long g_fakeTick;
inline unsigned long long GetTickCount64() { return g_fakeTick; }
inline long recvfrom(int s, char* b, int n, int f, sockaddr* a, int* l) { socklen_t sl = (socklen_t)*l; long r = ::recvfrom(s, b, (size_t)n, f, a, &sl); *l = (int)sl; return r; }
