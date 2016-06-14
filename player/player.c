#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "dai_mon_data.h"

#define PORT 10873  // The port on which to listen for incoming data.

void fail(const char *error) {
      perror(error);
      exit(1);
}

int main(void) {
      struct sockaddr_in other_address = {0};
      int other_socket, address_length = sizeof(other_address);

      // Create a UDP socket.
      if ((other_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
            fail("socket");

      other_address.sin_family = AF_INET;
      other_address.sin_port = htons(PORT);

      if (inet_aton("127.0.0.1", &other_address.sin_addr) == 0)
            fail("inet_aton()");

      struct dai_mon_msg msg = {0};
      msg.serial_number = 0xABADBABE;
      strncpy(msg.data.vehicle_number, "N8002RE\0", 8);
      msg.data.latitude = 1212;
      msg.data.longitude = 3434;
      msg.data.altitude = 5656;
      msg.data.ground_speed = 7878;
      msg.data.ground_track = 9090;
      msg.data.vertical_speed = 2020;
      msg.data.time_ms = 4949;

      for (int i = 0; i < 30; i++) {
            if (sendto(other_socket, &msg, sizeof(msg), 0, (struct sockaddr *) &other_address, address_length) == -1)
                  fail("sendto()");
            sleep(1);
      }
      close(other_socket);
      return 0;
}


