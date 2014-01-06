/*
 * Note that the device and listener should be run with the same port and IP.
 * For example:
 *
 *  xrun --xscope-realtime --xscope-port 127.0.0.1:12346 ...
 *
 *  ./packet_analyser -s 127.0.0.1 -p 12346
 *
 */
/*
 * Includes for thread support
 */
#ifdef _WIN32
  #include <winsock.h>
#else
  #include <pthread.h>
#endif

#include <unistd.h>

#include "xscope_host_shared.h"
#include "host_xscope.h"

#define MAX_FILENAME_LEN 1024

const char *g_prompt = "";

/* Interface on which the glitch occurred */
int g_interface = 0;
/* Size of the data received */
int g_expected_count = 0;

void hook_data_received(int xscope_probe, void *data, int data_len)
{
  char filename[MAX_FILENAME_LEN];
  int i = 0;
  int *int_data = (int*)(&data[0]);
  FILE *f = NULL;

  if (g_expected_count == 0) {
    if (data_len != 8)
      print_and_exit("ERROR: Received %d bytes when expecting 8 with the length\n", data_len);
    g_expected_count = int_data[0] >> 8;
    g_interface = int_data[0] & 0xff;

    printf("Received glitch on interface %d\n", g_interface);

  } else {
    if (data_len != g_expected_count)
      print_and_exit("ERROR: Received %d bytes when expecting %d\n", data_len, g_expected_count);

    printf("Received glitch data\n");
    
    /* Create a unique glitch filename */
    do {
      sprintf(filename, "glitch_%d_%d.csv", g_interface, i);
      i++;
    } while (access(filename, F_OK) != -1);

    f = fopen(filename, "w");
    // In the case of an error still print the data to the screen
    if (f == NULL)
      f = stdout;

    for (i = 0; i < g_expected_count/4; i++) {
      fprintf(f, "%d, ", int_data[i]);
      if (i && ((i % 8) == 0))
        fprintf(f, "\n");
    }

    if (f == NULL)
      print_and_exit("ERROR: Failed to open file to write glitch '%s'\n", filename);

    fclose(f);
    g_expected_count = 0;
  }
}

void hook_exiting()
{
  // Do nothing
}

static char get_next_char(const char **buffer)
{
  const char *ptr = *buffer;
  int len = 0;
  while (*ptr && isspace(*ptr))
    ptr++;

  *buffer = ptr + 1;
  return *ptr;
}

static int convert_atoi_substr(const char **buffer)
{
  const char *ptr = *buffer;
  unsigned int value = 0;
  while (*ptr && isspace(*ptr))
    ptr++;

  if (*ptr == '\0')
    return 0;

  value = atoi((char*)ptr);

  while (*ptr && !isspace(*ptr))
    ptr++;

  *buffer = ptr;
  return value;
}

void print_console_usage()
{
  printf("Supported commands:\n");
  printf("  h|?     : print this help message\n");
  printf("  e a     : enable all channels\n");
  printf("  e <n>   : enable channel n\n");
  printf("  d a     : disable all channels\n");
  printf("  d <n>   : disable channel n\n");
  printf("  c <n> <freq> <do_glitch> <glitch_period> : configure channel n\n");
  printf("  q       : quit\n");
}

#define LINE_LENGTH 1024

/*
 * A separate thread to handle user commands to control the target.
 */
#ifdef _WIN32
DWORD WINAPI console_thread(void *arg)
#else
void *console_thread(void *arg)
#endif
{
  int sockfd = *(int *)arg;
  char buffer[LINE_LENGTH + 1];
  do {
    int i = 0;
    int c = 0;

    for (i = 0; (i < LINE_LENGTH) && ((c = getchar()) != EOF) && (c != '\n'); i++)
      buffer[i] = tolower(c);
    buffer[i] = '\0';

    const char *ptr = &buffer[0];
    char cmd = get_next_char(&ptr);
    switch (cmd) {
      case 'q':
        print_and_exit("Done\n");
        break;

      case 'e': {
        char to_send[2];
        const char *prev = ptr;
        char next = get_next_char(&ptr);
        if (next == 'a') {
          to_send[0] = HOST_ENABLE_ALL;
        } else {
          to_send[0] = HOST_ENABLE_ONE;
          to_send[1] = convert_atoi_substr(&prev);
        }
        printf("Sending %d:%d\n", to_send[0], to_send[1]);
        if (xscope_ep_request_upload(sockfd, 2, (unsigned char *)&to_send) != XSCOPE_EP_SUCCESS)
          printf("Failed to send\n");
        break;
      }

      case 'd': {
        char to_send[2];
        const char *prev = ptr;
        char next = get_next_char(&ptr);
        if (next == 'a') {
          to_send[0] = HOST_DISABLE_ALL;
        } else {
          to_send[0] = HOST_DISABLE_ONE;
          to_send[1] = convert_atoi_substr(&prev);
        }
        printf("Sending %d:%d\n", to_send[0], to_send[1]);
        if (xscope_ep_request_upload(sockfd, 2, (unsigned char *)&to_send) != XSCOPE_EP_SUCCESS)
          printf("Failed to send\n");
        break;
      }

      case 'c': {
        unsigned to_send[4];
        unsigned char *to_send_c = (unsigned char *)(&to_send[0]);
        to_send_c[0] = HOST_CONFIGURE_ONE;
        to_send_c[1] = convert_atoi_substr(&ptr);
        to_send[1] = convert_atoi_substr(&ptr);
        to_send[2] = convert_atoi_substr(&ptr);
        to_send[3] = convert_atoi_substr(&ptr);

        printf("Sending %d:%d\n", to_send_c[0], to_send_c[1]);
        if (xscope_ep_request_upload(sockfd, sizeof(to_send), (unsigned char *)&to_send) != XSCOPE_EP_SUCCESS != XSCOPE_EP_SUCCESS)
          printf("Failed to send\n");
        break;
      }

      case 'h':
      case '?':
        print_console_usage();
        break;

      default:
        printf("Unrecognised command '%s'\n", buffer);
        print_console_usage();
    }
  } while (1);

#ifdef _WIN32
  return 0;
#else
  return NULL;
#endif
}

void usage(char *argv[])
{
  printf("Usage: %s [-s server_ip] [-p port]\n", argv[0]);
  printf("  -s server_ip :   The IP address of the xscope server (default %s)\n", DEFAULT_SERVER_IP);
  printf("  -p port      :   The port of the xscope server (default %s)\n", DEFAULT_PORT);
  exit(1);
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
  HANDLE thread;
#else
  pthread_t tid;
#endif
  char *server_ip = DEFAULT_SERVER_IP;
  char *port_str = DEFAULT_PORT;
  int err = 0;
  int sockfd = 0;
  int c = 0;

  while ((c = getopt(argc, argv, "s:p:")) != -1) {
    switch (c) {
      case 's':
        server_ip = optarg;
        break;
      case 'p':
        port_str = optarg;
        break;
      case ':': /* -f or -o without operand */
        fprintf(stderr, "Option -%c requires an operand\n", optopt);
        err++;
        break;
      case '?':
        fprintf(stderr, "Unrecognized option: '-%c'\n", optopt);
        err++;
    }
  }
  if (optind < argc)
    err++;

  if (err)
    usage(argv);

  sockfd = initialise_common(server_ip, port_str);

  // Now start the console
#ifdef _WIN32
  thread = CreateThread(NULL, 0, console_thread, &sockfd, 0, NULL);
  if (thread == NULL)
    print_and_exit("ERROR: Failed to create console thread\n");
#else
  err = pthread_create(&tid, NULL, &console_thread, &sockfd);
  if (err != 0)
    print_and_exit("ERROR: Failed to create console thread\n");
#endif

  handle_socket(sockfd);
  return 0;
}

