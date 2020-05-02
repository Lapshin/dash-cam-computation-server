#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <argp.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <inttypes.h>

#include "dash_cam.h"

const char *argp_program_version = "1.0";
const char *argp_program_bug_address = "<alexeyfonlapshin@gmail.com>";

struct arguments
{
    uint32_t size;
    uint32_t frame_size;
    bool     read;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = state->input;

    switch(key)
    {
    case 's':
        arguments->size = (uint32_t) atoll(arg);
        break;
    case 'f':
        arguments->frame_size = (uint32_t) atoll(arg);
        break;
    case 'r':
        arguments->read = true;
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static int parse_parameters(struct arguments *arguments, int argc, char **argv)
{
    struct argp_option options[] = {
        {"size", 's', "size", 0,  "size of generated data in bytes", 0},
        {"frame-size", 'f', "frame-size", 0,  "size of one frame in bytes", 0},
        {"read", 'r', NULL, 0, "reaading data from stdout", 0},
        { 0 }
    };

    char *doc = "This is a dash cam simulator";

    struct argp argp = {options, parse_opt, NULL, doc, NULL, NULL, NULL};

    return argp_parse(&argp, argc, argv, 0, 0, arguments);
}

static void fill_header(messageHeader_t *header, uint32_t size, uint32_t frame_size)
{
    if (header == NULL)
    {
        perror("fill_header NULL pointer");
        exit(1);
    }
    memset(header, 0x00, sizeof(*header));

    header->magic      = htonl(HEADER_MAGIC);
    header->size       = htonl(size);
    header->frame_size = htonl(frame_size);
    header->version    = htonl(HEADER_VERSION);
}

int dash_cam_read_indicators()
{
    size_t i;
    int operation = 1;
    messageHeader_t header;
    size_t read_size = fread(&header, sizeof(header), 1, stdin);
    if (read_size == 0)
    {
        printf("Error reading data from stdin\n");
        return operation;
    }
    uint32_t version = ntohl(header.version);
    uint32_t magic = ntohl(header.magic);
    uint32_t size = ntohl(header.size);
    uint32_t frame_size = ntohl(header.frame_size);
    uint32_t indicators_count = 0;
    uint64_t *payload_ptr = NULL;

    operation++;
    if (version != HEADER_VERSION)
    {
        printf("version is not compatible (%d:%d)\n", version, HEADER_VERSION);
        return operation;
    }
    operation++;
    if (magic != HEADER_MAGIC)
    {
        printf("magic is not compatible (%d:%d)\n", magic, HEADER_MAGIC);
        return operation;
    }
    operation++;
    if (frame_size != 8)
    {
        printf("frame size is not uint64_t size (%d:%ld)\n", frame_size, sizeof(uint64_t));
        return operation;
    }
    operation++;
    if (frame_size == 0)
    {
        printf("frame size is zero\n");
        return operation;
    }
    operation++;
    if (size%frame_size != 0)
    {
        printf("size of payload does not divided for frame_size\n");
        return operation;
    }
    indicators_count = size/frame_size;
    operation++;
    payload_ptr = malloc(size);
    if (payload_ptr == NULL)
    {
        printf("Can not allocate memory\n");
        return operation;
    }
    operation++;
    read_size = fread(payload_ptr, size, 1, stdin);
    if (read_size == 0)
    {
        printf("Error reading data from stdin\n");
        return operation;
    }
    for (i = 0; i < indicators_count; i++)

    {
        printf("%" PRIu64 " ", be64toh(payload_ptr[i]));
    }
    free(payload_ptr);
    return 0;
}

int main(int argc, char **argv)
{
    message_t *data_to_send = NULL;
    struct arguments arguments = {
        .size = 512,
        .frame_size = 32,
        .read = false
    };
    size_t i;
    int ret = parse_parameters(&arguments, argc, argv);
    if(ret != 0)
    {
        printf("Parse arguments fail (%d %s)...\n", errno, strerror(errno));
        return -1;
    }

    if (arguments.read)
    {
        return dash_cam_read_indicators();
    }

    data_to_send = calloc(sizeof(data_to_send->header) + arguments.size, 1);
    fill_header(&data_to_send->header, arguments.size, arguments.frame_size);

    srand((unsigned int)time(NULL));
    for(i = 0; i < arguments.size/4; i++)
    {
        ((uint32_t *)data_to_send->payload)[i] = htonl((uint32_t)rand());
    }

    fwrite(data_to_send, sizeof(data_to_send->header) + arguments.size, 1, stdout);
    fflush(stdout);
    free(data_to_send);
}
