#include "JPProtocol.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void framing(void) {
    uint8_t p[128],a[]={0,5,1,0,67};
    size_t n=jp_encode_frame(p,sizeof(p),4,126,255,a,sizeof(a));
    assert(n==12 && p[3]==12 && p[4]==0 && p[5]==0 && p[6]==0);
    JPFrame f;size_t o=0;
    assert(jp_next_frame(p,n,&o,&f)==1 && f.buffer==126 && f.sequence==255 && f.size==5);
    assert(!memcmp(f.data,a,5) && jp_next_frame(p,n,&o,&f)==0);
    n+=jp_encode_frame(p+n,sizeof(p)-n,2,0,3,a,5);o=0;
    assert(jp_next_frame(p,n,&o,&f)==1 && jp_next_frame(p,n,&o,&f)==1);
    assert(f.buffer==0 && o==n);
    for(size_t i=1;i<12;i++) {o=0;assert(jp_next_frame(p,i,&o,&f)==-1);}
    p[3]=6;o=0;assert(jp_next_frame(p,n,&o,&f)==-1);
    assert(jp_encode_frame(p,6,2,10,0,a,5)==0);
    uint8_t cmd[7];jp_pcmd(cmd,-120,300);
    const uint8_t expected[]={3,0,0,0,1,156,100};assert(!memcmp(cmd,expected,7));
    jp_pcmd(cmd,0,0);assert(cmd[4]==0 && cmd[5]==0 && cmd[6]==0);
    puts("OK: framing, little endian, combined frames, truncation, signed piloting");
}
static void discovery(void) {
    const char *good="{\"status\":0,\"c2d_port\":54321,\"arstream_fragment_size\":65000,\"arstream_fragment_maximum_number\":4,\"extra\":{\"x\":[true,3.5,\"y\"]}}";
    int p,f,m;assert(!jp_parse_discovery(good,strlen(good),&p,&f,&m));
    assert(p==54321 && f==65000 && m==4);
    for(size_t n=0;n<strlen(good);n++) assert(jp_parse_discovery(good,n,&p,&f,&m)==-1);
    const char *bad[]={"{}","{\"status\":0}","{\"status\":0,\"c2d_port\":0}",
        "{\"status\":0,\"c2d_port\":70000}","{\"status\":0,\"c2d_port\":01}",
        "{\"status\":0,\"c2d_port\":54321,\"status\":0}",
        "{\"status\":0,\"c2d_port\":54321,\"arstream_fragment_size\":2147483648}",
        "{\"status\":0,\"c2d_port\":54321,\"arstream_fragment_maximum_number\":999}",
        "{\"status\":0,\"c2d_port\":54321}garbage",
        "{\"status\":0,\"c2d_port\":54321,\"extra\":untrue}"};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) assert(jp_parse_discovery(bad[i],strlen(bad[i]),&p,&f,&m)==-1);
    const char *refused="{\"status\":1,\"c2d_port\":0}";
    assert(jp_parse_discovery(refused,strlen(refused),&p,&f,&m)==-2);
    puts("OK: discovery JSON, fragmented responses, invalid fields, refusal");
}
static void video(void) {
    JPVideo v;assert(jp_video_init(&v,8,4)==0);
    uint8_t first[]={255,255,1,0,2,0xff,0xd8,1,2,3,4,5,6};
    uint8_t second[]={255,255,1,1,2,7,8,0xff,0xd9};
    assert(jp_video_feed(&v,second,sizeof(second))==0);
    assert(jp_video_feed(&v,second,sizeof(second))==0);
    assert(jp_video_feed(&v,first,sizeof(first))==1 && v.length==12);
    assert(v.bytes[8]==7 && v.bytes[11]==0xd9);
    assert(jp_video_feed(&v,first,sizeof(first))==0);
    first[0]=0;first[1]=0;second[0]=0;second[1]=0;
    assert(jp_video_feed(&v,first,sizeof(first))==0); /* 65535 -> 0 wrap */
    assert(jp_video_feed(&v,second,sizeof(second))==1);
    first[0]=255;first[1]=255;assert(jp_video_feed(&v,first,sizeof(first))==0);
    first[0]=1;first[1]=0;first[3]=5;assert(jp_video_feed(&v,first,sizeof(first))==-1);
    jp_video_free(&v);
    assert(jp_video_init(&v,65000,129)==-1);
    puts("OK: video reordering, duplicates, delayed fragments, frame wrap, bounds");
}
static unsigned rng=0x12345678;
static unsigned random32(void) {rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void malformed(void) {
    JPVideo v;assert(!jp_video_init(&v,64,8));uint8_t bytes[1024];
    for(int i=0;i<50000;i++) {
        size_t n=random32()%sizeof(bytes);
        for(size_t j=0;j<n;j++)bytes[j]=(uint8_t)random32();
        JPFrame f;size_t offset=0;
        while(jp_next_frame(bytes,n,&offset,&f)==1) {}
        jp_video_feed(&v,bytes,n);
        int p,fs,max;jp_parse_discovery((char *)bytes,n,&p,&fs,&max);
    }
    jp_video_free(&v);puts("OK: 50,000 malformed packet / video / JSON inputs");
}
int main(void) {framing();discovery();video();malformed();return 0;}
