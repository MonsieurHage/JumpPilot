#ifndef JP_CLIENT_H
#define JP_CLIENT_H
#include <stddef.h>
#include <stdint.h>

typedef struct JPClient JPClient;
enum { JP_EVENT_STATE=1, JP_EVENT_BATTERY=2, JP_EVENT_VIDEO=3,
       JP_EVENT_MESSAGE=4, JP_EVENT_ALERT=5 };
enum { JP_OFFLINE=0, JP_CONNECTING=1, JP_CONNECTED=2, JP_FAILED=3 };
/* bytes are valid only during callback. Callbacks execute on the network thread.
   Callbacks must not call disconnect/destroy (which join that thread). */
typedef void (*JPCallback)(void *context,int event,int value,const uint8_t *bytes,size_t length);
JPClient *jp_client_create(JPCallback callback,void *context);
void jp_client_destroy(JPClient *client);
/* Connect/disconnect/destroy are called serially by the owner. IPv4 literal only. */
int jp_client_connect(JPClient *client,const char *address,uint16_t discovery_port);
void jp_client_disconnect(JPClient *client);
/* Thread-safe input API. Movement expires 350 ms after the last drive call. */
void jp_client_arm(JPClient *client,int enabled);
void jp_client_drive(JPClient *client,int speed,int turn);
/* Clears unsent actions, zeros wheels, disarms, asks the drone to cancel its jump. */
void jp_client_halt(JPClient *client);
/* 0=long, 1=high. Returns 0 if queued, -1 if not connected/armed/cooling down. */
int jp_client_jump(JPClient *client,int high);
#endif
