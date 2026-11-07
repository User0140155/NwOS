#ifndef TIME_H
#define TIME_H

extern volatile unsigned long long idle_us_accum;

void sleep(unsigned int ms);

#endif