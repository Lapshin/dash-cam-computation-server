#ifndef DASH_CAM_H
#define DASH_CAM_H

#include <stdint.h>

#define HEADER_MAGIC 0xDEADBEEF
#define HEADER_VERSION 1

struct messageHeader_s
{
    uint32_t magic;           // magic for identifying header
    uint32_t version;         // version of header. Useful if header format changed
    uint32_t size;            // size of whole transmitted data
    uint32_t frame_size;      // size of one payload chunk
    uint32_t crc32; // TODO   // hash calculated for header + payload
    uint8_t  reserved[32];    // zeroed. Useful for future versions
} __attribute__ ((__packed__));
typedef struct messageHeader_s messageHeader_t;

struct message_s
{
    messageHeader_t header;
    uint8_t         payload[];
} __attribute__ ((__packed__));
typedef struct message_s message_t;

#endif // DASH_CAM_H
