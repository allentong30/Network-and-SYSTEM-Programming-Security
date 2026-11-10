#define _POSIX_C_SOURCE 200809L
#include "common.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(int argc,char **argv) {
    if(argc<4 || argc>5 || (argc==5 && strcmp(argv[4],"--debug"))) {
        fprintf(stderr,"Usage: %s HOST PORT 'SYSINFO|MEMORY|DISK|PING|HOLD 1..10|QUIT' [--debug]\n",argv[0]); return 2;
    }
    int port=number(argv[2],1,65535); if(port<0) return 2;
    debug_enabled=argc==5;
    int fd=connect_server(argv[1],port,3000);
    if(fd<0) { perror("connect"); return 1; }
    char response[MAX_FRAME+1];
    if(send_frame(fd,argv[3],3000)<0 || recv_frame(fd,response,sizeof response,15000,1)<0) {
        perror("request"); close(fd); return 1;
    }
    puts(response); close(fd); return strncmp(response,"OK ",3)?1:0;
}
