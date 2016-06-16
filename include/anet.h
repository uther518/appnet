
#ifndef ANET_H
#define ANET_H

#define ANET_OK 0
#define ANET_ERR -1
#define ANET_ERR_LEN 256

/* Flags used with certain functions. */
#define ANET_NONE 0
#define ANET_IP_ONLY (1<<0)

#if defined(__sun) || defined(_AIX)
#define AF_LOCAL AF_UNIX
#endif

#ifdef _AIX
#undef ip_len
#endif
#include <stddef.h>
#include <sys/stat.h>
int anetTcpConnect(char *err, char *addr, int port);
int anetTcpNonBlockConnect(char *err, char *addr, int port);
int anetTcpNonBlockBindConnect(char *err, char *addr, int port, char *source_addr);
int anetTcpNonBlockBestEffortBindConnect(char *err, char *addr, int port, char *source_addr);
int anetUnixConnect(char *err, char *path);
int anetUnixNonBlockConnect(char *err, char *path);
int anetRead(int fd, char *buf, int count);
int anetRecv(int fd, char *buf, int count);
int anetResolve(char *err, char *host, char *ipbuf, size_t ipbuf_len);
int anetResolveIP(char *err, char *host, char *ipbuf, size_t ipbuf_len);
int anetTcpServer(char *err, int port, char *bindaddr, int backlog);
int anetTcp6Server(char *err, int port, char *bindaddr, int backlog);
int anetUnixServer(char *err, char *path, mode_t perm, int backlog);
int anetTcpAccept(char *err, int serversock, char *ip, size_t ip_len, int *port);
int anetUnixAccept(char *err, int serversock);
int anetWrite(int fd, char *buf, int count);
int anetNonBlock(char *err, int fd);
int anetBlock(char *err, int fd);
int anetEnableTcpNoDelay(char *err, int fd);
int anetDisableTcpNoDelay(char *err, int fd);
int anetTcpKeepAlive(char *err, int fd);
int anetSendTimeout(char *err, int fd, long long ms);
int anetPeerToString(int fd, char *ip, size_t ip_len, int *port);
int anetKeepAlive(char *err, int fd, int interval);
int anetSockName(int fd, char *ip, size_t ip_len, int *port);
int anetHandup( int fd, int timeout_ms, int events );
int listenToPort( char *bindaddr, int port, int* fds , int *count );

#endif
