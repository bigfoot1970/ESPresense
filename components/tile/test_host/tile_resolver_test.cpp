// Host test: g++ -O1 -I.. tile_resolver_test.cpp ../TileResolver.cpp -lcrypto -o t && ./t
#include "TileResolver.h"
#include <openssl/hmac.h>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <chrono>
using namespace TileResolver;
static int calls=0;
void H(const uint8_t s[16], uint32_t c, uint8_t o[8]){calls++;uint8_t m[32]={0};memcpy(m,&c,4);unsigned l;uint8_t d[32];HMAC(EVP_sha256(),s,16,m,32,d,&l);memcpy(o,d,8);}
void sd(const char* seedhex,uint32_t ctr,uint8_t*o){uint8_t s[16];for(int i=0;i<16;i++)sscanf(seedhex+2*i,"%2hhx",&s[i]);o[0]=2;o[1]=0;H(s,ctr,o+2);}
int main(){
 SetHmac(H);
 assert(Configure("dog:000102030405060708090a0b0c0d0e0f bad:zz cat:f0e1d2c3b4a5968778695a4b3c2d1e0f")==2);
 const char* W="000102030405060708090a0b0c0d0e0f";
 uint8_t a[10]; sd(W,2858,a);
 char hx[17];for(int i=0;i<8;i++)sprintf(hx+2*i,"%02x",a[2+i]); printf("pid %s\n",hx); assert(!strcmp(hx,"e972aca33d9ed300"));
 auto r=Resolve(a,10,1000); assert(r&&!strcmp(r,"dog")); printf("full search ok calls=%d\n",calls);
 calls=0; r=Resolve(a,10,2000); assert(r&&!strcmp(r,"dog")); printf("repeat calls=%d\n",calls);
 // 45 min later ctr+3
 sd(W,2861,a); calls=0; r=Resolve(a,10,1000+45*60000); assert(r&&!strcmp(r,"dog")); printf("drift ok calls=%d ctr=%d\n",calls,LastCounter(0));
 // 3 days later (288 steps) predicted
 sd(W,2861+288,a); calls=0; r=Resolve(a,10,1000+45*60000+288u*15*60000); assert(r); printf("3 days ok calls=%d\n",calls);
 // wrap
 // foreign tile pid: cat not synced -> full search rate limited
 uint8_t f[10]={2,0,1,2,3,4,5,6,7,8}; uint32_t t=1000+45*60000+288u*15*60000+5000;
 calls=0; r=Resolve(f,10,t); assert(!r); printf("foreign (cat unsynced) calls=%d\n",calls);
 calls=0; r=Resolve(f,10,t+1000); assert(!r); printf("foreign again (neg cache) calls=%d\n",calls);
 uint8_t g[10]={2,0,9,9,9,9,9,9,9,9}; calls=0; r=Resolve(g,10,t+2000); assert(!r); printf("other foreign within 60s calls=%d\n",calls);
 // cat appears
 const char* S="f0e1d2c3b4a5968778695a4b3c2d1e0f"; uint8_t s2[10]; sd(S,8640,s2); calls=0; r=Resolve(s2,10,t+70000); assert(r&&!strcmp(r,"cat")); printf("cat ctr 8640 ok\n");
 sd(S,0,s2); r=Resolve(s2,10,t+70000+15*60000); assert(r&&!strcmp(r,"cat")); printf("wrap ok ctr=%d\n",LastCounter(1));
 // all synced: foreign -> no full search
 uint8_t h[10]={2,0,7,7,7,7,7,7,7,7}; calls=0; r=Resolve(h,10,t+80000+15*60000); assert(!r); printf("foreign all-synced calls=%d\n",calls);
 // timing full search
 Configure(std::string("dog:")+W); sd(W,8000,a); auto t0=std::chrono::steady_clock::now(); Resolve(a,10,1); printf("host full %lld us\n",(long long)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-t0).count());
 puts("ALL OK");}
