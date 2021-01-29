#include <Arduino.h>
#include "DateTime.h"
#include "RTC_SAMD51.h"

#ifndef _SYSTIME_H_
#define _SYSTIME_H_

class SysTime
{
public:
    SysTime();
    ~SysTime();
    
    DateTime getTime();
    void setTime(DateTime);
    void begin(DateTime);
};
#endif 