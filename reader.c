#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define D0_LINE 17
#define D1_LINE 18
#define CHIP_NAME "gpiochip0"

#define READERTIMEOUT 3000000

static unsigned long long wiegandData;
static unsigned long wiegandBitCount;
static struct timespec wiegandBitTime;

void handleBit(int bitValue) {
    wiegandData <<= 1;
    wiegandData |= bitValue;
    wiegandBitCount++;
    clock_gettime(CLOCK_MONOTONIC, &wiegandBitTime);
}

int wiegandGetPendingBitCount() {
    struct timespec now, delta;
    clock_gettime(CLOCK_MONOTONIC, &now);

    delta.tv_sec = now.tv_sec - wiegandBitTime.tv_sec;
    delta.tv_nsec = now.tv_nsec - wiegandBitTime.tv_nsec;

    if ((delta.tv_sec > 1) || (delta.tv_nsec > READERTIMEOUT))
        return wiegandBitCount;

    return 0;
}

void wiegandReset() {
    wiegandData = 0;
    wiegandBitCount = 0;
}

char* ULL_to_binary(unsigned long long k) {
    static char c[65];
    c[0] = '\0';
    unsigned long long val;
    int i;

    for (i = 0, val = 1ULL << 63; val > 0; val >>= 1, i++) {
        if (i > 28) {
            strcat(c, ((k & val) == val) ? "1" : "0");
        }
    }
    return c;
}

int main() {
    struct gpiod_chip *chip;
    struct gpiod_line *line_d0, *line_d1;
    struct gpiod_line_event event;

    chip = gpiod_chip_open_by_name(CHIP_NAME);
    if (!chip) {
        perror("gpiod_chip_open_by_name");
        return 1;
    }

    line_d0 = gpiod_chip_get_line(chip, D0_LINE);
    line_d1 = gpiod_chip_get_line(chip, D1_LINE);
    if (!line_d0 || !line_d1) {
        perror("gpiod_chip_get_line");
        return 1;
    }

    if (gpiod_line_request_falling_edge_events(line_d0, "wiegand_d0") < 0 ||
        gpiod_line_request_falling_edge_events(line_d1, "wiegand_d1") < 0) {
        perror("gpiod_line_request_falling_edge_events");
        return 1;
    }

    wiegandReset();

    printf("Listening for Wiegand input...\n");

    while (1) {
        struct timespec timeout = {1, 0}; // 1 second timeout
        int ret_d0 = gpiod_line_event_wait(line_d0, &timeout);
        int ret_d1 = gpiod_line_event_wait(line_d1, &timeout);

        if (ret_d0 == 1 && gpiod_line_event_read(line_d0, &event) == 0)
            handleBit(0);
        if (ret_d1 == 1 && gpiod_line_event_read(line_d1, &event) == 0)
            handleBit(1);

        int bitLen = wiegandGetPendingBitCount();
        if (bitLen > 0) {
            unsigned long long data = wiegandData;
            printf("Wiegand: %s (%d bits)\n", ULL_to_binary(data), bitLen);

            // Extract code using your logic or simplified:
            unsigned long long val;
            int code = 0;
            for (val = 1ULL << 20; val > 1; val >>= 1) {
                if ((data & val) == val) {
                    code += 1 * (val >> 1);
                }
            }
            printf("Decoded: %d\n", code);
            wiegandReset();
        }

        usleep(1000); // Reduce CPU usage
    }

    gpiod_line_release(line_d0);
    gpiod_line_release(line_d1);
    gpiod_chip_close(chip);
    return 0;
}
