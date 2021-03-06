//modified to work better with URC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <irc.h>

#define mywrite(a,b) write(a,b,strlen(b))

int *fds;
char **chans;

void extra_handler(int fd) {
 return;
}

void sendto_others(int fd,char *action,char *msg,struct user *user) {
 int i;
 char tmp[512];
 for(i=0;fds[i] != -1;i++) {
  if(fds[i] != fd) {
   if(chans[fdtoi(fds[i])][0]=='u') {
    snprintf(tmp,sizeof(tmp)-1,"NICK %s\r\n",user->nick);
    write(fds[i],tmp,strlen(tmp));
    snprintf(tmp,sizeof(tmp)-1,"%s %s :%s\r\n",action,chans[fdtoi(fds[i])]+1,msg);
   } else {
    snprintf(tmp,sizeof(tmp)-1,"%s %s :<%s> %s\r\n",action,chans[fdtoi(fds[i])],user->nick,msg);
   }
   printf("writing: %s\n",tmp);
   write(fds[i],tmp,strlen(tmp));
  }
 }
}

void message_handler(int fd,char *from,struct user *user,char *line) {
 char tmp[512];
 printf("message_handler: line: '%s'\n",line);
 if(!strcmp(from,chans[fdtoi(fd)][0]=='u'?chans[fdtoi(fd)]+1:chans[fdtoi(fd)])) {//don't want to be forwarding PMs. :P
  if(line[0] == '\x01' && strlen(line) > 9 && !strncmp(line+1,"ACTION ",7) && line[strlen(line)-1] == '\x01') {
   snprintf(tmp,sizeof(tmp)-1,"%cACTION %s %s",1,user->nick,line+8);
  } else {
   snprintf(tmp,sizeof(tmp)-1,"%s",line);
  }
  sendto_others(fd,"PRIVMSG",tmp,user);
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
  if(!strcmp(a[0],"004") || !strcmp(a[0],"001") || !strcmp(a[0],"376")) {
   snprintf(tmp,sizeof(tmp)-1,"JOIN %s\r\n",chans[fdtoi(fd)][0]=='u'?chans[fdtoi(fd)]+1:chans[fdtoi(fd)]);
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
   snprintf(tmp,sizeof(tmp)-1,"%s has joined %s",user->nick,a[1]+(*a[1]==':'));
   sendto_others(fd,"NOTICE",tmp,user);
  }
  if(!strcmp(a[0],"PART")) {
   snprintf(tmp,sizeof(tmp)-1,"%s has parted %s",user->nick,a[1]+(*a[1]==':'));
   sendto_others(fd,"NOTICE",tmp,user);
  }
  if(!strcmp(a[0],"QUIT")) {
   snprintf(tmp,sizeof(tmp)-1,"%s has quited %s",user->nick,a[1]+(*a[1]==':'));
   sendto_others(fd,"NOTICE",tmp,user);
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
 char tmp[512];
 printf("%d\n",argc);
 for(i=0;((i*3)+3)<argc;i++) {
  printf("%d server: %s port: %s channel: %s\n",i,argv[(i*3)+1],argv[(i*3)+2],argv[(i*3)+3]);
  fds[i]=serverConnect(argv[(i*3)+1],argv[(i*3)+2]);
  if(fds[i] == -1) return 1;
  chans[i]=strdup(argv[(i*3)+3]);
  snprintf(tmp,sizeof(tmp)-1,"NICK %s\r\nUSER a b c :d\r\n",getenv("NICK"));
  write(fds[i],tmp,strlen(tmp));
 }
 fds[i]=-1;
 //heh. you can write your own code for picking a different nick per server. fuck you.
 runem(fds,line_handler,extra_handler);
}
