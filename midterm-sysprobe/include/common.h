#ifndef COMMON_H
#define COMMON_H
#include <stddef.h>
#define MAX_FRAME 4096
extern int debug_enabled;
void log_message(const char *level, const char *fmt, ...);
#ifdef DEBUG
#define LOGD(...) do { if (debug_enabled) log_message("DEBUG", __VA_ARGS__); } while (0)
#else
#define LOGD(...) ((void)0)
#endif
int number(const char *s, int min, int max);
int send_frame(int fd, const char *s, int timeout_ms);
int recv_frame(int fd, char *s, size_t cap, int timeout_ms, int robust);
int connect_server(const char *host, int port, int timeout_ms);
#endif
