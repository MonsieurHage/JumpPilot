#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L
#include "JPClient.h"
#include "JPProtocol.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <net/if.h>
#endif

typedef struct { uint8_t bytes[128]; size_t size; int action; } Command;
struct JPClient {
    pthread_t thread;
    pthread_mutex_t mutex;
    int started,stop,connected,armed,speed,turn,halt_pending,critical_battery;
    double input_time,last_jump;
    char host[INET_ADDRSTRLEN]; uint16_t discovery_port;
    Command queue[16]; unsigned qhead,qcount;
    JPCallback callback; void *context;
};
static double now(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
    return (double)ts.tv_sec+(double)ts.tv_nsec/1e9;
}
static void event(JPClient *c,int type,int value,const void *data,size_t n) {
    if(c->callback) c->callback(c->context,type,value,data,n);
}
static void state(JPClient *c,int value,const char *message) {
    event(c,JP_EVENT_STATE,value,message,strlen(message));
}
static int stopped(JPClient *c) {
    pthread_mutex_lock(&c->mutex); int s=c->stop; pthread_mutex_unlock(&c->mutex); return s;
}
static int wait_fd(JPClient *c,int fd,short events,double deadline) {
    while(!stopped(c) && now()<deadline) {
        struct pollfd p={fd,events,0};
        int rc=poll(&p,1,40);
        if(rc<0 && errno!=EINTR) return -1;
        if(rc>0 && (p.revents&events)) return 0;
        if(rc>0 && (p.revents&(POLLERR|POLLHUP|POLLNVAL))) return -1;
    }
    return -1;
}
static void protect_socket(int fd) {
#ifdef SO_NOSIGPIPE
    int one=1; setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof(one));
#else
    (void)fd;
#endif
}
static ssize_t send_safe(int fd,const void *p,size_t n) {
#ifdef MSG_NOSIGNAL
    return send(fd,p,n,MSG_NOSIGNAL);
#else
    return send(fd,p,n,0);
#endif
}
static void wifi_socket(int fd,const char *host) {
#ifdef __APPLE__
    /* iPhone Wi-Fi is en0. Keep all drone traffic on that interface, even without Internet. */
    if(strcmp(host,"127.0.0.1")) {
        unsigned index=if_nametoindex("en0");
        if(index) setsockopt(fd,IPPROTO_IP,IP_BOUND_IF,&index,sizeof(index));
    }
#else
    (void)fd; (void)host;
#endif
}
static int discovery(JPClient *c,int d2c_port,int *c2d,int *fs,int *max) {
    int fd=socket(AF_INET,SOCK_STREAM,0); if(fd<0) return -1;
    protect_socket(fd); wifi_socket(fd,c->host);
    fcntl(fd,F_SETFL,O_NONBLOCK);
    struct sockaddr_in addr={0}; addr.sin_family=AF_INET;
#ifdef __APPLE__
    addr.sin_len=sizeof(addr);
#endif
    addr.sin_port=htons(c->discovery_port); inet_pton(AF_INET,c->host,&addr.sin_addr);
    double deadline=now()+9.0;
    int rc=connect(fd,(struct sockaddr *)&addr,sizeof(addr));
    if(rc<0 && errno!=EINPROGRESS) {close(fd);return -1;}
    if(wait_fd(c,fd,POLLOUT,deadline)) {close(fd);return -1;}
    int error=0; socklen_t size=sizeof(error);
    if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&size)<0 || error) {close(fd);return -1;}
    char request[256];
    int length=snprintf(request,sizeof(request),
        "{\"d2c_port\":%d,\"controller_type\":\"iPhone\",\"controller_name\":\"Jump Pilot\"}",d2c_port);
    size_t sent=0,total=(size_t)length+1; /* Parrot discovery includes the terminating NUL. */
    while(sent<total) {
        if(wait_fd(c,fd,POLLOUT,deadline)) {close(fd);return -1;}
        ssize_t n=send_safe(fd,request+sent,total-sent);
        if(n<0 && (errno==EAGAIN || errno==EINTR)) continue;
        if(n<=0) {close(fd);return -1;}
        sent+=(size_t)n;
    }
    char response[8193]; size_t used=0; int result=-1;
    while(used<sizeof(response)-1 && !stopped(c)) {
        if(wait_fd(c,fd,POLLIN,deadline)) break;
        ssize_t n=recv(fd,response+used,sizeof(response)-1-used,0);
        if(n<0 && (errno==EAGAIN || errno==EINTR)) continue;
        if(n<=0) break;
        used+=(size_t)n;response[used]=0;
        /* Also handles fragmented TCP JSON and firmware that omits NUL. */
        result=jp_parse_discovery(response,used,c2d,fs,max);
        if(result==0 || result==-2) break;
        if(memchr(response,0,used)) break;
    }
    close(fd);return result;
}
static int queue_locked(JPClient *c,uint8_t p,uint8_t cl,uint16_t cmd,
                         const uint8_t *args,size_t n,int action) {
    if(c->qcount==16) return -1;
    Command *q=&c->queue[(c->qhead+c->qcount)%16];
    q->size=jp_command(q->bytes,p,cl,cmd,args,n);q->action=action;c->qcount++;
    return q->size ? 0:-1;
}
static int transmit(int fd,struct sockaddr_in *dest,uint8_t *sequences,
                     uint8_t type,uint8_t buffer,const uint8_t *data,size_t n,
                     uint8_t *save,size_t *save_length) {
    uint8_t packet[256];
    size_t length=jp_encode_frame(packet,sizeof(packet),type,buffer,sequences[buffer]++,data,n);
    if(!length) return -1;
    if(save) {memcpy(save,packet,length);*save_length=length;}
    return sendto(fd,packet,length,0,(struct sockaddr *)dest,sizeof(*dest))==(ssize_t)length ? 0:-1;
}
static void *run(void *arg) {
    JPClient *c=arg; int fd=-1,fs=65000,max=4,c2d=0,failed=0;
    JPVideo video={0}; uint8_t sequences[256]={0},last_sequence[256]={0},seen[256]={0};
    uint8_t pending[256]={0}; size_t pending_size=0;
    int pending_action=0,retries=0,active=0;
    double pending_time=0;
    state(c,JP_CONNECTING,"Connexion au drone…");
    fd=socket(AF_INET,SOCK_DGRAM,0);
    if(fd<0) {failed=1;goto done;}
    wifi_socket(fd,c->host);
    int receive_size=2*1024*1024;
    setsockopt(fd,SOL_SOCKET,SO_RCVBUF,&receive_size,sizeof(receive_size));
    struct sockaddr_in local={0}; local.sin_family=AF_INET;local.sin_addr.s_addr=htonl(INADDR_ANY);
#ifdef __APPLE__
    local.sin_len=sizeof(local);
#endif
    if(bind(fd,(struct sockaddr *)&local,sizeof(local))<0) {failed=1;goto done;}
    socklen_t local_size=sizeof(local);
    if(getsockname(fd,(struct sockaddr *)&local,&local_size)<0) {failed=1;goto done;}
    int response=discovery(c,ntohs(local.sin_port),&c2d,&fs,&max);
    if(stopped(c)) goto done;
    if(response) {
        state(c,JP_FAILED,response==-2 ? "Connexion refusée. Fermez FreeFlight puis réessayez." :
            "Drone inaccessible. Vérifiez son Wi-Fi et l’autorisation Réseau local, puis réessayez.");
        failed=2;goto done;
    }
    if(jp_video_init(&video,(size_t)fs,(unsigned)max)) {failed=1;goto done;}
    fcntl(fd,F_SETFL,O_NONBLOCK);
    struct sockaddr_in dest={0};dest.sin_family=AF_INET;dest.sin_port=htons((uint16_t)c2d);
#ifdef __APPLE__
    dest.sin_len=sizeof(dest);
#endif
    inet_pton(AF_INET,c->host,&dest.sin_addr);
    pthread_mutex_lock(&c->mutex);
    c->connected=0;c->armed=0;c->speed=0;c->turn=0;c->last_jump=0;c->halt_pending=0;c->critical_battery=0;
    c->qhead=0;c->qcount=0;
    queue_locked(c,0,2,0,NULL,0,0); /* Common.Settings.AllSettings */
    queue_locked(c,0,4,0,NULL,0,0); /* Common.Common.AllStates */
    uint8_t enabled=1;
    queue_locked(c,3,18,0,&enabled,1,0); /* jpsumo.MediaStreaming.VideoEnable */
    pthread_mutex_unlock(&c->mutex);
    double last_receive=now(),last_pcmd=0;
    uint8_t packet[65536];
    while(!stopped(c)) {
        double t=now();int speed=0,turn=0,halt=0;
        pthread_mutex_lock(&c->mutex);
        if(c->armed && t-c->input_time<=0.350) {speed=c->speed;turn=c->turn;}
        halt=c->halt_pending;c->halt_pending=0;
        pthread_mutex_unlock(&c->mutex);
        if(halt && pending_action) pending_size=0; /* No retry of a disarmed jump. */
        if(t-last_pcmd>=0.05 || halt) {
            uint8_t pcmd[7];jp_pcmd(pcmd,speed,turn);
            transmit(fd,&dest,sequences,2,10,pcmd,7,NULL,NULL);last_pcmd=t;
        }
        if(!pending_size) {
            Command command;int have=0;
            pthread_mutex_lock(&c->mutex);
            if(c->qcount) {
                command=c->queue[c->qhead];c->qhead=(c->qhead+1)%16;c->qcount--;have=1;
                if(command.action && !c->armed) have=0;
            }
            pthread_mutex_unlock(&c->mutex);
            if(have) {
                transmit(fd,&dest,sequences,4,11,command.bytes,command.size,pending,&pending_size);
                pending_action=command.action;pending_time=t;retries=0;
            }
        } else if(t-pending_time>=0.150) {
            if(retries++>=5) {
                if(pending_action) event(c,JP_EVENT_MESSAGE,0,"Saut non confirmé par le drone.",strlen("Saut non confirmé par le drone."));
                pending_size=0;
            } else {
                sendto(fd,pending,pending_size,0,(struct sockaddr *)&dest,sizeof(dest));pending_time=t;
            }
        }
        struct pollfd poller={fd,POLLIN,0};int ready=poll(&poller,1,10);
        if(ready<0 && errno!=EINTR) {failed=1;break;}
        if(ready>0 && (poller.revents&(POLLERR|POLLNVAL))) {failed=1;break;}
        if(ready>0 && (poller.revents&POLLIN)) {
            /* Bound each receive batch so input, watchdog and stop are never starved. */
            for(int batch=0;batch<32;batch++) {
                struct sockaddr_in from={0};socklen_t from_size=sizeof(from);
                ssize_t n=recvfrom(fd,packet,sizeof(packet),0,(struct sockaddr *)&from,&from_size);
                if(n<=0) break;
                if(from.sin_addr.s_addr!=dest.sin_addr.s_addr) continue;
                size_t offset=0;JPFrame frame;
                /* Validate the whole packet before accepting even its first frame. */
                int valid;while((valid=jp_next_frame(packet,(size_t)n,&offset,&frame))==1) {}
                if(valid<0) continue;
                offset=0;
                while(jp_next_frame(packet,(size_t)n,&offset,&frame)==1) {
                    last_receive=now();
                    if(!active) {
                        active=1;pthread_mutex_lock(&c->mutex);c->connected=1;pthread_mutex_unlock(&c->mutex);
                        state(c,JP_CONNECTED,"Connecté · commandes désactivées");
                    }
                    if(frame.type==4 && frame.buffer<128) {
                        uint8_t seq=frame.sequence;
                        transmit(fd,&dest,sequences,1,(uint8_t)(frame.buffer+128),&seq,1,NULL,NULL);
                    }
                    if(frame.type==1) {
                        if(frame.buffer==139 && frame.size==1 && pending_size && frame.data[0]==pending[2]) pending_size=0;
                        continue;
                    }
                    if(frame.buffer==0) {
                        transmit(fd,&dest,sequences,2,1,frame.data,frame.size,NULL,NULL);continue;
                    }
                    /* ARStream fragment order is separate from ARNetwork sequence order. */
                    if(frame.buffer==125) {
                        if(jp_video_feed(&video,frame.data,frame.size)==1)
                            event(c,JP_EVENT_VIDEO,0,video.bytes,video.length);
                        continue;
                    }
                    if(frame.buffer!=126 && frame.buffer!=127) continue;
                    uint8_t b=frame.buffer,s=frame.sequence;
                    if(seen[b]) {
                        unsigned backward=(uint8_t)(last_sequence[b]-s);
                        if(backward==0 || (backward<=10)) continue;
                    }
                    seen[b]=1;last_sequence[b]=s;
                    const uint8_t *d=frame.data;size_t length=frame.size;
                    if(length>=5 && d[0]==0 && d[1]==5 && jp_read16(d+2)==1 && d[4]<=100)
                        event(c,JP_EVENT_BATTERY,d[4],NULL,0);
                    if(length>=8 && d[0]==3 && d[1]==1 && jp_read16(d+2)==1) {
                        int alert=(int)jp_read32(d+4);
                        pthread_mutex_lock(&c->mutex);c->critical_battery=(alert==1);pthread_mutex_unlock(&c->mutex);
                        event(c,JP_EVENT_ALERT,alert,NULL,0);
                        if(alert==1) jp_client_halt(c);
                    }
                    if(length>=8 && d[0]==3 && d[1]==3 && jp_read16(d+2)==2 && jp_read32(d+4))
                        event(c,JP_EVENT_MESSAGE,0,"Mécanisme de saut bloqué ou trop chaud.",strlen("Mécanisme de saut bloqué ou trop chaud."));
                }
            }
        }
        if(now()-last_receive>3.0) {
            state(c,JP_FAILED,"Liaison perdue. Commandes désactivées ; reconnectez-vous.");failed=2;break;
        }
    }
    /* Best effort wheel stop + cancel, with new sequences. No queued jump survives. */
    { uint8_t zero[7];jp_pcmd(zero,0,0);
      for(int i=0;i<3;i++) transmit(fd,&dest,sequences,2,10,zero,7,NULL,NULL);
      uint8_t cancel[4];jp_command(cancel,3,2,1,NULL,0);
      transmit(fd,&dest,sequences,4,11,cancel,4,NULL,NULL);
      uint8_t off=0,disable[5];jp_command(disable,3,18,0,&off,1);
      transmit(fd,&dest,sequences,4,11,disable,5,NULL,NULL);
    }
done:
    if(fd>=0) close(fd);
    jp_video_free(&video);
    pthread_mutex_lock(&c->mutex);
    c->connected=0;c->armed=0;c->speed=0;c->turn=0;c->qhead=0;c->qcount=0;
    pthread_mutex_unlock(&c->mutex);
    if(failed==1) state(c,JP_FAILED,"Erreur réseau. Vérifiez le Wi-Fi du drone puis réessayez.");
    else if(!failed) state(c,JP_OFFLINE,"Déconnecté");
    return NULL;
}
JPClient *jp_client_create(JPCallback callback,void *context) {
    JPClient *c=calloc(1,sizeof(*c));if(!c)return NULL;
    if(pthread_mutex_init(&c->mutex,NULL)) {free(c);return NULL;}
    c->callback=callback;c->context=context;return c;
}
void jp_client_disconnect(JPClient *c) {
    if(!c)return;
    pthread_mutex_lock(&c->mutex);c->stop=1;c->armed=0;c->speed=0;c->turn=0;pthread_mutex_unlock(&c->mutex);
    if(c->started) {pthread_join(c->thread,NULL);c->started=0;}
}
void jp_client_destroy(JPClient *c) {
    if(!c)return;
    jp_client_disconnect(c);pthread_mutex_destroy(&c->mutex);free(c);
}
int jp_client_connect(JPClient *c,const char *host,uint16_t port) {
    if(!c || !host || !port) return -1;
    struct in_addr address;if(inet_pton(AF_INET,host,&address)!=1)return -1;
    jp_client_disconnect(c);
    pthread_mutex_lock(&c->mutex);
    snprintf(c->host,sizeof(c->host),"%s",host);c->discovery_port=port;
    c->stop=0;c->armed=0;c->connected=0;c->halt_pending=0;c->qhead=0;c->qcount=0;
    pthread_mutex_unlock(&c->mutex);
    if(pthread_create(&c->thread,NULL,run,c))return -1;
    c->started=1;return 0;
}
void jp_client_arm(JPClient *c,int enabled) {
    if(!c)return;
    if(!enabled) {jp_client_halt(c);return;}
    pthread_mutex_lock(&c->mutex);c->armed=c->connected && !c->critical_battery;c->speed=0;c->turn=0;c->input_time=now();pthread_mutex_unlock(&c->mutex);
}
void jp_client_drive(JPClient *c,int speed,int turn) {
    if(!c)return;
    pthread_mutex_lock(&c->mutex);
    if(c->armed && c->connected) {c->speed=speed;c->turn=turn;c->input_time=now();}
    pthread_mutex_unlock(&c->mutex);
}
void jp_client_halt(JPClient *c) {
    if(!c)return;
    pthread_mutex_lock(&c->mutex);
    c->armed=0;c->speed=0;c->turn=0;c->halt_pending=1;
    /* Keep initialization commands; purge only unsent movement actions. */
    Command kept[16];unsigned count=0;
    for(unsigned i=0;i<c->qcount;i++) {
        Command q=c->queue[(c->qhead+i)%16];if(!q.action)kept[count++]=q;
    }
    memcpy(c->queue,kept,count*sizeof(Command));c->qhead=0;c->qcount=count;
    if(c->connected) {queue_locked(c,3,2,1,NULL,0,0);uint8_t zero[4]={0};queue_locked(c,3,2,4,zero,4,0);}
    pthread_mutex_unlock(&c->mutex);
}
int jp_client_jump(JPClient *c,int high) {
    if(!c || (high!=0 && high!=1))return -1;
    pthread_mutex_lock(&c->mutex);
    int result=-1;double t=now();
    if(c->connected && c->armed && t-c->last_jump>=3.0) {
        uint8_t arg[4];jp_write32(arg,(uint32_t)high);
        result=queue_locked(c,3,2,3,arg,4,1);
        if(!result) {c->last_jump=t;c->speed=0;c->turn=0;c->input_time=t;}
    }
    pthread_mutex_unlock(&c->mutex);return result;
}
