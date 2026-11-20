#include "JPProtocol.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

uint16_t jp_read16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
uint32_t jp_read32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
void jp_write16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
void jp_write32(uint8_t *p, uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
size_t jp_encode_frame(uint8_t *out, size_t cap, uint8_t type, uint8_t buffer,
                       uint8_t seq, const uint8_t *data, size_t n) {
    if (!out || n > UINT32_MAX-7 || cap < 7 || n > cap-7 || (n && !data)) return 0;
    out[0]=type; out[1]=buffer; out[2]=seq; jp_write32(out+3,(uint32_t)n+7);
    if (n) memcpy(out+7,data,n);
    return n+7;
}
int jp_next_frame(const uint8_t *p, size_t n, size_t *offset, JPFrame *f) {
    if (!p || !offset || !f || *offset>n) return -1;
    if (*offset==n) return 0;
    if (n-*offset<7) return -1;
    const uint8_t *h=p+*offset;
    size_t length=jp_read32(h+3);
    if (h[0]<1 || h[0]>4 || length<7 || length>n-*offset) return -1;
    f->type=h[0]; f->buffer=h[1]; f->sequence=h[2]; f->data=h+7; f->size=length-7;
    *offset+=length;
    return 1;
}
size_t jp_command(uint8_t *out, uint8_t p, uint8_t c, uint16_t cmd,
                  const uint8_t *args, size_t n) {
    if (!out || n>124 || (n && !args)) return 0;
    out[0]=p; out[1]=c; jp_write16(out+2,cmd);
    if (n) memcpy(out+4,args,n);
    return n+4;
}
static int clamp(int v) { return v>100 ? 100 : v< -100 ? -100 : v; }
void jp_pcmd(uint8_t out[7], int speed, int turn) {
    out[0]=3; out[1]=0; out[2]=0; out[3]=0;
    out[4]=(speed || turn) ? 1 : 0;
    out[5]=(uint8_t)(int8_t)clamp(speed); out[6]=(uint8_t)(int8_t)clamp(turn);
}
int jp_video_init(JPVideo *v, size_t fs, unsigned max) {
    if (!v || fs<1 || fs>65000 || max<1 || max>128 || fs*max>8*1024*1024) return -1;
    memset(v,0,sizeof(*v));
    v->bytes=malloc(fs*max);
    if (!v->bytes) return -1;
    v->fragment_size=fs; v->capacity=fs*max;
    return 0;
}
void jp_video_free(JPVideo *v) {
    if (!v) return;
    free(v->bytes); memset(v,0,sizeof(*v));
}
int jp_video_feed(JPVideo *v, const uint8_t *p, size_t n) {
    if (!v || !v->bytes || !p || n<6) return -1;
    uint16_t number=jp_read16(p);
    unsigned idx=p[3], count=p[4];
    size_t payload=n-5;
    if (!count || count>128 || idx>=count || (size_t)count>v->capacity/v->fragment_size ||
        payload>v->fragment_size || (idx+1<count && payload!=v->fragment_size)) return -1;
    if (!v->active || number!=v->frame_number) {
        /* Modular comparison discards delayed fragments, including across wraparound. */
        if (v->active && (uint16_t)(number-v->frame_number)>=32768) return 0;
        memset(v->received,0,sizeof(v->received));
        v->frame_number=number; v->count=(uint8_t)count;
        v->active=1; v->length=0; v->delivered=0;
    }
    if (v->count!=count) return -1;
    if (v->delivered || v->received[idx]) return 0;
    memcpy(v->bytes+(size_t)idx*v->fragment_size,p+5,payload);
    v->received[idx]=1;
    if (idx+1==count) v->length=(size_t)idx*v->fragment_size+payload;
    for (unsigned i=0;i<count;i++) if (!v->received[i]) return 0;
    v->delivered=1;
    if (v->length<4 || v->bytes[0]!=0xff || v->bytes[1]!=0xd8 ||
        v->bytes[v->length-2]!=0xff || v->bytes[v->length-1]!=0xd9) return -1;
    return 1;
}

/* Bounded JSON reader. Discovery uses a flat object; unknown nested values are skipped. */
typedef struct { const char *p,*end; int depth; } Reader;
static void ws(Reader *r) { while(r->p<r->end && isspace((unsigned char)*r->p)) r->p++; }
static int string(Reader *r, char *out, size_t cap) {
    if (r->p==r->end || *r->p++!='"') return -1;
    size_t n=0;
    while(r->p<r->end) {
        unsigned char c=(unsigned char)*r->p++;
        if(c=='"') { if(out && cap) out[n<cap ? n:cap-1]=0; return 0; }
        if(c<32) return -1;
        if(c=='\\') {
            if(r->p==r->end) return -1;
            c=(unsigned char)*r->p++;
            if(c=='u') { for(int i=0;i<4;i++) { if(r->p==r->end || !isxdigit((unsigned char)*r->p++)) return -1; } c='?'; }
            else if(!strchr("\"\\/bfnrt",c)) return -1;
        }
        if(out && n+1<cap) out[n]=(char)c;
        n++;
    }
    return -1;
}
static int integer(Reader *r,int *out) {
    ws(r); int neg=0; int64_t v=0;
    if(r->p<r->end && *r->p=='-') {neg=1;r->p++;}
    if(r->p==r->end || !isdigit((unsigned char)*r->p)) return -1;
    int first=*r->p;
    int digits=0;
    while(r->p<r->end && isdigit((unsigned char)*r->p)) {
        if(++digits>10) return -1;
        v=v*10+(*r->p++-'0');
        if(v>2147483648LL) return -1;
    }
    if(first=='0' && digits>1) return -1;
    if(!neg && v>INT_MAX) return -1;
    *out=(int)(neg ? -v:v); return 0;
}
static int skip(Reader *r) {
    ws(r); if(r->p==r->end || r->depth>8) return -1;
    if(*r->p=='"') return string(r,NULL,0);
    if(*r->p=='{' || *r->p=='[') {
        char close=(*r->p++=='{')?'}':']'; r->depth++;
        int object=close=='}'; ws(r);
        if(r->p<r->end && *r->p==close) {r->p++;r->depth--;return 0;}
        while(r->p<r->end) {
            if(object) { if(string(r,NULL,0)) return -1;ws(r);if(r->p==r->end || *r->p++!=':') return -1; }
            if(skip(r)) return -1;
            ws(r); if(r->p==r->end) return -1;
            char c=*r->p++; if(c==close) {r->depth--;return 0;}
            if(c!=',') return -1;
            ws(r);
        }
        return -1;
    }
    const char *start=r->p;
    while(r->p<r->end && !strchr(",}] \r\n\t",*r->p)) r->p++;
    size_t n=(size_t)(r->p-start);
    if((n==4 && (!memcmp(start,"true",4) || !memcmp(start,"null",4))) ||
       (n==5 && !memcmp(start,"false",5))) return 0;
    /* Unknown numeric metadata can be fractional, but must be a valid JSON number. */
    Reader number={start,r->p,0}; int ignored;
    if(integer(&number,&ignored)) return -1;
    if(number.p<number.end && *number.p=='.') {
        number.p++; const char *s=number.p;
        while(number.p<number.end && isdigit((unsigned char)*number.p)) number.p++;
        if(s==number.p) return -1;
    }
    if(number.p<number.end && (*number.p=='e' || *number.p=='E')) {
        number.p++; if(number.p<number.end && (*number.p=='+' || *number.p=='-')) number.p++;
        const char *s=number.p;
        while(number.p<number.end && isdigit((unsigned char)*number.p)) number.p++;
        if(s==number.p) return -1;
    }
    return number.p==number.end ? 0:-1;
}
int jp_parse_discovery(const char *json,size_t n,int *port,int *fs,int *max) {
    if(!json || !port || !fs || !max || !n || n>8192) return -1;
    while(n && json[n-1]==0) n--;
    Reader r={json,json+n,0}; ws(&r);
    if(r.p==r.end || *r.p++!='{') return -1;
    int status=-1,p=0,f=65000,m=4; unsigned found=0;
    ws(&r);
    if(r.p==r.end || *r.p=='}') return -1;
    for(;;) {
        char key[96]; if(string(&r,key,sizeof(key))) return -1;
        ws(&r); if(r.p==r.end || *r.p++!=':') return -1;
        int *dest=NULL; unsigned bit=0;
        if(!strcmp(key,"status")){dest=&status;bit=1;}
        if(!strcmp(key,"c2d_port")){dest=&p;bit=2;}
        if(!strcmp(key,"arstream_fragment_size")){dest=&f;bit=4;}
        if(!strcmp(key,"arstream_fragment_maximum_number")){dest=&m;bit=8;}
        if(dest) {if(found&bit || integer(&r,dest)) return -1;found|=bit;}
        else if(skip(&r)) return -1;
        ws(&r); if(r.p==r.end) return -1;
        char c=*r.p++; if(c=='}') break;
        if(c!=',') return -1;
        ws(&r);
    }
    ws(&r); if(r.p!=r.end || (found&3)!=3) return -1;
    if(status!=0) return -2;
    if(p<1 || p>65535 || f<1 || f>65000 || m<1 || m>128 || (int64_t)f*m>8*1024*1024) return -1;
    *port=p;*fs=f;*max=m;return 0;
}
