// Read voltages from MCP3202 @ SPI CE1 with 3.3V ref
// Requires wiringPi library
// Licence: MIT, see LICENSE file
// by Martin Strohmayer

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include <wiringPi.h>
#include <wiringPiSPI.h>

#define TRUE                (1==1)
#define FALSE               (!TRUE)
#define CHAN_CONFIG_SINGLE  8
#define CHAN_CONFIG_DIFF    0

const float fRefVoltage = 3.3f;
const float fResolution = 4096; //12-Bit
static sig_atomic_t end = 0;

static void sighandler(int signo) {
  end = 1;
}

const char *usage = "Usage: mcp3202 - reads cyclic A0 und A1 from SPI-Bus CE1 with 3.3V ref. voltage";


void spiSetup(int spiChannel, int &hSPI) {
    if ((hSPI = wiringPiSPISetup (spiChannel, 900000)) < 0) { //900 MHz SPi clock
        fprintf(stderr, "Can't open the SPI bus: %s\n", strerror(errno));
        exit (EXIT_FAILURE);
    }
}


static int AnalogRead(int spiChannel, int analogChannel) {
    if (analogChannel<0 || analogChannel>1) {
        return -1;
    }

    unsigned char spiData[3];
    unsigned char chanBits;

    if (analogChannel == 0) {
        chanBits = 0b11010000;
    } else {
        chanBits = 0b11110000;
    }
    spiData[0] = chanBits;
    spiData[1] = 0;
    spiData[2] = 0;
    wiringPiSPIDataRW(spiChannel, spiData, 3);
    return ((spiData [0] << 9) | (spiData [1] << 1) | (spiData[2] >> 7)) & 0xFFF;
}


void StoreMsg(const char* szMesg) {
    FILE* logile = fopen("voltage.log","a");
    if (logile) {
        fprintf(logile, "%s\n", szMesg);
        fclose(logile);
    } else {
        fprintf(stderr, "Can't open log file: %s\n", strerror(errno));
    }
}


void StoreVoltages(time_t rawtime, float elepsed, int CH0, int CH1) {
    char timestr[80];
    struct tm * timeinfo;

    float value0 = CH0 * fRefVoltage / fResolution;
    float value1 = CH1 * fRefVoltage / fResolution;

    timeinfo = localtime(&rawtime);
    strftime(timestr, 80, "%d.%m.%Y %H:%M:%S", timeinfo);
    printf("MCP3202(CE1): '%s' CH 0,1 = %04d, %04d -> %.3f, %.3f V\n", timestr, CH0, CH1, value0, value1);
    FILE* logile = fopen("voltage.log","a");
    if (logile) {
        fprintf(logile,"%s\t%.0f\t%.3f\t%.3f\n", timestr, elepsed, value0, value1);
        fflush(logile); //flush in case of power loose
        fclose(logile);
    } else {
       fprintf(stderr, "Can't open log file: %s\n", strerror(errno));
    }
}


int main(int argc, char *argv []){

    struct sigaction sa;
    time_t rawtime;
    time_t timerref;
    struct tm y2k19 = {0};
    int nLoopCount = 0;
    int nSleepFactor = 1;

    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = sighandler;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    y2k19.tm_hour = 0;   y2k19.tm_min = 0; y2k19.tm_sec = 0;
    y2k19.tm_year = 119; y2k19.tm_mon = 0; y2k19.tm_mday = 1;
    timerref = mktime(&y2k19);

    const int spiChannel = 1;
    static int hSPI;
    int CH0, CH1;

    wiringPiSetup();
    spiSetup(spiChannel, hSPI);
    StoreMsg("MCP3202 START");
    do {
        nLoopCount++;
        CH0 = AnalogRead(spiChannel, 0);
        CH1 = AnalogRead(spiChannel, 1);
        time(&rawtime);
        if (CH0>0 || CH1>0) {
            StoreVoltages(rawtime, difftime(rawtime, timerref), CH0, CH1);
            nSleepFactor = 1;
        } else {
            fprintf(stderr, "no answer\n");
            if (nSleepFactor<10) {
                nSleepFactor++;
            }
        }
        sleep(60*nSleepFactor);
    } while(!end);
    close(hSPI);
    StoreMsg("MCP3202 FINISHED");
    return 0;
}

