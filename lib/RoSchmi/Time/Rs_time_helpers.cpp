// RoSchmi_time_helpers for Arduino framework
//
// Parts of the code are taken from @sstaub NTP library for Arduino framework
// which has the following license
/**
 * NTP library for Arduino framework
 * The MIT License (MIT)
 * (c) 2018 sstaub
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Time/Rs_time_helpers.h"

Rs_time_helpers::Rs_time_helpers() { }

Rs_time_helpers::~Rs_time_helpers() { }
  
  void Rs_time_helpers::begin() 
  {
    if (dstZone) {
      timezoneOffset = dstEnd.tzOffset * SECS_PER_MINUTES;
      dstOffset = (dstStart.tzOffset - dstEnd.tzOffset) * SECS_PER_MINUTES;
      currentTime();
      beginDST();
    } 
  }

  void Rs_time_helpers::update(DateTime utcNow)
  {
      utcTime = utcNow.secondstime() + SECONDS_FROM_1970_TO_2000;
      lastUpdate = millis();
      //const unsigned long seventyYears = 2208988800UL;
            // subtract seventy years:
      //unsigned long epoch = secsSince1900 - seventyYears;
  }

  void Rs_time_helpers::update(uint32_t utcNowSeconds)
  {
      utcTime = utcNowSeconds + SECONDS_FROM_1970_TO_2000;
      lastUpdate = millis();
  }

void Rs_time_helpers::ruleDST(const char* tzName, int8_t week, int8_t wday, int8_t month, int8_t hour, int tzOffset) {
  strcpy(dstStart.tzName, tzName);
  dstStart.week = week;
  dstStart.wday = wday;
  dstStart.month = month;
  dstStart.hour = hour;
  dstStart.tzOffset = tzOffset;
  }

const char* Rs_time_helpers::ruleDST() {
  if(dstZone) {
    return ctime(&dstTime);
    }
  else return RULE_DST_MESSAGE;
  }

void Rs_time_helpers::ruleSTD(const char* tzName, int8_t week, int8_t wday, int8_t month, int8_t hour, int tzOffset) {
  strcpy(dstEnd.tzName, tzName);
  dstEnd.week = week;
  dstEnd.wday = wday;
  dstEnd.month = month;
  dstEnd.hour = hour;
  dstEnd.tzOffset = tzOffset;
  }
  
const char* Rs_time_helpers::ruleSTD() {
  if(dstZone) {
    return ctime(&stdTime);
    }
  else return RULE_STD_MESSAGE;
  }

const char* Rs_time_helpers::tzName() {
  if (dstZone) {
    if (summerTime()) return dstStart.tzName;
    else return dstEnd.tzName;
    }
  return GMT_MESSAGE; // TODO add timeZoneOffset
  }

void Rs_time_helpers::timeZone(int8_t tzHours, int8_t tzMinutes) {
  this->tzHours = tzHours;
  this->tzMinutes = tzMinutes;
  timezoneOffset = tzHours * 3600;
  if (tzHours < 0) {
    timezoneOffset -= tzMinutes * 60;
    }
  else {
    timezoneOffset += tzMinutes * 60;
    }
  }

void Rs_time_helpers::isDST(bool dstZone) {
  this->dstZone = dstZone;
  }

bool Rs_time_helpers::isDST() {
  return summerTime();
  }

  char* Rs_time_helpers::formattedTime(const char *format) {
  currentTime();
  memset(timeString, 0, sizeof(timeString));
  strftime(timeString, sizeof(timeString), format, current);
  return timeString;
  }

  bool Rs_time_helpers::formattedTime(char * outBuffer64Bytes, uint32_t bufferSize, char *format)
  {
    currentTime();
    memset(timeString, 0, sizeof(timeString));
    strftime(timeString, sizeof(timeString), format, current);
    
    if (bufferSize > strlen(timeString))
    {
      strcpy(outBuffer64Bytes, timeString);
      return true;
    }
    else
    {
      return false;
    }
  }

void Rs_time_helpers::offset(int16_t days, int8_t hours, int8_t minutes, int8_t seconds) 
{
  diffTime = (86400 * days) + (3600 * hours) + (60 * minutes) + seconds;
}

void Rs_time_helpers::beginDST() 
{
  dstTime = calcDateDST(dstStart, current->tm_year + 1900);
  utcDST = dstTime - (dstEnd.tzOffset * SECS_PER_MINUTES);
  stdTime = calcDateDST(dstEnd, current->tm_year + 1900);
  utcSTD = stdTime - (dstStart.tzOffset * SECS_PER_MINUTES);
  yearDST = current->tm_year + 1900;
  }

time_t Rs_time_helpers::calcDateDST(struct ruleDST rule, int year) 
{
	uint8_t month = rule.month;
	uint8_t week = rule.week;
	if (week == 0) {
		if (month++ > 11) {
			month = 0;
			year++;
			}
		week = 1;
}

	struct tm tm;
	tm.tm_hour = rule.hour;
	tm.tm_min = 0;
	tm.tm_sec = 0;
	tm.tm_mday = 1;
	tm.tm_mon = month;
	tm.tm_year = year - 1900;
	time_t t = mktime(&tm);

	t += ((rule.wday - tm.tm_wday + 7) % 7 + (week - 1) * 7 ) * SECS_PER_DAY;
	if (rule.week == 0) t -= 7 * SECS_PER_DAY;
	return t;
	}

bool Rs_time_helpers::summerTime() 
{
  if ((utcCurrent > utcDST) && (utcCurrent <= utcSTD)) {
    return true;
    }
  else {
    return false;
    }
}

void Rs_time_helpers::currentTime() 
{
  utcCurrent = diffTime + utcTime + ((millis() - lastUpdate) / 1000); 
  if (dstZone) {
    if (summerTime()) {
      local = utcCurrent + dstOffset + timezoneOffset;
      current = gmtime(&local);    
      }
    else {
      local = utcCurrent + timezoneOffset;
      current = gmtime(&local);
      }
    if ((current->tm_year + 1900) > yearDST) beginDST();
    }
  else {
    local = utcCurrent + timezoneOffset;
    current = gmtime(&local);
    }
}

void Rs_time_helpers::dateTimeToStringFormat_01(char * outBuffer50Bytes, const char *format)
{
  currentTime();
  memset(timeString, 0, sizeof(timeString));
  strftime(timeString, sizeof(timeString), format, current);
  strcpy(outBuffer50Bytes, timeString); 
}
