#define _GNU_SOURCE
#include "common.h"
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>
#define LIMIT 64
static volatile sig_atomic_t stopping;
static void stop(int sig) { (void)sig; stopping=1; }
static void response(const char *q,char *out,size_t size) {
    if(!strcmp(q,"PING")) snprintf(out,size,"OK PONG pid=%ld",(long)getpid());
    else if(!strcmp(q,"SYSINFO")) {
        struct utsname u;
        if(uname(&u)<0) snprintf(out,size,"ERR system information unavailable");
        else snprintf(out,size,"OK host=%s os=%s kernel=%s machine=%s pid=%ld",u.nodename,u.sysname,u.release,u.machine,(long)getpid());
    } else if(!strcmp(q,"MEMORY")) {
        struct sysinfo s;
        if(sysinfo(&s)<0) snprintf(out,size,"ERR memory unavailable");
        else snprintf(out,size,"OK uptime_seconds=%ld total_bytes=%llu free_bytes=%llu load1=%.2f",s.uptime,(unsigned long long)s.totalram*s.mem_unit,(unsigned long long)s.freeram*s.mem_unit,s.loads[0]/65536.0);
    } else if(!strcmp(q,"DISK")) {
        struct statvfs s;
        if(statvfs("/",&s)<0) snprintf(out,size,"ERR disk unavailable");
        else snprintf(out,size,"OK path=/ total_bytes=%llu available_bytes=%llu",(unsigned long long)s.f_blocks*s.f_frsize,(unsigned long long)s.f_bavail*s.f_frsize);
    } else if(!strncmp(q,"HOLD ",5)) {
        int seconds=number(q+5,1,10);
        if(seconds<0) snprintf(out,size,"ERR HOLD requires 1..10 seconds");
        else { sleep((unsigned)seconds); snprintf(out,size,"OK held=%d pid=%ld",seconds,(long)getpid()); }
    } else if(!strcmp(q,"QUIT")) snprintf(out,size,"OK goodbye");
    else snprintf(out,size,"ERR unknown command");
}
static void serve(int fd,int robust,int timeout) {
    char q[MAX_FRAME+1],out[MAX_FRAME+1];
    while(!stopping) {
        if(recv_frame(fd,q,sizeof q,timeout,robust)<0) {
            log_message("INFO","connection ended: %s",strerror(errno)); break;
        }
        LOGD("command=%s",q); response(q,out,sizeof out);
        /* A simplistic parser drops the session instead of returning an error. */
        if(!robust && !strncmp(out,"ERR",3)) break;
        if(send_frame(fd,out,timeout)<0 || !strcmp(q,"QUIT")) break;
    }
}
int main(int argc,char **argv) {
    int port=9090,robust=1,timeout=3000; const char *bind_ip="127.0.0.1";
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--debug")) debug_enabled=1;
        else if(!strcmp(argv[i],"--baseline")) robust=0;
        else if(!strcmp(argv[i],"--port") && i+1<argc) port=number(argv[++i],1,65535);
        else if(!strcmp(argv[i],"--timeout-ms") && i+1<argc) timeout=number(argv[++i],100,60000);
        else if(!strcmp(argv[i],"--bind") && i+1<argc) bind_ip=argv[++i];
        else { fprintf(stderr,"Usage: server [--port N] [--bind IPv4] [--timeout-ms N] [--debug] [--baseline]\n"); return 2; }
    }
    if(port<0 || timeout<0) return 2;
    struct sigaction sa={0}; sigemptyset(&sa.sa_mask); sa.sa_handler=stop;
    if(sigaction(SIGTERM,&sa,NULL)<0 || sigaction(SIGINT,&sa,NULL)<0) { perror("sigaction"); return 1; }
    int listener=socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK,0),one=1;
    if(listener<0) { perror("socket"); return 1; }
    struct sockaddr_in a={0}; a.sin_family=AF_INET; a.sin_port=htons((unsigned short)port);
    if(inet_pton(AF_INET,bind_ip,&a.sin_addr)!=1) { close(listener); return 2; }
    if(setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one)<0 || bind(listener,(void*)&a,sizeof a)<0 || listen(listener,64)<0) { perror("listen/bind"); close(listener); return 1; }
    pid_t children[LIMIT]={0}; int active=0;
    log_message("INFO","listening %s:%d mode=%s",bind_ip,port,robust?"robust":"baseline");
    while(!stopping) {
        int status; pid_t p;
        while((p=waitpid(-1,&status,WNOHANG))>0) {
            for(int i=0;i<LIMIT;i++) if(children[i]==p) { children[i]=0; active--; break; }
            log_message("INFO","reaped child=%ld signal=%d active=%d",(long)p,WIFSIGNALED(status)?WTERMSIG(status):0,active);
        }
        struct pollfd f={listener,POLLIN,0}; int r=poll(&f,1,100);
        if(r<0) { if(errno==EINTR) continue; perror("poll"); break; } if(!r) continue;
        int fd=accept(listener,NULL,NULL);
        if(fd<0) { if(errno!=EINTR && errno!=EAGAIN) { perror("accept"); usleep(100000); } continue; }
        if(active==LIMIT) { close(fd); log_message("INFO","capacity reached"); continue; }
        p=fork();
        if(p==0) {
            close(listener);
            sa.sa_handler=SIG_DFL; sigaction(SIGTERM,&sa,NULL); sigaction(SIGINT,&sa,NULL);
            serve(fd,robust,timeout); close(fd); _exit(0);
        }
        close(fd);
        if(p<0) { perror("fork"); continue; }
        for(int i=0;i<LIMIT;i++) if(!children[i]) { children[i]=p; active++; break; }
        log_message("INFO","accepted child=%ld active=%d",(long)p,active);
    }
    close(listener);
    for(int i=0;i<LIMIT;i++) if(children[i]) kill(children[i],SIGTERM);
    while(waitpid(-1,NULL,0)>0 || errno==EINTR) {}
    log_message("INFO","shutdown complete"); return 0;
}
