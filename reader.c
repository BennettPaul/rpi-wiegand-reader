/*
 * linked with -lpthread -lwiringPi -lrt
 */

// #define _POSIX_C_SOURCE 200809L

#include <curl/curl.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <wiringPi.h>

#define PIN_0 0       // GPIO Pin 17 | Green cable | Data0
#define PIN_1 1       // GPIO Pin 18 | White cable | Data1
#define PIN_SOUND 25  // GPIO Pin 26 | Yellow cable | Sound

#define MAXWIEGANDBITS 32
#define READERTIMEOUT 3000000
#define LEN 256

static unsigned long long __wiegandData;
static unsigned long __wiegandBitCount;
static struct timespec __wiegandBitTime;

#define Max_Digits 10

void getData0(void) {
    __wiegandData <<= 1;
    __wiegandBitCount++;
    clock_gettime(CLOCK_MONOTONIC, &__wiegandBitTime);
}

void getData1(void) {
    __wiegandData <<= 1;
    __wiegandData |= 1;
    __wiegandBitCount++;
    clock_gettime(CLOCK_MONOTONIC, &__wiegandBitTime);
}

int wiegandInit(int d0pin, int d1pin) {
    // Setup wiringPi
    wiringPiSetup();
    pinMode(d0pin, INPUT);
    pinMode(d1pin, INPUT);
    pinMode(PIN_SOUND, OUTPUT);

    wiringPiISR(d0pin, INT_EDGE_FALLING, getData0);
    wiringPiISR(d1pin, INT_EDGE_FALLING, getData1);
}

void wiegandReset() {
    __wiegandData = 0;
    __wiegandBitCount = 0;
}

int wiegandGetPendingBitCount() {
    struct timespec now, delta;
    clock_gettime(CLOCK_MONOTONIC, &now);
    delta.tv_sec = now.tv_sec - __wiegandBitTime.tv_sec;
    delta.tv_nsec = now.tv_nsec - __wiegandBitTime.tv_nsec;

    if ((delta.tv_sec > 1) || (delta.tv_nsec > READERTIMEOUT))
        return __wiegandBitCount;

    return 0;
}

int wiegandReadData(unsigned long long* data, int dataMaxLen) {
    if (wiegandGetPendingBitCount() > 0) {
        int bitCount = __wiegandBitCount;
        int byteCount = (__wiegandBitCount / 8) + 1;
        if (byteCount < dataMaxLen) {
            *data = __wiegandData;
        }

        // memcpy(data, (void *)__wiegandData,
        //        ((byteCount > dataMaxLen) ? dataMaxLen : byteCount));

        wiegandReset();
        return bitCount;
    }
    return 0;
}

// void makeBeep(int millisecs, int times) {
//     int i;
//     for (i = 0; i < times; i++) {
//         digitalWrite(PIN_SOUND, LOW);
//         delay(millisecs);
//         digitalWrite(PIN_SOUND, HIGH);
//         delay(millisecs / 2);
//     }
// }

char* long_to_binary(unsigned long long k, int size)
{
    static char c[65];
    c[0] = '\0';

    unsigned long long val;
    int i;
    for (i = 0, val = 1ULL << (sizeof(unsigned long long)*8-1); val > 0; val >>= 1, i++) {
        if (i > 28) {
            strcat(c, ((k & val) == val) ? "1" : "0");
        }
    }
    return c;
}


void main(void) {
    CURL *handle = curl_easy_init();

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    int i;

    wiegandInit(PIN_0, PIN_1);

    while (1) {
        int bitLen = wiegandGetPendingBitCount();
        if (bitLen == 0) {
            usleep(5000);
        } else {
            unsigned long long data;
            bitLen = wiegandReadData(&data, 100);
            int bytes = bitLen / 8 + 1;
            FILE *fp;
            fp = fopen("output", "a");


            char timeStr[Max_Digits + sizeof(char)];
            sprintf(timeStr, "%lu", (unsigned long)time(NULL));
            printf("%lu ", (unsigned long)time(NULL));
            fprintf(fp, "%lu ", (unsigned long)time(NULL));
            printf("Read %d bits (%d bytes): ", bitLen, bytes);
            fprintf(fp, "Read %d bits (%d bytes): ", bitLen, bytes);
            // for (i = 0; i < bytes; i++) printf("%02X", (int)data[i]);
            // for (i = 0; i < bytes; i++) fprintf(fp, "%02X", (int)data[i]);

            printf(" : ");
            fprintf(fp, " : ");
            fclose(fp);

            char* dataStr = long_to_binary(data, 35);
            printf("%s\n", dataStr);
            fprintf(fp, "%s\n", dataStr);
            for (int i = 14; i <= 33; i++) {
                printf("%c", dataStr[i]);
            }
            printf("\n");
            long code = 0;
            long pval = 1;
            for (int i = 33; i >= 14; i--) {
                if (dataStr[i] == '1') {
                    code += 1 * pval;
                }
                pval *= 2;
            }
            printf("%d", code);

            char codeStr[Max_Digits + sizeof(char)];
            sprintf(codeStr, "%li", code);

            char data[256];
            strcpy(data, "{\"room\": \"CRTVC300\", \"timestamp\": ");
            strcat(data, timeStr);
            strcat(data, ", \"sid\": \"");
            strcat(data, codeStr);
            strcat(data, "\"}");
            /* post binary data */
            curl_easy_setopt(handle, CURLOPT_POSTFIELDS, data);
            /* set the size of the postfields data */
            curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, 256);
            /* pass our list of custom made headers */
            curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(handle, CURLOPT_URL, "https://api.ams-lti.com/attendance");

            curl_easy_perform(handle); /* post away! */

            fp = fopen("output", "a");
            printf("\n");
            fprintf(fp, "\n");
            fclose(fp);

            // makeBeep(200, 1);
        }
    }
    curl_slist_free_all(headers); /* free the header list */
}
