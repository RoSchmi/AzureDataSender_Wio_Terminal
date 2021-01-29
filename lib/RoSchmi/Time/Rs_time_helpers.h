#include <Arduino.h>
#include "DateTime.h"
#include <stdio.h>
#include <stdlib.h>

#ifndef _ROSCHMI_TIME_HELPERS_H_
#define _ROSCHMI_TIME_HELPERS_H_

#define SEVENTYYEARS 2208988800UL
#define SECS_PER_MINUTES 60
#define SECS_PER_DAY 86400

#define GMT_MESSAGE "GMT +/- offset"
#define RULE_DST_MESSAGE "no DST rule"
#define RULE_STD_MESSAGE "no STD rule"

enum week_t {Last, First, Second, Third, Fourth}; 
enum dow_t {Sun, Mon, Tue, Wed, Thu, Fri, Sat};
enum month_t {Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec};

class Rs_time_helpers {

    public:
    /**
     * @brief constructor for the Rs_time_helpers object
     * 
     * @param udp 
     */
    Rs_time_helpers();

    /**
     * @brief destructor for the NTP object
     * 
     */
    ~Rs_time_helpers();

    /**
     * @brief Updates time and date with the values sent as parameter.
     *   
     */
    void update(DateTime utcNow);
    void update(uint32_t utcNowSeconds);
    
    /**
     * @brief Has to be called when the rules for DST and STD have been set and UTC is updated.
     *   
     */
    void begin();

    /**
     * @brief set the rule for DST (daylight saving time)
     * start date of DST 
     * 
     * @param tzName name of the time zone
     * @param week Last, First, Second, Third, Fourth (0 - 4)
     * @param wday Sun, Mon, Tue, Wed, Thu, Fri, Sat (0 - 7)
     * @param month Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec (0 -11)
     * @param hour the local hour when rule chages
     * @param tzOffset sum of summertime and timezone offset
     */
    void ruleDST(const char* tzName, int8_t week, int8_t wday, int8_t month, int8_t hour, int tzOffset);

    /**
     * @brief get the DST time as a ctime string
     * 
     * @return char* time string
     */
    const char* ruleDST();

    /**
     * @brief set the rule for STD (standard day)
     * end date of DST
     * 
     * @param tzName name of the time zone
     * @param week Last, First, Second, Third, Fourth (0 - 4)
     * @param wday Sun, Mon, Tue, Wed, Thu, Fri, Sat (0 - 7)
     * @param month Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec (0 -11)
     * @param hour the local hour when rule chages
     * @param tzOffset timezone offset
     */
    void ruleSTD(const char* tzName, int8_t week, int8_t wday, int8_t month, int8_t hour, int tzOffset);

    /**
     * @brief get the STD time as a ctime string
     * 
     * @return char* time string
     */
    const char* ruleSTD();

    /**
     * @brief get the name of the timezone
     * 
     * @return char* name of the timezone
     */
    const char* tzName();

   /**
     * @brief set the timezone manually 
     * this should used if there is no DST!
     * 
     * @param tzHours 
     * @param tzMinutes 
     */
    void timeZone(int8_t tzHours, int8_t tzMinutes = 0);

    /**
     * @brief set daylight saving manually! 
     * use in conjunction with timeZone, when there is no DST!
     * 
     * @param dstZone 
     */
    void isDST(bool dstZone);

    /**
     * @brief returns the DST state
     * 
     * @return int 1 if summertime, 0 no summertime
     */
    bool isDST();


    /**
     * @brief returns a formatted string
     * 
     * @param format for strftime
     * @return char* formated time string
     */
    char* formattedTime(const char *format);

    bool formattedTime(char * outBuffer64Bytes, uint32_t bufferSize, char *format);

    /**
     * @brief for debug purposes you can make an offset
     * 
     * @param days 
     * @param hours 
     * @param minutes 
     * @param seconds 
     */
    void offset(int16_t days, int8_t hours, int8_t minutes, int8_t seconds);

char PROGMEM daysOfTheWeek[7][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
char PROGMEM monthsOfTheYear[12][5] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
char PROGMEM weekOfMonth[5][8] {"Last", "First", "Second", "Third", "Fourth"}; 

void dateTimeToStringFormat_01(char * outBuffer50Bytes, const char *format);

private:
    //UDP *udp;
    //const char* server = "pool.ntp.org";
    //int16_t port = NTP_DEFAULT_LOCAL_PORT;
    //const uint8_t ntpRequest[NTP_PACKET_SIZE] = {0xE3, 0x00, 0x06, 0xEC};
    //uint8_t ntpQuery[NTP_PACKET_SIZE];
    time_t utcCurrent = 0;
    time_t local = 0;
    struct tm *current;
    //uint32_t interval = 60000;
    uint32_t lastUpdate = 0;
    uint8_t tzHours = 0;
    uint8_t tzMinutes = 0;
    int32_t timezoneOffset;
    int16_t dstOffset = 0;
    bool dstZone = true;
    uint32_t utcTime = 0;
    int32_t diffTime;    
    time_t utcSTD, utcDST;
    time_t dstTime, stdTime;
    uint16_t yearDST;
    char timeString[64];
    struct ruleDST {
	    char tzName[6]; // five chars max
	    int8_t week;   // First, Second, Third, Fourth, or Last week of the month
	    int8_t wday;   // day of week, 0 = Sun, 2 = Mon, ... 6 = Sat
	    int8_t month;  // 0 = Jan, 1 = Feb, ... 11=Dec
	    int8_t hour;   // 0 - 23
	    int tzOffset;   // offset from UTC in minutes
      } dstStart, dstEnd;
    //bool ntpUpdate();
    uint32_t localTime();
    void currentTime();   
    void beginDST();
    time_t calcDateDST(struct ruleDST rule, int year);
    bool summerTime();

};

#endif
