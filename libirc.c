#define _GNU_SOURCE //I want memmem out of string.h
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/un.h>

//epoch's lib includes:
#include <idc.h> //going to use this for mainloop instead of writing one in here.
#include "irc.h"

//#define DEBUG "epoch" //nick or channel to send debug info to.
#define CHUNK 4096

#define SILLYLIMIT 1024

//extern struct idc_global idc;//not sure if this is needed.

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
 char *name[4];
 char buf[SILLYLIMIT];
 struct addrinfo hints, *servinfo, *p=0;
 struct hostent *he;
 struct sockaddr_in saddr;
 struct sockaddr_in6 saddr6;
 //printf("libirc: serverConnect: %s %s\n",serv,port);
 if(!serv || !port) return -1;
 if(*serv == '|') {
  name[0]=serv+1;
  name[1]=port;
  if((name[2]=strchr(port,' '))) {
   *name[2]=0;
   name[2]++;
   name[3]=0;
  }
  socketpair(PF_LOCAL,SOCK_STREAM,0,s);
  if(!(pid=fork())) {
   dup2(s[1],fileno(stdin));
   dup2(s[1],fileno(stdout));
   execv(name[0],name);
  }
  if(pid == -1) return -1;
  //printf("libirc: serverConnect: returning something! %d\n",fd);
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
   if(!getaddrinfo(buf,port,&hints,&servinfo)) {
    for(p=servinfo;p;p=p->ai_next) {
     if(connect(fd,p->ai_addr, p->ai_addrlen) < 0) {
      //printf("libirc: serverConnect: trying something else...\n");
      continue;
     } else {
      //printf("libirc: serverConnect: returning something! %d\n",fd);
      return fd;
     }
    }
   }
  }
 }
 //printf("I tried as hard as I could and couldn't connect to %s:%s\n",serv,port);
 return -1;
}

int fdlen(int *fds) {
 int i;
 for(i=0;fds[i] != -1;i++);
 return i+1;
}

char *memstr(char *s,char *find,size_t l) {
 return memmem(s,l,find,strlen(find));
}

/* already in string.h!
char *memmem(char *s,char *find,size_t sl,size_t fl) {
 size_t i,j,fl;
 if(!s) return 0;
 if(!find) return 0;
 for(i=0;i<l;i++) {
  for(j=0;j<fl;j++) {
   if(s[i] != find[j]) break;
  }
  if(j == fl) return s+i;
 }
 return 0;
}
*/

void (*g_line_handler)(); //kek
void (*g_extra_handler)(); //kek

int g_extra_fd;

void irc_handler(struct shit *me,char *line) {
//  fprintf(stderr,"debug: %s\n",line);
  if(!line) {
    //we're EOFd
    //we need to reconnect to the server.
    return;
  }
  if(!strncmp(line,"PING ",5)) {
    //I write back to the me->fd
    fprintf(stderr,"libirc: GOT A PING. SENDING PONG.\n");
    dprintf(me->fd,"PONG %s\r\n",line+5);
    return;//we probably don't need to let the bot know that it pinged. right?
  }
  //I need a way to get the line_handler passed to runem in here.
  g_line_handler(me->fd,line);
  //if(g_extra_handler) g_extra_handler(me->fd);///haxxxxxxxx fuck me.
  //GLOBALS. OFC. THAT IS THE RIGHT WAY TO DO IT. :D :D :D :D :D :D :D :|
}

void alarm_handler(int sig) {
  g_extra_handler(g_extra_fd);//???
  alarm(1);//lol
}

//is this really good enough to work? it doesn't have extra_handler use though.
int runem(int *fds,void (*line_handler)(),void (*extra_handler)()) { //wrap select_everything() so runem() still works
  g_line_handler=line_handler;
  g_extra_handler=extra_handler;
  g_extra_fd=fds[0];
  int i;
  //signal(SIGALRM,alarm_handler);
  //alarm(1);
  //initialization of this can't be in here. this gets ran after a tail is added.
  for(i=0;fds[i] != -1;i++) {
    add_fd(fds[i],irc_handler);
  }
  select_on_everything();
  return 1;
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
 if(!a) exit(54);
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
