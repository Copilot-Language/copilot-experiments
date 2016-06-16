#include <arpa/inet.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "copilot.h"
#include "dai_mon_data.h"

// Max number of vehicles encounterable before we start replacing the oldest.
#define MAX_VEHICLES 500
// Time to wait between checks (ms).
#define DELAY 500
// The port on which to listen for incoming data.
#define PORT 10873

char numO[TAIL_NUMBER_SIZE];
double latO, lonO, gsO, trkO, altO, vsO;
char numI[TAIL_NUMBER_SIZE];
double latI, lonI, gsI, trkI, altI, vsI;

// TODO(chathhorn): initialize these
double dthr = 1000;
double tthr = 1000;
double zthr = 1000;
double tcoathr = 1000;

struct mon_vehicle {
      char (* number)[TAIL_NUMBER_SIZE];
      double * latitude;        // degrees
      double * longitude;       // degrees
      double * altitude;        // feet
      double * ground_speed;    // knots
      double * ground_track;    // degrees
      double * vertical_speed;  // ft/min
};

struct vehicle_meta {
      long serial_number;
      bool fresh;
      pthread_mutex_t mutex;
      struct vehicle vehicle;
};

static struct mon_vehicle ownship = {
      .number = &numO,
      .latitude = &latO,
      .longitude = &lonO,
      .altitude = &altO,
      .ground_speed = &gsO,
      .ground_track = &trkO,
      .vertical_speed = &vsO,
};

static struct mon_vehicle intruder = {
      .number = &numI,
      .latitude = &latI,
      .longitude = &lonI,
      .altitude = &altI,
      .ground_speed = &gsI,
      .ground_track = &trkI,
      .vertical_speed = &vsI,
};

static int nvehicles;
static struct vehicle_meta vehicles[MAX_VEHICLES];

static void mon_vehicle(struct mon_vehicle, struct vehicle);
static void fail(const char *);
static void * udp_listener(void *);
static int find_vehicle(const char (*)[TAIL_NUMBER_SIZE]);
static void update_vehicle(struct dai_mon_msg);
static void run_monitor(void);
static void lock(pthread_mutex_t *);
static void unlock(pthread_mutex_t *);
static void nap(void);

int main(void) {
      pthread_t listener_tid;

      if (pthread_create(&listener_tid, NULL, udp_listener, NULL) != 0)
            fail("pthread_create()");

      while (nvehicles < 2) nap();

      for (;;) {
            for (int own = 0; own != nvehicles - 1; ++own) {
                  lock(&vehicles[own].mutex);
                  mon_vehicle(ownship, vehicles[own].vehicle);
                  bool fresh = vehicles[own].fresh;
                  vehicles[own].fresh = false;
                  unlock(&vehicles[own].mutex);

                  if (fresh) {
                        for (int intr = 0; intr != nvehicles; ++intr) {
                              if (intr == own) continue;

                              lock(&vehicles[intr].mutex);
                              mon_vehicle(intruder, vehicles[intr].vehicle);
                              unlock(&vehicles[intr].mutex);

                              run_monitor();
                        }
                  }
            }
            nap();
      }
      return 0;
}

int find_vehicle(const char (* number) [TAIL_NUMBER_SIZE]) {
      assert(number);
      for (int i = 0; i != nvehicles; ++i) {
            if (!strncmp((const char *) number, (const char *) vehicles[i].vehicle.number, TAIL_NUMBER_SIZE)) {
                  return i;
            }
      }
      return -1;
}

int find_oldest(void) {
      assert(nvehicles > 0);
      int oldest = 0;;
      long oldest_serial = vehicles[0].serial_number;
      for (int i = 0; i != nvehicles; ++i) {
            if (vehicles[i].serial_number > oldest_serial) {
                  oldest = i;
                  oldest_serial = vehicles[i].serial_number;
            }
      }
      return oldest;
}

void update_vehicle(struct dai_mon_msg msg) {
      int i;
      if ((i = find_vehicle(&msg.vehicle.number)) != -1) {
            lock(&vehicles[i].mutex);
            vehicles[i].fresh = true;
            vehicles[i].serial_number = msg.serial_number;
            vehicles[i].vehicle = msg.vehicle;
            unlock(&vehicles[i].mutex);
      } else if (nvehicles < MAX_VEHICLES) {
            vehicles[nvehicles].fresh = true;
            vehicles[nvehicles].serial_number = msg.serial_number;
            vehicles[nvehicles].vehicle = msg.vehicle;
            pthread_mutex_init(&vehicles[nvehicles].mutex, NULL);
            nvehicles++;
      } else {
            puts("Too many vehicles, replacing the oldest!");
            int oldest = find_oldest();
            lock(&vehicles[oldest].mutex);
            vehicles[oldest].fresh = true;
            vehicles[oldest].serial_number = msg.serial_number;
            vehicles[oldest].vehicle = msg.vehicle;
            unlock(&vehicles[oldest].mutex);
      }
}

void * udp_listener(void * args) {
      struct dai_mon_msg rcvd;

      int self_socket;
      if ((self_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
            fail("socket");
      }

      struct sockaddr_in self_address = { 0 };
      self_address.sin_family = AF_INET;
      self_address.sin_port = htons(PORT);
      self_address.sin_addr.s_addr = htonl(INADDR_ANY);

      if (bind(self_socket, (struct sockaddr*) &self_address, sizeof(self_address)) == -1) {
            fail("bind");
      }

      int current_serial = 0;
      for (;;) {
            printf("[UDP THREAD] waiting for data...\n");

            struct sockaddr_in other_address;
            socklen_t address_length = sizeof(other_address);
            ssize_t actual_length;
            if ((actual_length = recvfrom(self_socket, &rcvd, sizeof(rcvd), 0, (struct sockaddr *) &other_address, &address_length)) != sizeof(rcvd)) {
                  fail("recvfrom()");
            }

            if (actual_length < sizeof(rcvd)) {
                  puts("received packet fragment; discarding instead of attempting reassembly.");
                  continue;
            }

            printf("[UDP THREAD] received packet from: %s:%u\n", inet_ntoa(other_address.sin_addr), ntohs(other_address.sin_port));

            if (rcvd.serial_number < current_serial) {
                  puts("received out-of-order packet; discarding.");
                  continue;
            }

            current_serial = rcvd.serial_number;
            update_vehicle(rcvd);
      }

      return NULL;
}

void mon_vehicle(struct mon_vehicle mon, struct vehicle vehicle) {
      strncpy((char *) mon.number, (char *) &vehicle.number, TAIL_NUMBER_SIZE);
      *mon.number[TAIL_NUMBER_SIZE - 1] = 0;
      *mon.latitude = vehicle.latitude;
      *mon.longitude = vehicle.longitude;
      *mon.altitude = vehicle.altitude;
      *mon.ground_speed = vehicle.ground_speed;
      *mon.ground_track = vehicle.ground_track;
      *mon.vertical_speed = vehicle.vertical_speed;
}

void run_monitor(void) {
      printf("Checking: %s <==> %s\n", (char *) ownship.number, (char *) intruder.number);
      step();
}

void alert_wcv(void) {
      puts("*** Alert triggered: well-clear violation!");
      printf("*** Ownship: %s, Intruder: %s\n", (char *) ownship.number, (char *) intruder.number);
}

void lock(pthread_mutex_t * mutex) {
      if (pthread_mutex_lock(mutex)) {
            fprintf(stderr, "Error: failed to acquire a lock... something bad has gone wrong, exiting.");
            exit(1);
      }
}

void unlock(pthread_mutex_t * mutex) {
      if (pthread_mutex_unlock(mutex)) {
            fprintf(stderr, "Error: failed to unlock a lock... something bad has gone wrong, exiting.");
            exit(1);
      }
}

void nap(void) {
      struct timespec ts = { .tv_nsec = DELAY * 1000000 };
      nanosleep(&ts, NULL);
}

void fail(const char * error) {
      perror(error);
      exit (1);
}
