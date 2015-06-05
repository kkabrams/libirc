#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/un.h>
#include "irc.h"

//#define DEBUG "epoch" //nick or channel to send debug info to.
#define CHUNK 4096

#define SILLYLIMIT 1024

/* how this works:
if server == |/program then open a socketpair and fork and exec program
else try serv as IPv6, as IPv4, resolve as IPv6, resolve as IPv4
connect to that.
*/
int serverConnect(char *serv,char *port) {
 int fd=-1;
 int s[2];
 int pid;
 int try_ipv4;
 char *name[3];
 char buf[SILLYLIMIT];
 struct addrinfo hints, *servinfo, *p=0;
 struct hostent *he;
 struct sockaddr_in saddr;
 struct sockaddr_in6 saddr6;
 printf("libirc: serverConnect: %s %s\n",serv,port);
 if(!serv || !port) return -1;
 if(*serv == '|') {
  name[0]=serv+1;
  name[1]=port;
  name[2]=0;
  socketpair(PF_LOCAL,SOCK_STREAM,0,s);
  if(!(pid=fork())) {
   dup2(s[1],0);
   dup2(s[1],1);
   execv(name[0],name);
  }
  if(pid == -1) return -1;
  return s[0];
 }
 memset(&hints,0,sizeof hints);
 hints.ai_socktype=SOCK_STREAM;
 hints.ai_protocol=IPPROTO_TCP;
 for(try_ipv4=0;try_ipv4 < 2;try_ipv4++) {
  hints.ai_family=try_ipv4?AF_INET:AF_INET6;
  if(fd != -1) close(fd);
  if((fd=socket(hints.ai_family,SOCK_STREAM,IPPROTO_TCP)) < 0) return -1;
  if(!(he=gethostbyname2(
          try_ipv4
            ?inet_aton(serv,&(saddr.sin_addr))
              ?inet_ntoa(saddr.sin_addr)
              :serv
            :inet_pton(AF_INET6,serv,&(saddr6.sin6_addr))
              ?inet_ntop(AF_INET6,&(saddr6.sin6_addr),buf,SILLYLIMIT)
              :serv
          ,hints.ai_family))) continue;
  for(;*(he->h_addr_list);(he->h_addr_list)++) {
   inet_ntop(hints.ai_family,*(he->h_addr_list),buf,SILLYLIMIT);
   if(!getaddrinfo(buf,port,&hints,&servinfo))
    for(p=servinfo;p;p=p->ai_next)
     if(connect(fd,p->ai_addr, p->ai_addrlen) < 0) continue; else return fd;
  }
 }
 printf("I tried as hard as I could and couldn't connect to %s:%s\n",serv,port);
 return -1;
}

int fdlen(int *fds) {
 int i;
 for(i=0;fds[i] != -1;i++);
 return i+1;
}

int runem(int *fds,void (*line_handler)(),void (*extra_handler)()) {
 int j;
 int fdl=fdlen(fds);
 fd_set master;
 fd_set readfs;
 struct timeval timeout;
 int fdmax=0,n,i;
 char *backlogs[fdl];
 char *t,*line=0;
 int blsize=CHUNK;
 int bllen=0;
 char buffers[fdl][CHUNK];//THIS IS *NOT* NULL TERMINATED.
 FD_ZERO(&master);
 FD_ZERO(&readfs);
 for(i=0;fds[i] != -1;i++) {
  FD_SET(fds[i],&master);
  backlogs[i]=malloc(CHUNK+1);
  memset(backlogs[i],0,CHUNK);
  memset(buffers[i],0,CHUNK);
  fdmax=fds[i]>fdmax?fds[i]:fdmax;
 }
 for(;;) {
  readfs=master;
  timeout.tv_sec=0;
  timeout.tv_usec=1000;
  if((j=select(fdmax+1,&readfs,0,0,&timeout)) == -1 ) return perror("select"),1;
  for(i=0;fds[i] != -1;i++) if(extra_handler) extra_handler(fds[i]);
  if(j == 0) continue;//don't bother to loop over them.
  printf("getting there.\n");
  for(i=0;fds[i] != -1;i++) {
   if(!FD_ISSET(fds[i],&readfs)) continue;
   if((n=recv(fds[i],buffers[i],CHUNK,0)) <= 0) return (perror("recv"),2);
   buffers[i][n]=0;//deff right.
   if(bllen+n >= blsize) {//this is probably off...
    blsize+=n;
    t=malloc(blsize);
    if(!t) exit(253);
    memcpy(t,backlogs[i],blsize-n+1);
    free(backlogs[i]);
    backlogs[i]=t;
   }
   memcpy(backlogs[i]+bllen,buffers[i],n);
   bllen+=n;

//HERE EPOCH
   while((t=strstr(backlogs[i],"\r\n"))) {
    line=backlogs[i];
    if(!strncmp(line,"PING",4)) {
     line[1]='O';
     write(fds[i],line,t-backlogs[i]+2);
    } else {
     if(!strncmp(line,"ERROR",5)) return 0;    
     *t=0;
     line_handler(fds[i],line);
    }
    bllen-=((t+2)-backlogs[i]);
    if(bllen <= 0) bllen=0;
    else memmove(backlogs[i],(t+2),bllen);
   }
  }
 }
 return 0;
}

//wrap runem to keep runit around :P
int runit(int fd,void (*line_handler)(),void (*extra_handler)()) {
 int fds[2];
 fds[0]=fd;
 fds[1]=-1;
 return runem(fds,line_handler,extra_handler);
}

//not needed?
int ircConnect(char *serv,char *port,char *nick,char *user) {
 char sendstr[1024];
 int fd;
 fd=serverConnect(serv,port);
 if(!fd) {
  return 0;
 }
 snprintf(sendstr,sizeof(sendstr)-1,"NICK %s\r\nUSER %s\r\n",nick,user);
 write(fd,sendstr,strlen(sendstr));
 return fd;
}

//this function mangles the input.
//gotta free the returned pointer but not each pointer in the array.
char **line_cutter(int fd,char *line,struct user *user) {
 int i;
 char **a=malloc(sizeof(char *) * 256);//heh.
 memset(a,0,sizeof(char *) * 256);
 if(!user) return 0;
 user->nick=0;
 user->user=0;
 user->host=0;
 if(!line) return 0;
 if(strchr(line,'\r')) *strchr(line,'\r')=0;
 if(strchr(line,'\n')) *strchr(line,'\n')=0;
 if(line[0]==':') {
  if((user->nick=strchr(line,':'))) {
   *(user->nick)=0;
   (user->nick)++;
  }
 }
 if(user->nick) {

  if((a[0]=strchr((user->nick),' '))) {
   *a[0]=0;
   a[0]++;
   for(i=0;(a[i+1]=strchr(a[i],' '));i++) {
    *a[i+1]=0;
    a[i+1]++;
    if(*a[i+1] == ':') {//we're done.
     *a[i+1]=0;
     a[i+1]++;
     break;
    }
   }
  }

  if(((user->user)=strchr((user->nick),'!'))) {
   *(user->user)=0;
   (user->user)++;
   if(((user->host)=strchr((user->user),'@'))) {
    *(user->host)=0;
    (user->host)++;
   }
  } else {
   user->host=user->nick;
  }
 }
 return a;
}
