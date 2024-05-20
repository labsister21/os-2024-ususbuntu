#include "header/cpu/portio.h"
#include "header/stdlib/string.h"
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define CURRENT_YEAR 2024

int century_register = 0x00;

unsigned char second;
unsigned char minute;
unsigned char hour;
unsigned char day;
unsigned char month;
unsigned int year;

enum {
    cmos_address = 0x70,
    cmos_data    = 0x71
};

void syscallClock(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
  __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
  __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
  __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
  __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
  // Note : gcc usually use %eax as intermediate register,
  //        so it need to be the last one to mov
  __asm__ volatile("int $0x30");
}

int get_update_in_progress_flag() {
    out(cmos_address, 0x0A);
    return (in(cmos_data) & 0x80);
}

unsigned char get_RTC_register(int reg) {
    out(cmos_address, reg);
    return in(cmos_data);
}

void read_rtc() {
    unsigned char century = 0;
    unsigned char last_second;
    unsigned char last_minute;
    unsigned char last_hour;
    unsigned char last_day;
    unsigned char last_month;
    unsigned char last_year;
    unsigned char last_century;
    unsigned char registerB;

    while (get_update_in_progress_flag());
    second = get_RTC_register(0x00);
    minute = get_RTC_register(0x02);
    hour = get_RTC_register(0x04);
    day = get_RTC_register(0x07);
    month = get_RTC_register(0x08);
    year = get_RTC_register(0x09);
    if (century_register != 0) {
        century = get_RTC_register(century_register);
    }

    do {
        last_second = second;
        last_minute = minute;
        last_hour = hour;
        last_day = day;
        last_month = month;
        last_year = year;
        last_century = century;

        while (get_update_in_progress_flag());
        second = get_RTC_register(0x00);
        minute = get_RTC_register(0x02);
        hour = get_RTC_register(0x04);
        day = get_RTC_register(0x07);
        month = get_RTC_register(0x08);
        year = get_RTC_register(0x09);
        if (century_register != 0) {
            century = get_RTC_register(century_register);
        }
    } while ((last_second != second) || (last_minute != minute) || (last_hour != hour) ||
             (last_day != day) || (last_month != month) || (last_year != year) ||
             (last_century != century));

    registerB = get_RTC_register(0x0B);

    if (!(registerB & 0x04)) {
        second = (second & 0x0F) + ((second / 16) * 10);
        minute = (minute & 0x0F) + ((minute / 16) * 10);
        hour = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | (hour & 0x80);
        day = (day & 0x0F) + ((day / 16) * 10);
        month = (month & 0x0F) + ((month / 16) * 10);
        year = (year & 0x0F) + ((year / 16) * 10);
        if (century_register != 0) {
            century = (century & 0x0F) + ((century / 16) * 10);
        }
    }

    if (!(registerB & 0x02) && (hour & 0x80)) {
        hour = ((hour & 0x7F) + 12) % 24;
    }

    if (century_register != 0) {
        year += century * 100;
    } else {
        year += (CURRENT_YEAR / 100) * 100;
        if (year < CURRENT_YEAR) year += 100;
    }
}

void custom_sprintf(char *str, const char *format, int h, int m, int s) {
    // This assumes the format is always "%02d:%02d:%02d"
    str[0] = (h / 10) + '0';
    str[1] = (h % 10) + '0';
    str[2] = ':';
    str[3] = (m / 10) + '0';
    str[4] = (m % 10) + '0';
    str[5] = ':';
    str[6] = (s / 10) + '0';
    str[7] = (s % 10) + '0';
    str[8] = '\0';
}

void display_time(char* timeResult) {
    char time_str[9];
    custom_sprintf(time_str, "%02d:%02d:%02d", hour, minute, second);
    memcpy(timeResult, time_str, 9);
    printf("Time: %s\n", timeResult);
}

void clear()
{
  syscallClock(13, 0, 0, 0);
}

int main() {
    char timeResult[9];  // Allocate memory for the timeResult array
    while (1) {
        read_rtc();
        display_time(timeResult);
        sleep(1);
    }
    return 0;
}
