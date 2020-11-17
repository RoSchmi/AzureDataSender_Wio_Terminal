#include <Arduino.h>
#include <DateTime.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _ROSCHMI_TIME_HELPERS_H_
#define _ROSCHMI_TIME_HELPERS_H_

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus



char daysOfTheWeek[7][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
//char monthsOfTheYear[12][5] = {"Jan", "Feb"}

void dateTimeToStringFormat_01(char * outBuffer50Bytes, DateTime now);




#ifdef __cplusplus
}
#endif // __cplusplus

#endif
