#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../irc.h"

#define mywrite(a,b) write(a,b,strlen(b))

int *fds;
char **chans;

void extra_handler(int fd) {
 return;
}

void privmsg_others(int fd,char *msg) {
 int i;
 char tmp[512];
 for(i=0;fds[i] != -1;i++) {
  if(fds[i] != fd) {
   snprintf(tmp,sizeof(tmp)-1,"PRIVMSG %s :%s\r\n",chans[fdtoi(fds[i])],msg);
   write(fds[i],tmp,strlen(tmp));
  }
 }
}

void message_handler(int fd,char *from,struct user *user,char *line) {
 int i;
 char tmp[512];
 if(!strcmp(from,chans[fdtoi(fd)])) {//don't want to be forwarding PMs. :P
  snprintf(tmp,sizeof(tmp)-1,"<%s> %s",user->nick,line);
  privmsg_others(fd,tmp);
 }
}

void line_handler(int fd,char *line) {//this should be built into the libary?
 char *temp;
 char **a;
 char tmp[512];
 struct user *user=malloc(sizeof(struct user));
 printf("line: '%s'\n",line);
 a=line_cutter(fd,line,user);
 if(!user->user && a[0]) {
  if(!strcmp(a[0],"004")) {
   snprintf(tmp,sizeof(tmp)-1,"JOIN %s\r\n",chans[fdtoi(fd)]);
   temp=strchr(chans[fdtoi(fd)],' ');
   if(temp) *temp=0;
   mywrite(fd,tmp);
  }
 }
 if(a[0] && a[1] && a[2]) {
  if(!strcmp(a[0],"PRIVMSG")) {
   message_handler(fd,*a[1]=='#'?a[1]:user->nick,user,a[2]);
  }
 }
 if(a[0] && user->nick && a[1]) {
  if(!strcmp(a[0],"JOIN")) {
   snprintf(tmp,sizeof(tmp)-1,"%cACTION %s has joined %s%c",1,user->nick,a[1]+(*a[1]==':'),1);
   privmsg_others(fd,tmp);
  }
  if(!strcmp(a[0],"PART")) {
   snprintf(tmp,sizeof(tmp)-1,"%cACTION %s has parted %s%c",1,user->nick,a[1]+(*a[1]==':'),1);
   privmsg_others(fd,tmp);
  }
  if(!strcmp(a[0],"QUIT")) {
   snprintf(tmp,sizeof(tmp)-1,"%cACTION %s has quited %s%c",1,user->nick,a[1]+(*a[1]==':'),1);
   privmsg_others(fd,tmp);
  }
 }
 free(user);
}

int fdtoi(int fd) {
 int i;
 for(i=0;fds[i] != -1;i++) {
  if(fds[i] == fd) return i;
 }
 return -1;
}

int main(int argc,char *argv[]) {
 fds=malloc(sizeof(int) * (argc+3) / 3);
 chans=malloc(sizeof(char *) * (argc+3) / 3);
 int i=0;
 printf("%d\n",argc);
 for(i=0;((i*3)+3)<argc;i++) {
  printf("%d server: %s port: %s channel: %s\n",i,argv[(i*3)+1],argv[(i*3)+2],argv[(i*3)+3]);
  fds[i]=serverConnect(argv[(i*3)+1],argv[(i*3)+2]);
  if(fds[i] == -1) return 1;
  chans[i]=strdup(argv[(i*3)+3]);
  mywrite(fds[i],"NICK link8239\r\nUSER a b c :d\r\n");
 }
 fds[i]=-1;
 //heh. you can write your own code for picking a different nick per server. fuck you.
 runem(fds,line_handler,extra_handler);
}
