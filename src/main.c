#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <pthread.h>
#include <argp.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdbool.h>

#include "indicators.h"
#include "server_core.h"
#include "log.h"

#define DEFAILT_IP   "127.0.0.1"
#define DEFAULT_PORT  5000

const char *argp_program_version     = "1.0";
const char *argp_program_bug_address = "<alexeyfonlapshin@gmail.com>";

struct arguments
{
    char     *ip;
    uint16_t  port;
    bool      quiet;
    bool      verbose;
    uint8_t   daemonize;
};


static const int handle_signals[] =
{
    SIGHUP, SIGTERM, SIGINT, SIGQUIT
};

static void signal_handler(__attribute__((unused)) int signal)
{
    server_exit();
}

static int signals_handle_init(void)
{
    size_t i;
    int ret = 0;
    struct sigaction act = {0};
    sigemptyset(&act.sa_mask);
    act.sa_handler = signal_handler;
    for (i = 0; i < sizeof(handle_signals)/sizeof(handle_signals[0]); i++)
    {
        ret = sigaction(handle_signals[i], &act, NULL);
        if(ret != 0)
        {
            return ret;
        }
    }
    return ret;
}

static int is_number(char *str)
{
    size_t i;
    for(i = 0; i < strlen(str); i++)
    {
        if(!isdigit(str[i]))
        {
            return -1;
        }
    }
    return 0;
}

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;
  char *tmp;

  switch (key)
    {
    case 'q':
        arguments->quiet = true;
        break;
    case 'V':
        arguments->verbose = true;
        break;
    case 'i':
        arguments->ip = arg;
        break;
    case 'p':
      if(is_number(arg) != 0)
      {
          printf("Input port value not a number! (%s)\n", arg);
          return -1;
      }
      unsigned long tmp_port = strtoul(arg, &tmp, 10);
      if (tmp_port > USHRT_MAX)
      {
          printf("Input port value not a number! (%s)\n", arg);
          return -1;
      }
      arguments->port = (uint16_t)tmp_port;
      break;
    case 'd':
      arguments->daemonize = 1;
      break;


    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static int parse_parameters(int argc, char **argv, struct arguments *arguments)
{
    struct argp_option options[] = {
      {"quiet"    , 'q',  NULL    , 0,  "Print only error messages", 0},
      {"verbose"  , 'V',  NULL    , 0,  "Print debug messages", 0},
      {"ip"       , 'i', "address", 0,  "Server ip address (default: localhost)", 0},
      {"port"     , 'p', "port"   , 0,  "Server TCP port (default: 5000)", 0},
      {"daemonize", 'd',  NULL    , 0,  "Run as a daemon", 0},
      { 0 },
    };
    char *doc = "This is a computation server which receives video files from"
            "dash cams, calculating a number of computation-intensive driver "
            "behaviour indicators, and sending these back "
            "to the vehicle in the same connection";
    struct argp argp = {options, parse_opt, NULL, doc, NULL, NULL, NULL};

    return argp_parse(&argp, argc, argv, 0, 0, arguments);
}

int main(int argc, char **argv) {
    struct arguments arguments = {
            .ip = DEFAILT_IP,
            .port = DEFAULT_PORT,
            .quiet = false,
            .verbose = false,
            .daemonize = 0
    };

    int operation = -1;
    int ret = parse_parameters(argc, argv, &arguments);
    if(ret != 0)
    {
        logger(ERROR, "Parse arguments failed\n");
        goto exit;
    }

    operation--;
    ret = signals_handle_init();
    if(ret != 0)
    {
        perror("Signals handler failed while setting\n");
        goto exit;
    }

    operation--;
    if(arguments.daemonize == 1)
    {
        ret = daemon(0, 0); //don't change working dir. Silent
        if(ret != 0)
        {
            perror("Can't init daemon\n");
            goto exit;
        }
    }
    set_debug(arguments.verbose);
    set_quiet(arguments.quiet);

    server_run(arguments.ip, arguments.port);

exit:
    return ret == 0 ? ret : operation;
}


