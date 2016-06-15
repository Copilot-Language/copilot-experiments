#include <arpa/inet.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "dai_mon_data.h"
#include "listener.h"

// Time to wait between sending packets (ms).
#define DELAY 100

static struct vehicle next_vehicle(FILE * restrict s);
static bool is_help(const char * arg);
static void fail(const char * error);
static void delay(void);

int main(int argc, char ** argv) {
      FILE * file = NULL;

      if (argc == 1) {
            puts("Reading from stdin...");
            file = stdin;
      } else if (argc == 2 && !is_help(argv[1])) {
            if (!(file = fopen(argv[1], "r"))) fail("fopen");
      } else {
            printf("Usage: %s <file>\n"
                   "Where <file> is a text file containing comma-separated vehicle test data. Columns should be in this order:\n"
                   "name, latitude (degrees), longitude (degrees), altitude (feet), track (degrees), ground speed (knots), vertical speed (feet/min), time (s)\n"
                   "For example:\n"
                   "TRAF1, 37.02641019, -76.59844441, 656.36590000, 128.55563983, 26.38695916, 0.00000000, 22.0\n"
                   "If <file> is omitted, lines are read from the standard input.\n"
                   , *argv);
            exit(1);
      }

      struct sockaddr_in other_address = { 0 };
      int other_socket, address_length = sizeof(other_address);

      if ((other_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
            fail("socket");

      other_address.sin_family = AF_INET;
      other_address.sin_port = htons(PORT);

      if (inet_aton("127.0.0.1", &other_address.sin_addr) == 0)
            fail("inet_aton()");

      struct dai_mon_msg msg = { 0 };

      for (int i = 0; ; i++) {
            msg.serial_number = i;
            msg.vehicle = next_vehicle(file);

            printf("Sending #%d: %s\n", i, msg.vehicle.number);
            if (sendto(other_socket, &msg, sizeof(msg), 0, (struct sockaddr *) &other_address, address_length) == -1)
                  fail("sendto()");


            delay();
      }
      close(other_socket);
      return 0;
}

// Parses and returns the next vehicle, otherwise halts the program on error or when the end of the file is reached.
struct vehicle next_vehicle(FILE * restrict s) {
      assert(s);
      struct vehicle vehicle;
      double secs = 0;
      for (;;) {
            int nitems = fscanf(s, "%7[^,], %lf, %lf, %lf, %lf, %lf, %lf, %lf\n",
                        (char *) &vehicle.number, &vehicle.latitude, &vehicle.longitude, &vehicle.altitude,
                        &vehicle.ground_track, &vehicle.ground_speed, &vehicle.vertical_speed, &secs);

            vehicle.time_ms = (unsigned) secs * 1000;

            if (nitems == EOF) {
                  puts("End of input reached.");
                  exit(0);
            }

            if (nitems == 8) {
                  return vehicle;
            }

            size_t len;
            char * line = fgetln(s, &len);
            if (line) {
                  line[len - 1] = 0; // Newline to null.
                  printf("Error reading line; discarding: %s\n", line);
            } else {
                  puts("Error reading line; discarding.");
            }
      }
}

bool is_help(const char * arg) {
      return !strcmp("--help", arg) || !strcmp("-h", arg) || !strcmp("-?", arg);
}

void fail(const char * error) {
      perror(error);
      exit(1);
}

void delay(void) {
      struct timespec ts = { .tv_nsec = DELAY * 1000000 };
      nanosleep(&ts, NULL);
}
