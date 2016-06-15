/*
 * Insert copyright notices and related text
 * Date : 02/08/15
 */

#ifndef DAI_MON_DATA_H
#define DAI_MON_DATA_H

#include <stdint.h>

#define TAIL_NUMBER_SIZE 8 // 7 characters for ID + 1 null terminator

struct vehicle {
    char number[TAIL_NUMBER_SIZE];
    double latitude;        // degrees
    double longitude;       // degrees
    double altitude;        // feet
    double ground_speed;    // knots
    double ground_track;    // degrees
    double vertical_speed;  // ft/min
    uint32_t time_ms;       // milliseconds
};

struct dai_mon_msg {
    long serial_number;
    struct vehicle vehicle;
};

#endif

