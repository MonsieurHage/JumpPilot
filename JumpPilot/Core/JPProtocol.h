#ifndef JP_PROTOCOL_H
#define JP_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t type, buffer, sequence;
    const uint8_t *data;
    size_t size;
} JPFrame;

uint16_t jp_read16(const uint8_t *p);
uint32_t jp_read32(const uint8_t *p);
void jp_write16(uint8_t *p, uint16_t v);
void jp_write32(uint8_t *p, uint32_t v);
size_t jp_encode_frame(uint8_t *out, size_t capacity, uint8_t type,
                       uint8_t buffer, uint8_t sequence,
                       const uint8_t *data, size_t size);
/* 1: frame, 0: end, -1: invalid. No unaligned memory accesses. */
int jp_next_frame(const uint8_t *packet, size_t size, size_t *offset, JPFrame *frame);
size_t jp_command(uint8_t *out, uint8_t project, uint8_t class_id,
                  uint16_t command, const uint8_t *args, size_t size);
void jp_pcmd(uint8_t out[7], int speed, int turn);

typedef struct {
    uint8_t *bytes;
    size_t capacity, fragment_size, length;
    uint16_t frame_number;
    uint8_t count, received[128];
    int active, delivered;
} JPVideo;
int jp_video_init(JPVideo *video, size_t fragment_size, unsigned maximum_fragments);
void jp_video_free(JPVideo *video);
/* 1: complete JPEG; 0: incomplete/duplicate; -1: invalid fragment. */
int jp_video_feed(JPVideo *video, const uint8_t *data, size_t size);
/* Discovery response parser; accepts a complete JSON object, rejects missing fields. */
int jp_parse_discovery(const char *json, size_t length, int *port,
                        int *fragment_size, int *maximum_fragments);
#endif
