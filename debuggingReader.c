/*
 * linked with -lpthread -lwiringPi -lrt -lcurl
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

// used to pull data the Data1 (White)
void getData0(void) {
    __wiegandData <<= 1;                                // Shift the data by 1 to the left data is little endian
    __wiegandBitCount++;                                // counts the clock cycles
    clock_gettime(CLOCK_MONOTONIC, &__wiegandBitTime);  // gets the time from CLOCK_MONOTONIC and stores it into __wiengandBitTime
}

// reads data from from Data1(White)
void getData1(void) {
    __wiegandData <<= 1;                                // left bitshit wiegandData 
    __wiegandData |= 1;                                 // or the value to 1
    __wiegandBitCount++;                                // add to the count
    clock_gettime(CLOCK_MONOTONIC, &__wiegandBitTime);  // store time from CLOCK_MONOTONIC and store int __wiengandBitTime
}

// initilize the wires needed for the card reader
int wiegandInit(int d0pin, int d1pin) {
    // Setup wiringPi
    wiringPiSetup();                                // initilizes the wiringPi program
    pinMode(d0pin, INPUT);                          // sets d0pin as an input (data0)
    pinMode(d1pin, INPUT);                          // sets d1pin as an input (data1)
    pinMode(PIN_SOUND, OUTPUT);                     // sets the audio pin as an output

    wiringPiISR(d0pin, INT_EDGE_FALLING, getData0); // receives data from d0pin at a falling clock edge and calls getData0.
    wiringPiISR(d1pin, INT_EDGE_FALLING, getData1); // receives data from d1pin at a falling clock edge and calls getData1
}

// resets the reader
void wiegandReset() {
    __wiegandData = 0;      //changes global to 0
    __wiegandBitCount = 0;  //changes global to 0
}

// delays the reader so it can collect all data
int wiegandGetPendingBitCount() {
    struct timespec now, delta;                                 // create two time values now and a delta
    clock_gettime(CLOCK_MONOTONIC, &now);                       // set now to current time
    delta.tv_sec = now.tv_sec - __wiegandBitTime.tv_sec;        // sets the delta.tv_sec (whole seconds) = to the current seconds - the BitTime seconds
    delta.tv_nsec = now.tv_nsec - __wiegandBitTime.tv_nsec;     // sets the delta nano seconds = to the currnt nanoseconds - the BitTime nano seconds

    if ((delta.tv_sec > 1) || (delta.tv_nsec > READERTIMEOUT))  // checks to see if delta is too great and times out (checks seconds and nanoseconds)
        return __wiegandBitCount;                               // if true returns the number of bits counted

    return 0;
}

// reads the data and returns it as an it
int wiegandReadData(unsigned long long* data, int dataMaxLen) {
    if (wiegandGetPendingBitCount() == 35) {              // checks to see if there are any pending Bits
        // TODO: check if facility code is correct
        int bitCount = __wiegandBitCount;
        int byteCount = (__wiegandBitCount / 8) + 1;
        if (byteCount < dataMaxLen) {                   // checks to see if the byteCount is less than the max length of the data
            *data = __wiegandData;                      // if so then it loads the pointer data with the wiegandData 
        }
        wiegandReset();                                 // reset everything
        return bitCount;                                // return the bitcount
    }
    return 0;
}

// Converts an unsigned long long to a binary string literal
char* ULL_to_binary(unsigned long long k) {
    static char c[65];                                                                      // set the size of the output to 65
    c[0] = '\0';                                                                            // load a null terminator into the string.

    unsigned long long val;
    int i;
    
    for (i = 0, val = 1ULL << (sizeof(unsigned long long)*8-1); val > 0; val >>= 1, i++) { // checks that val is less than 0 :
        if (i > 28) { // trim binary down to last 35-bits
            strcat(c, ((k & val) == val) ? "1" : "0");
        }
    }
    return c;
}


int main(int argc, char const *argv[]) {
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

            printf("Read %d bits (%d bytes): %s", bitLen, bytes, ULL_to_binary(data));
            fprintf(fp, "Read %d bits (%d bytes): %s ", bitLen, bytes, ULL_to_binary(data));

            // char* dataStr = ULL_to_binary(data);
            // printf("%s\n", dataStr);
            // fprintf(fp, "%s\n", dataStr);
            // for (int i = 14; i <= 33; i++) {
                // printf("%c", dataStr[i]);
            // }
            // printf("\n");
            
            unsigned long long val;
            int code = 0;
    
            for (val = 1ULL << 20; val > 1; val >>= 1) {
                if (data & val == val) {
                    code += 1 * (val >> 1);
                }
            }

            printf("(%d)", code);
            fprintf("(%d)", code);
            fclose(fp);
            char codeStr[Max_Digits + sizeof(char)];
            sprintf(codeStr, "%d", code);

            char jsonData[256];
            strcpy(jsonData, "{\"room\": \"");
            strcat(jsonData, argv[1]);
            strcat(jsonData, "\", \"timestamp\": ");
            strcat(jsonData, timeStr);
            strcat(jsonData, ", \"sid\": \"");
            strcat(jsonData, codeStr);
            strcat(jsonData, "\"}");
            /* post AttendanceRecord data */
            curl_easy_setopt(handle, CURLOPT_POSTFIELDS, jsonData);
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
