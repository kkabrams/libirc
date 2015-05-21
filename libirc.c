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

//int main(int argc,char *argv[]) {
// return 0;
//}

#define SILLYLIMIT 1024

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
 int fdmax=0,n,s,i;
 int fd;
 char *backlogs[fdl];
 char *t,*line=0;
 int blsize=CHUNK;
 int bllen=0;
 char buffers[fdl][CHUNK];//THIS IS *NOT* NULL TERMINATED.
 FD_ZERO(&master);
 FD_ZERO(&readfs);
 for(i=0;fds[i] != -1;i++) {
  //if(!backlogs[i]) return 252;//wtf is this here for? ofc they're not set!
  FD_SET(fds[i],&master);
  backlogs[i]=malloc(CHUNK+1);
  memset(backlogs[i],0,CHUNK);
  memset(buffers[i],0,CHUNK);
  fdmax=fds[i]>fdmax?fds[i]:fdmax;
 }
 int done=0;
 while(!done) {
  for(fd=0;fd<=fdmax;fd++) {
   if(FD_ISSET(fd,&master)) {
    if(extra_handler) extra_handler(fd);
   }
  }
  readfs=master;
  timeout.tv_sec=0;
  timeout.tv_usec=1000;
  if( select(fdmax+1,&readfs,0,0,&timeout) == -1 ) {
   printf("\n!!!It is crashing here!!!\n\n");
   perror("select");
   return 1;
  }
  for(i=0;fds[i] != -1;i++) {
   if(FD_ISSET(fds[i],&readfs)) {
    if((n=recv(fds[i],buffers[i],CHUNK,0)) <= 0) {//read CHUNK bytes
     if(n) {
      fprintf(stderr,"recv: %d\n",n);
      perror("recv");
     } else {
      fprintf(stderr,"connection closed. fd: %d\n",fds[i]);
     }
     return 2;
    } else {
     buffers[i][n]=0;//deff right.
     if(bllen+n >= blsize) {//this is probably off...
      blsize+=n;
      t=malloc(blsize);
      if(!t) {
       printf("OH FUCK! MALLOC FAILED!\n");
       exit(253);
      }
      memset(t,0,blsize);//optional?
      memcpy(t,backlogs[i],blsize-n+1);//???
      free(backlogs[i]);
      backlogs[i]=t;
     }
     memcpy(backlogs[i]+bllen,buffers[i],n);
     bllen+=n;
     for(j=0,s=0;j<bllen;j++) {
      if(backlogs[i][j]=='\n') {
       line=malloc(j-s+3);//on linux it crashes without the +1 +3? weird. when did I do that?
       if(!line) {
        printf("ANOTHER malloc error!\n");
        exit(254);
       }
       memcpy(line,backlogs[i]+s,j-s+2);
       line[j-s+1]=0;//gotta null terminate this. line_handler expects it .
       s=j+1;//the character after the newline.
       if(!strncmp(line,"PING",4)) {
        t=malloc(strlen(line));
        strcpy(t,"PONG ");
        strcat(t,line+6);
        write(fds[i],t,strlen(t));
 #ifdef DEBUG
        printf("%s\nPONG %s\n",line,line+6);
        write(fds[i],"PRIVMSG %s :PONG! w00t!\r\n",DEBUG,28);
 #endif
       } else if(!strncmp(line,"ERROR",5)) {
 #ifdef DEBUG
        printf("error: %s\n",line);
 #endif
        return 0;
       } else {
        line_handler(fds[i],line);
       }
       free(line);
      }
     }
     //left shift the backlog so the last thing we got to is at the start
     if(s > bllen) { //if the ending position is after the size of the backlog...
      bllen=0;//fuck shifting. :P
     } else {
      for(j=s;j<=bllen;j++) {//should work.
       backlogs[i][j-s]=backlogs[i][j];
      }
      bllen-=s;
     }
    }
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
