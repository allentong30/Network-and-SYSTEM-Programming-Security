#define _POSIX_C_SOURCE 200809L
#include "common.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
int debug_enabled;
void log_message(const char *level, const char *fmt, ...) {
    char line[1024];
    int n = snprintf(line, sizeof line, "[%s pid=%ld] ", level, (long)getpid());
    va_list ap; va_start(ap, fmt);
    vsnprintf(line+n, sizeof line-(size_t)n, fmt, ap); va_end(ap);
    size_t len = strlen(line); line[len++] = '\n';
    ssize_t written = write(STDERR_FILENO, line, len);
    (void)written;
}
int number(const char *s, int min, int max) {
    char *end; errno=0; long n=strtol(s,&end,10);
    return errno || !*s || *end || n<min || n>max ? -1 : (int)n;
}
static long long now_ms(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return (long long)t.tv_sec*1000+t.tv_nsec/1000000;
}
static int ready(int fd, short events, long long deadline) {
    for (;;) {
        long long left=deadline-now_ms();
        if(left<=0) { errno=ETIMEDOUT; return -1; }
        struct pollfd p={fd,events,0};
        int r=poll(&p,1,left>INT_MAX?INT_MAX:(int)left);
        if(r>0) return 0;
        if(r==0) { errno=ETIMEDOUT; return -1; }
        if(errno!=EINTR) return -1;
    }
}
static int transfer(int fd, void *buf, size_t n, int writing, long long deadline) {
    size_t done=0;
    while(done<n) {
        if(ready(fd,writing?POLLOUT:POLLIN,deadline)<0) return -1;
        ssize_t r=writing?send(fd,(char*)buf+done,n-done,MSG_NOSIGNAL|MSG_DONTWAIT):
                          recv(fd,(char*)buf+done,n-done,MSG_DONTWAIT);
        if(r==0) { errno=done?EPROTO:ECONNRESET; return -1; }
        if(r<0) { if(errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK) continue; return -1; }
        done+=(size_t)r;
    }
    return 0;
}
int send_frame(int fd, const char *s, int timeout_ms) {
    size_t n=strlen(s); if(!n || n>MAX_FRAME) { errno=EMSGSIZE; return -1; }
    uint32_t len=htonl((uint32_t)n); long long deadline=now_ms()+timeout_ms;
    LOGD("send frame bytes=%zu",n);
    if(transfer(fd,&len,4,1,deadline)<0) return -1;
    return transfer(fd,(void*)s,n,1,deadline);
}
int recv_frame(int fd, char *s, size_t cap, int timeout_ms, int robust) {
    uint32_t net; long long deadline=now_ms()+timeout_ms;
    /* Baseline intentionally assumes recv returns each complete field. */
    if(robust) { if(transfer(fd,&net,4,0,deadline)<0) return -1; }
    else if(recv(fd,&net,4,MSG_WAITALL)!=4) return -1;
    size_t n=ntohl(net);
    /* Bounds are retained even in teaching mode: never introduce memory corruption. */
    if(!n || n>MAX_FRAME || n>=cap) { errno=EMSGSIZE; return -1; }
    if(robust) { if(transfer(fd,s,n,0,deadline)<0) return -1; }
    else { ssize_t r=recv(fd,s,n,0); if(r!=(ssize_t)n) { errno=EPROTO; return -1; } }
    if(memchr(s,0,n)) { errno=EPROTO; return -1; }
    s[n]=0; LOGD("received frame bytes=%zu",n); return (int)n;
}
int connect_server(const char *host, int port, int timeout_ms) {
    char service[16]; snprintf(service,sizeof service,"%d",port);
    struct addrinfo hints={0},*list; hints.ai_socktype=SOCK_STREAM; hints.ai_family=AF_INET;
    if(getaddrinfo(host,service,&hints,&list)) { errno=EHOSTUNREACH; return -1; }
    int fd=-1;
    for(struct addrinfo *a=list;a;a=a->ai_next) {
        fd=socket(a->ai_family,a->ai_socktype,a->ai_protocol); if(fd<0) continue;
        int flags=fcntl(fd,F_GETFL,0);
        if(flags<0 || fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0) { close(fd); fd=-1; continue; }
        int r=connect(fd,a->ai_addr,a->ai_addrlen);
        if(r<0 && errno==EINPROGRESS) {
            r=ready(fd,POLLOUT,now_ms()+timeout_ms);
            if(r==0) { int e=0; socklen_t size=sizeof e; if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&e,&size)<0) r=-1; else if(e) { errno=e; r=-1; } }
        }
        if(r==0 && fcntl(fd,F_SETFL,flags)==0) break;
        int saved=errno; close(fd); fd=-1; errno=saved;
    }
    freeaddrinfo(list); return fd;
}
