// Application AzureDataSender_Wio_Terminal
// Copyright RoSchmi 2020, 2021. License Apache 2.0

// Set WiFi and Azure credentials in file include/config_secret.h  (take config_secret_template.h as template)
// Settings like sendinterval, timezone, daylightsavingstime settings, transport protocol, 
// tablenames and so on are to be defined in /include/config.h


  // I started to make this App from the Azure Storage Blob Example see: 
  // https://github.com/Azure/azure-sdk-for-c/blob/5c7444dfcd5f0b3bcf3aec2f7b62639afc8bd664/sdk/samples/storage/blobs/src/blobs_client_example.c

  // The used Azure-sdk-for-c libraries were Vers. 1.1.0-beta.3
  // https://github.com/Azure/azure-sdk-for-c/releases/tag/1.1.0-beta.3
  // In Vers. 1.1.0-beta.3 was a difficult to find bug in the file az_http_internal.h
  // which is fixed in this repo and in later versions of the Microsoft repo

  
// Memory usage Wio Terminal
// 20000000 	First 64 K Ram Block
// 2000FFFF
// 20010000	  Second 64 K Ram Block
// 2001FFFF
// 20020000	  Third 64 K Ram Block
// 2002FFFF

// Heap goes upwards from about 2000A4A0
// Stack goes downwards from 2000FFFF

// To save available Ram the following memory areas are 'extraordinarily' used as buffers from this program
// 20029000	    300 Bytes PROPERTIES_BUFFER_MEMORY_ADDR
// 20029200     900 Bytes Request Buffer
// 2002A000	    2000 Bytes RESPONSE_BUFFER_MEMORY_ADDR
// 2002FFFF     End of Ram


#include <Arduino.h>
#include "config.h"
#include "config_secret.h"

#include "AzureStorage/CloudStorageAccount.h"
#include "AzureStorage/TableClient.h"
#include "AzureStorage/TableEntityProperty.h"
#include "AzureStorage/TableEntity.h"
#include "AzureStorage/AnalogTableEntity.h"
#include "AzureStorage/OnOffTableEntity.h"

#include "rpcWiFi.h"

#include "SAMCrashMonitor.h"
#include "DateTime.h"
#include <time.h>

#include "Time/SysTime.h"

#include "SensorData/DataContainerWio.h"
#include "SensorData/OnOffDataContainerWio.h"
#include "SensorData/OnOffSwitcherWio.h"
#include "SensorData/ImuManagerWio.h"
#include "SensorData/AnalogSensorMgr.h"

#include "az_wioterminal_roschmi.h"

#include <stdio.h>
#include <stdlib.h>

#include "HTTPClient.h"
#include "DHT.h"
#include"LIS3DHTR.h"

#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"

#include "TFT_eSPI.h"
#include "Free_Fonts.h" 

#include "azure/core/az_platform.h"
#include "azure/core/az_http.h"
#include "azure/core/az_http_transport.h"
#include "azure/core/az_result.h"
#include "azure/core/az_config.h"
#include "azure/core/az_context.h"
#include "azure/core/az_span.h"

//#include <azure/core/az_json.h>
//#include <azure/iot/az_iot_common.h>
//#include <azure/iot/az_iot_hub_client.h>

//#include <azure/core/_az_cfg_prefix.h>   // may not be included

#include "Time/Rs_time_helpers.h"

const char analogTableName[45] = ANALOG_TABLENAME;

const char OnOffTableName_1[45] = ON_OFF_TABLENAME_01;
const char OnOffTableName_2[45] = ON_OFF_TABLENAME_02;
const char OnOffTableName_3[45] = ON_OFF_TABLENAME_03;
const char OnOffTableName_4[45] = ON_OFF_TABLENAME_04;

// The PartitionKey for the analog table may have a prefix to be distinguished, here: "Y2_" 
const char * analogTablePartPrefix = (char *)ANALOG_TABLE_PART_PREFIX;

// The PartitionKey for the On/Off-tables may have a prefix to be distinguished, here: "Y3_" 
const char * onOffTablePartPrefix = (char *)ON_OFF_TABLE_PART_PREFIX;

// The PartitinKey can be augmented with a string representing year and month (recommended)
const bool augmentPartitionKey = true;

// The TableName can be augmented with the actual year (recommended)
const bool augmentTableNameWithYear = true;

// Define Datacontainer with SendInterval and InvalidateInterval as defined in config.h
int sendIntervalSeconds = (SENDINTERVAL_MINUTES * 60) < 1 ? 1 : (SENDINTERVAL_MINUTES * 60);

DataContainerWio dataContainer(TimeSpan(sendIntervalSeconds), TimeSpan(0, 0, INVALIDATEINTERVAL_MINUTES % 60, 0), (float)MIN_DATAVALUE, (float)MAX_DATAVALUE, (float)MAGIC_NUMBER_INVALID);

AnalogSensorMgr analogSensorMgr(MAGIC_NUMBER_INVALID);

OnOffDataContainerWio onOffDataContainer;

OnOffSwitcherWio onOffSwitcherWio;

ImuManagerWio imuManagerWio;

LIS3DHTR<TwoWire> lis;

#define DHTPIN 0
#define DHTTYPE DHT22

// In the original driver the default third parameter (Count) is 25
// This didn't work when the code was compiled in release mode
// I found out by trial that a value of 13 was o.k. for dubug mode and release mode 
DHT dht(DHTPIN, DHTTYPE, 13);

TFT_eSPI tft;
int current_text_line = 0;

#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define LCD_FONT FreeSans9pt7b
#define LCD_LINE_HEIGHT 18

const GFXfont *textFont = FSSB9;

uint32_t screenColor = TFT_BLUE;
uint32_t backColor = TFT_WHITE;
uint32_t textColor = TFT_BLACK;

bool showGraphScreen = SHOW_GRAPHIC_SCREEN == 1;

uint32_t loopCounter = 0;
unsigned int insertCounterAnalogTable = 0;
uint32_t tryUploadCounter = 0;
uint32_t timeNtpUpdateCounter = 0;
volatile int32_t sysTimeNtpDelta = 0;

uint32_t previousNtpUpdateMillis = 0;
uint32_t previousNtpRequestMillis = 0;

volatile uint32_t previousSensorReadMillis = 0;

uint32_t ntpUpdateInterval = 60000;

char timeServer[] = "pool.ntp.org"; // external NTP server e.g. better pool.ntp.org
unsigned int localPort = 2390;      // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

bool ledState = false;
uint8_t lastResetCause = -1;

bool sendResultState = true;

uint32_t failedUploadCounter = 0;

const int timeZoneOffset = (int)TIMEZONEOFFSET;
const int dstOffset = (int)DSTOFFSET;

unsigned long utcTime;
DateTime dateTimeUTCNow;

const char *ssid = IOT_CONFIG_WIFI_SSID;
const char *password = IOT_CONFIG_WIFI_PASSWORD;

WiFiUDP udp;

HTTPClient http;

static HTTPClient * httpPtr = &http;

// must be static !!
static SysTime sysTime;

Rs_time_helpers time_helpers;

typedef const char* X509Certificate;

X509Certificate myX509Certificate = baltimore_root_ca;

// Set transport protocol as defined in config.h
static bool UseHttps_State = TRANSPORT_PROTOCOL == 0 ? false : true;
CloudStorageAccount myCloudStorageAccount(AZURE_CONFIG_ACCOUNT_NAME, AZURE_CONFIG_ACCOUNT_KEY, UseHttps_State);
CloudStorageAccount * myCloudStorageAccountPtr = &myCloudStorageAccount;

// Array to retrieve spaces with different length
char PROGMEM spacesArray[13][13] = {  "", 
                                      " ", 
                                      "  ", 
                                      "   ", 
                                      "    ", 
                                      "     ", 
                                      "      ", 
                                      "       ", 
                                      "        ", 
                                      "         ", 
                                      "          ", 
                                      "           ",  
                                      "            "};



static void button_handler_right() 
{
  int state = digitalRead(BUTTON_1);
  DateTime utcNow = sysTime.getTime();
  time_helpers.update(utcNow);
  int timeZoneOffsetUTC = time_helpers.isDST() ? TIMEZONEOFFSET + DSTOFFSET : TIMEZONEOFFSET;
  onOffDataContainer.SetNewOnOffValue(2, state == 0, utcNow, timeZoneOffsetUTC); 
}

static void button_handler_mid() 
{
  int state = digitalRead(BUTTON_2);
  DateTime utcNow = sysTime.getTime();
  time_helpers.update(utcNow);
  int timeZoneOffsetUTC = time_helpers.isDST() ? TIMEZONEOFFSET + DSTOFFSET : TIMEZONEOFFSET;
  onOffDataContainer.SetNewOnOffValue(1, state == 0, utcNow, timeZoneOffsetUTC);
}

static void button_handler_left() 
{
  int state = digitalRead(BUTTON_3);
  DateTime utcNow = sysTime.getTime();
  time_helpers.update(utcNow);
  int timeZoneOffsetUTC = time_helpers.isDST() ? TIMEZONEOFFSET + DSTOFFSET : TIMEZONEOFFSET;
  onOffDataContainer.SetNewOnOffValue(0, state == 0, utcNow, timeZoneOffsetUTC);
}

// Routine to send messages to the display
void lcd_log_line(char* line) 
{  
  tft.setTextColor(textColor);
  tft.setFreeFont(textFont);
  tft.fillRect(0, current_text_line * LCD_LINE_HEIGHT, 320, LCD_LINE_HEIGHT, backColor);
  tft.drawString(line, 5, current_text_line * LCD_LINE_HEIGHT);
  current_text_line++;
  current_text_line %= ((LCD_HEIGHT-20)/LCD_LINE_HEIGHT);
  if (current_text_line == 0) 
  {
    tft.fillScreen(screenColor);
  }
}

// forward declarations
unsigned long getNTPtime();
unsigned long sendNTPpacket(const char* address);
String floToStr(float value);
float ReadAnalogSensor(int pSensorIndex);
DateTime actualizeSysTimeFromNtp();
void createSampleTime(DateTime dateTimeUTCNow, int timeZoneOffsetUTC, char * sampleTime);
az_http_status_code  createTable(CloudStorageAccount * myCloudStorageAccountPtr, X509Certificate myX509Certificate, const char * tableName);
az_http_status_code CreateTable( const char * tableName, ContType pContentType, AcceptType pAcceptType, ResponseType pResponseType, bool);
az_http_status_code insertTableEntity(CloudStorageAccount *myCloudStorageAccountPtr, X509Certificate pCaCert, const char * pTableName, TableEntity pTableEntity, char * outInsertETag);
void makePartitionKey(const char * partitionKeyprefix, bool augmentWithYear, DateTime dateTime, az_span outSpan, size_t *outSpanLength);
void makeRowKey(DateTime actDate, az_span outSpan, size_t *outSpanLength);
void showDisplayFrame();
void fillDisplayFrame(double an_1, double an_2, double an_3, double an_4, bool on_1,  bool on_2, bool on_3, bool on_4, bool sendResultState, uint32_t tryUpLoadCtr);
int getDayNum(const char * day);
int getMonNum(const char * month);
int getWeekOfMonthNum(const char * weekOfMonth);

// Seems not to work
void myCrashHandler(SAMCrashReport &report)
{
  int pcState = report.pc;
  char buf[100];
  sprintf(buf, "Pc: %i", pcState);
  lcd_log_line(buf);
  SAMCrashMonitor::dumpCrash(report);
};

void setup() 
{ 
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(screenColor);
  tft.setFreeFont(&LCD_FONT);
  tft.setTextColor(TFT_BLACK);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(WIO_LIGHT, INPUT );
  
  Serial.begin(9600);
  //while(!Serial);
  Serial.println("\r\nStarting");
  Serial.println("\r\nStarting");

  pinMode(WIO_5S_PRESS, INPUT_PULLUP);

  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(BUTTON_3, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_1), button_handler_right, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BUTTON_2), button_handler_mid, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BUTTON_3), button_handler_left, CHANGE);

  onOffDataContainer.begin(DateTime(), OnOffTableName_1, OnOffTableName_2, OnOffTableName_3, OnOffTableName_4);
  // Initialize State of 4 On/Off-sensor representations 
  // and of the inverter flags (Application specific)
  for (int i = 0; i < 4; i++)
  {
    onOffDataContainer.PresetOnOffState(i, false, true);
    onOffDataContainer.Set_OutInverter(i, true);
    onOffDataContainer.Set_InputInverter(i, false);
  }
  
  //Initialize OnOffSwitcher (for tests and simulation)
  onOffSwitcherWio.begin(TimeSpan(15 * 60));   // Toggle every 15 min
  onOffSwitcherWio.SetInactive();
  //onOffSwitcherWio.SetActive();

  lcd_log_line((char *)"Start - Disable watchdog.");
  SAMCrashMonitor::begin();

  // Get last ResetCause
  // Ext. Reset: 16
  // WatchDog:   32
  // BySystem:   64
  lastResetCause = SAMCrashMonitor::getResetCause();
  lcd_log_line((char *)SAMCrashMonitor::getResetDescription().c_str());
  SAMCrashMonitor::dump();
  SAMCrashMonitor::disableWatchdog();

  // RoSchmi: Not sure if this works
  SAMCrashMonitor::setUserCrashHandler(myCrashHandler);

  // Logging can be activated here:
  // Seeed_Arduino_rpcUnified/src/rpc_unified_log.h:
  // ( https://forum.seeedstudio.com/t/rpcwifi-library-only-working-intermittently/255660/5 )

  // buffer to hold messages for display
  char buf[100];
  
  sprintf(buf, "RTL8720 Firmware: %s", rpc_system_version());
  lcd_log_line(buf);
  sprintf(buf, "Initial WiFi-Status: %i", (int)WiFi.status());
  lcd_log_line(buf);
  
  // Setting Daylightsavingtime. Enter values for your zone in file include/config.h
  // Program aborts in some cases of invalid values
  bool firstTimeZoneDef_is_Valid = true;
  int dstWeekday = getDayNum(DST_START_WEEKDAY);
  int dstMonth = getMonNum(DST_START_MONTH);
  int dstWeekOfMonth = getWeekOfMonthNum(DST_START_WEEK_OF_MONTH);

  if (!(dstWeekday == -1 || dstMonth == - 1 || dstWeekOfMonth == -1 || DST_START_HOUR > 23 ? true : DST_START_HOUR < 0 ? true : false))
  {
     time_helpers.ruleDST(DST_ON_NAME, dstWeekOfMonth, dstWeekday, dstMonth, DST_START_HOUR, TIMEZONEOFFSET + DSTOFFSET);
     //time_helpers.ruleDST("CEST", Last, Sun, Mar, 2, TIMEZONEOFFSET + DSTOFFSET); // e.g last sunday in march 2:00, timezone +120min (+1 GMT + 1h summertime offset) 
  }
  else
  {
    firstTimeZoneDef_is_Valid = false;
  }

  dstWeekday = getDayNum(DST_STOP_WEEKDAY);
  dstMonth = getMonNum(DST_STOP_MONTH);
  dstWeekOfMonth = getWeekOfMonthNum(DST_STOP_WEEK_OF_MONTH);
  if (!(dstWeekday == -1 || dstMonth == - 1 || dstWeekOfMonth == -1 || DST_STOP_HOUR > 23 ? true : DST_STOP_HOUR < 0 ? true : false || !firstTimeZoneDef_is_Valid))
  {
    time_helpers.ruleSTD(DST_OFF_NAME, dstWeekOfMonth, dstWeekday, dstMonth, DST_STOP_HOUR, TIMEZONEOFFSET);
    //time_helpers.ruleSTD("CET", Last, Sun, Oct, 3, TIMEZONEOFFSET); // e.g. last sunday in october 3:00, timezone +60min (+1 GMT)
    sprintf(buf, "Selected Timezone: %s", DST_OFF_NAME);
     lcd_log_line(buf);
  }
  else
  {
    lcd_log_line((char *)"Invalid Timezone entries, stop");
    while (true)
    {
      delay(1000);
    }
  }

  delay(500);

  //Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  lcd_log_line((char *)"First disconnecting.");
  while (WiFi.status() != WL_DISCONNECTED)
  {
    WiFi.disconnect();
    delay(200); 
  }
  
  sprintf(buf, "Status: %i", (int)WiFi.status());
  lcd_log_line(buf);
  
  delay(500);

  sprintf(buf, "Connecting to SSID: %s", ssid);
  lcd_log_line(buf);

  if (!ssid || *ssid == 0x00 || strlen(ssid) > 31)
  {
    lcd_log_line((char *)"Invalid: SSID or PWD, Stop");
    // Stay in endless loop
      while (true)
      {         
        delay(1000);
      }
  }

#if USE_WIFI_STATIC_IP == 1
  IPAddress presetIp(192, 168, 1, 83);
  IPAddress presetGateWay(192, 168, 1, 1);
  IPAddress presetSubnet(255, 255, 255, 0);
  IPAddress presetDnsServer1(8,8,8,8);
  IPAddress presetDnsServer2(8,8,4,4);
#endif

WiFi.begin(ssid, password);
 
if (!WiFi.enableSTA(true))
{
  #if WORK_WITH_WATCHDOG == 1   
    __unused int timeout = SAMCrashMonitor::enableWatchdog(4000);
  #endif

  while (true)
  {
    // Stay in endless loop to reboot through Watchdog
    lcd_log_line((char *)"Connect failed.");
    delay(1000);
    }
}

#if USE_WIFI_STATIC_IP == 1
  if (!WiFi.config(presetIp, presetGateWay, presetSubnet, presetDnsServer1, presetDnsServer2))
  {
    while (true)
    {
      // Stay in endless loop
    lcd_log_line((char *)"WiFi-Config failed");
      delay(3000);
    }
  }
  else
  {
    lcd_log_line((char *)"WiFi-Config successful");
    delay(1000);
  }
  #endif

  while (WiFi.status() != WL_CONNECTED)
  {  
    delay(1000);
    lcd_log_line(itoa((int)WiFi.status(), buf, 10));
    WiFi.begin(ssid, password);
  }

  lcd_log_line((char *)"Connected, new Status:");
  lcd_log_line(itoa((int)WiFi.status(), buf, 10));

  #if WORK_WITH_WATCHDOG == 1
    
    Serial.println(F("Enabling watchdog."));
    int timeout = SAMCrashMonitor::enableWatchdog(4000);
    sprintf(buf, "Watchdog enabled: %i %s", timeout, "ms");
    lcd_log_line(buf);
  #endif
  
  IPAddress localIpAddress = WiFi.localIP();
  IPAddress gatewayIp =  WiFi.gatewayIP();
  IPAddress subNetMask =  WiFi.subnetMask();
  IPAddress dnsServerIp = WiFi.dnsIP();
   
// Wait for 1500 ms
  for (int i = 0; i < 4; i++)
  {
    delay(500);
    #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();
    #endif
  }
  
  current_text_line = 0;
  tft.fillScreen(screenColor);
    
  lcd_log_line((char *)"> SUCCESS.");
  sprintf(buf, "Ip: %s", (char*)localIpAddress.toString().c_str());
  lcd_log_line(buf);
  sprintf(buf, "Gateway: %s", (char*)gatewayIp.toString().c_str());
  lcd_log_line(buf);
  sprintf(buf, "Subnet: %s", (char*)subNetMask.toString().c_str());
  lcd_log_line(buf);
  sprintf(buf, "DNS-Server: %s", (char*)dnsServerIp.toString().c_str());
  lcd_log_line(buf);
  sprintf(buf, "Protocol: %s", UseHttps_State ? (char *)"https" : (char *)"http");
  lcd_log_line(buf);
  
  ntpUpdateInterval =  (NTP_UPDATE_INTERVAL_MINUTES < 1 ? 1 : NTP_UPDATE_INTERVAL_MINUTES) * 60 * 1000;

  // Set SensorReadInterval for all sensors to value defined in config.h (limited from 1 sec to 4 hours)
  analogSensorMgr.SetReadInterval(ANALOG_SENSOR_READ_INTERVAL_SECONDS < 1 ? 1 : ANALOG_SENSOR_READ_INTERVAL_SECONDS > 14400 ? 14400: ANALOG_SENSOR_READ_INTERVAL_SECONDS);
  
  // Set SensorReadInterval different for sensors index 2 and 3
  analogSensorMgr.SetReadInterval(2, 1);
  analogSensorMgr.SetReadInterval(3, 1);


  #if WORK_WITH_WATCHDOG == 1
    SAMCrashMonitor::iAmAlive();
  #endif

  int getTimeCtr = 0; 
  utcTime = getNTPtime();
  while ((getTimeCtr < 5) && utcTime == 0)
  {   
      getTimeCtr++;
      utcTime = getNTPtime();
  }

  #if WORK_WITH_WATCHDOG == 1
    SAMCrashMonitor::iAmAlive();
  #endif

  if (utcTime != 0 )
  {
    sysTime.begin(utcTime);
    dateTimeUTCNow = utcTime;
  }
  else
  {
    lcd_log_line((char *)"Failed get network time");
    delay(10000);
    // do something, evtl. reboot
    while (true)
    {
      delay(100);
    }   
  }
  
  dateTimeUTCNow = sysTime.getTime();
  
  // RoSchmi for DST tests
  // dateTimeUTCNow = DateTime(2021, 10, 31, 1, 1, 0);


  time_helpers.update(dateTimeUTCNow);
  time_helpers.begin();


  /* RoSchmi for DST tests
  volatile uint64_t utcDstStart = time_helpers.getUtcDST();
  volatile uint64_t utcDstEnd = time_helpers.getUtcSTD();
  volatile uint64_t utcCurrent = time_helpers.getUtcCurrent();
  Serial.print("utcStart: ");
  Serial.println((unsigned long)utcDstStart);
  Serial.print("utcAct:   ");
  Serial.println((unsigned long)utcCurrent);
  Serial.print("utcEnd:   ");
  Serial.println((unsigned long)utcDstEnd);
  volatile bool dstState = time_helpers.isDST();
  */
  

  lcd_log_line((char *)time_helpers.formattedTime("%d. %B %Y"));    // dd. Mmm yyyy
  lcd_log_line((char *)time_helpers.formattedTime("%A %T"));        // Www hh:mm:ss
  
  // Temperature/Humidity Sensor
  dht.begin();

  // Accelerometer
  lis.begin(Wire1);
  lis.setOutputDataRate(LIS3DHTR_DATARATE_25HZ); //Data output rate
  lis.setFullScaleRange(LIS3DHTR_RANGE_2G); //Scale range set to 2g
  
  imuManagerWio.begin();
  imuManagerWio.SetActive();

  // Wait for 1000 ms
  for (int i = 0; i < 3; i++)
  {
    delay(500);
    #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();
    #endif
  }

  // Clear screen
  current_text_line = 0;
  tft.fillScreen(screenColor);
  
  dateTimeUTCNow = sysTime.getTime();
  time_helpers.update(dateTimeUTCNow);

  String augmentedAnalogTableName = analogTableName;  
  if (augmentTableNameWithYear)
  {
    augmentedAnalogTableName += (dateTimeUTCNow.year());
  }
  
  #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();
    #endif

  showDisplayFrame();
  fillDisplayFrame(999.9, 999.9, 999.9, 999.9, false, false, false, false, sendResultState, tryUploadCounter);
  delay(50);

  previousNtpUpdateMillis = millis();
  previousNtpRequestMillis = millis();
}

void loop() 
{ 
  if (loopCounter++ % 10000 == 0)   // Make decisions to send data every 10000 th round and toggle Led to signal that App is running
  {
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
    
    #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();
    #endif

    
    // Update RTC from Ntp when ntpUpdateInterval has expired, retry after 20 sec if update was not successful
    if (((millis() - previousNtpUpdateMillis) >= ntpUpdateInterval) && ((millis() - previousNtpRequestMillis) >= 20000))  
    {      
        dateTimeUTCNow = sysTime.getTime();
        uint32_t actRtcTime = dateTimeUTCNow.secondstime();

        int loopCtr = 0;
        unsigned long  utcNtpTime = getNTPtime();   // Get NTP time, try up to 4 times        
        while ((loopCtr < 4) && utcTime == 0)
        {
          loopCtr++;
          utcTime = getNTPtime();
        }

        previousNtpRequestMillis = millis();
        
        if (utcNtpTime != 0 )       // if NTP request was successful --> synchronize RTC 
        {  
            previousNtpUpdateMillis = millis();     
            dateTimeUTCNow = utcNtpTime;
            sysTimeNtpDelta = actRtcTime - dateTimeUTCNow.secondstime();
            
            if (!showGraphScreen)
            { 
              char buffer[] = "NTP-Utc: YYYY-MM-DD hh:mm:ss";           
              dateTimeUTCNow.toString(buffer);
              lcd_log_line((char *)buffer);
            } 
            
            sysTime.setTime(dateTimeUTCNow);
            timeNtpUpdateCounter++;   
        }  
    }
    else            // it was not NTP Update, proceed with send to analog table or On/Off-table
    {
      dateTimeUTCNow = sysTime.getTime();
      
      // Get offset in minutes between UTC and local time with consideration of DST
      time_helpers.update(dateTimeUTCNow);
      int timeZoneOffsetUTC = time_helpers.isDST() ? TIMEZONEOFFSET + DSTOFFSET : TIMEZONEOFFSET;
      DateTime localTime = dateTimeUTCNow.operator+(TimeSpan(timeZoneOffsetUTC * 60));
      
      // In the last 15 sec of each day we set a pulse to Off-State when we had On-State before
      bool isLast15SecondsOfDay = (localTime.hour() == 23 && localTime.minute() == 59 &&  localTime.second() > 45) ? true : false;
      
      // Get readings from 4 differend analog sensors and store the values in a container
      dataContainer.SetNewValue(0, dateTimeUTCNow, ReadAnalogSensor(0));
      dataContainer.SetNewValue(1, dateTimeUTCNow, ReadAnalogSensor(1));
      dataContainer.SetNewValue(2, dateTimeUTCNow, ReadAnalogSensor(2));
      dataContainer.SetNewValue(3, dateTimeUTCNow, ReadAnalogSensor(3));

      // Check if automatic OnOfSwitcher has toggled (used to simulate on/off changes)
      // and accordingly change the state of one representation (here index 2) in onOffDataContainer
      if (onOffSwitcherWio.hasToggled(dateTimeUTCNow))
      {
        bool state = onOffSwitcherWio.GetState();
        time_helpers.update(dateTimeUTCNow);
        int timeZoneOffsetUTC = time_helpers.isDST() ? TIMEZONEOFFSET + DSTOFFSET : TIMEZONEOFFSET;
        onOffDataContainer.SetNewOnOffValue(0, state, dateTimeUTCNow, timeZoneOffsetUTC);
        onOffDataContainer.SetNewOnOffValue(1, !state, dateTimeUTCNow, timeZoneOffsetUTC);
      }
      
      // Check if something is to do: send analog data ? send On/Off-Data ? Handle EndOfDay stuff ?
      if (dataContainer.hasToBeSent() || onOffDataContainer.One_hasToBeBeSent(localTime) || isLast15SecondsOfDay)
      {    
        //Create some buffer
        char sampleTime[25] {0};    // Buffer to hold sampletime        
        char strData[100];          // Buffer to hold display message  
        char EtagBuffer[50] {0};    // Buffer to hold returned Etag

        // Create az_span to hold partitionkey
        char partKeySpan[25] {0};
        size_t partitionKeyLength = 0;
        az_span partitionKey = AZ_SPAN_FROM_BUFFER(partKeySpan);
        
        // Create az_span to hold rowkey
        char rowKeySpan[25] {0};
        size_t rowKeyLength = 0;
        az_span rowKey = AZ_SPAN_FROM_BUFFER(rowKeySpan);

        if (dataContainer.hasToBeSent())       // have to send analog values ?
        {
          // Retrieve edited sample values from container
          SampleValueSet sampleValueSet = dataContainer.getCheckedSampleValues(dateTimeUTCNow);
                  
          createSampleTime(sampleValueSet.LastUpdateTime, timeZoneOffsetUTC, (char *)sampleTime);

          // Define name of the table (arbitrary name + actual year, like: AnalogTestValues2020)
          String augmentedAnalogTableName = analogTableName; 
          if (augmentTableNameWithYear)
          {
            augmentedAnalogTableName += (dateTimeUTCNow.year());     
          }

          // Create table if table doesn't exist
          if (localTime.year() != dataContainer.Year)
          {
            az_http_status_code theResult = createTable(myCloudStorageAccountPtr, myX509Certificate, (char *)augmentedAnalogTableName.c_str());
                 
            if ((theResult == AZ_HTTP_STATUS_CODE_CONFLICT) || (theResult == AZ_HTTP_STATUS_CODE_CREATED))
            {
              dataContainer.Set_Year(localTime.year());                   
            }
            else
            {
              // Reset board if not successful
              NVIC_SystemReset();     // ResetCause Code 64
            }
          }


          // Create an Array of (here) 5 Properties
          // Each Property consists of the Name, the Value and the Type (here only Edm.String is supported)

          // Besides PartitionKey and RowKey we have 5 properties to be stored in a table row
          // (SampleTime and 4 samplevalues)
          size_t analogPropertyCount = 5;
          EntityProperty AnalogPropertiesArray[5];
          AnalogPropertiesArray[0] = (EntityProperty)TableEntityProperty((char *)"SampleTime", (char *) sampleTime, (char *)"Edm.String");
          AnalogPropertiesArray[1] = (EntityProperty)TableEntityProperty((char *)"T_1", (char *)floToStr(sampleValueSet.SampleValues[0].Value).c_str(), (char *)"Edm.String");
          AnalogPropertiesArray[2] = (EntityProperty)TableEntityProperty((char *)"T_2", (char *)floToStr(sampleValueSet.SampleValues[1].Value).c_str(), (char *)"Edm.String");
          AnalogPropertiesArray[3] = (EntityProperty)TableEntityProperty((char *)"T_3", (char *)floToStr(sampleValueSet.SampleValues[2].Value).c_str(), (char *)"Edm.String");
          AnalogPropertiesArray[4] = (EntityProperty)TableEntityProperty((char *)"T_4", (char *)floToStr(sampleValueSet.SampleValues[3].Value).c_str(), (char *)"Edm.String");
  
          // Create the PartitionKey (special format)
          makePartitionKey(analogTablePartPrefix, augmentPartitionKey, localTime, partitionKey, &partitionKeyLength);
          partitionKey = az_span_slice(partitionKey, 0, partitionKeyLength);

          // Create the RowKey (special format)        
          makeRowKey(localTime, rowKey, &rowKeyLength);
          
          rowKey = az_span_slice(rowKey, 0, rowKeyLength);
  
          // Create TableEntity consisting of PartitionKey, RowKey and the properties named 'SampleTime', 'T_1', 'T_2', 'T_3' and 'T_4'
          AnalogTableEntity analogTableEntity(partitionKey, rowKey, az_span_create_from_str((char *)sampleTime),  AnalogPropertiesArray, analogPropertyCount);
     
          // Print message on display
          if (!showGraphScreen)
          {      
            sprintf(strData, "   Trying to insert %u", insertCounterAnalogTable);   
            lcd_log_line(strData);
          }
             
          // Keep track of tries to insert and check for memory leak
          insertCounterAnalogTable++;

          // RoSchmi, Todo: event. include code to check for memory leaks here


          // Store Entity to Azure Cloud   
         __unused az_http_status_code insertResult =  insertTableEntity(myCloudStorageAccountPtr, myX509Certificate, (char *)augmentedAnalogTableName.c_str(), analogTableEntity, (char *)EtagBuffer);

        }
        else     // Task to do was not NTP and not send analog table, so it is Send On/Off values or End of day stuff?
        {
          OnOffSampleValueSet onOffValueSet = onOffDataContainer.GetOnOffValueSet();
        
          for (int i = 0; i < 4; i++)    // Do for 4 OnOff-Tables
          {
            DateTime lastSwitchTimeDate = DateTime( onOffValueSet.OnOffSampleValues[i].LastSwitchTime.year(), 
                                                onOffValueSet.OnOffSampleValues[i].LastSwitchTime.month(), 
                                                onOffValueSet.OnOffSampleValues[i].LastSwitchTime.day());

            DateTime actTimeDate = DateTime(localTime.year(), localTime.month(), localTime.day());

            if (onOffValueSet.OnOffSampleValues[i].hasToBeSent || ((onOffValueSet.OnOffSampleValues[i].actState == true) &&  (lastSwitchTimeDate.operator!=(actTimeDate))))
            {
              onOffDataContainer.Reset_hasToBeSent(i);     
              EntityProperty OnOffPropertiesArray[5];

               // RoSchmi
               TimeSpan  onTime = onOffValueSet.OnOffSampleValues[i].OnTimeDay;
               if (lastSwitchTimeDate.operator!=(actTimeDate))
               {
                  onTime = TimeSpan(0);                 
                  onOffDataContainer.Set_OnTimeDay(i, onTime);

                  if (onOffValueSet.OnOffSampleValues[i].actState == true)
                  {
                    onOffDataContainer.Set_LastSwitchTime(i, actTimeDate);
                  }
               }
                          
              char OnTimeDay[15] = {0};
              sprintf(OnTimeDay, "%03i-%02i:%02i:%02i", onTime.days(), onTime.hours(), onTime.minutes(), onTime.seconds());
              createSampleTime(dateTimeUTCNow, timeZoneOffsetUTC, (char *)sampleTime);

              // Tablenames come from the onOffValueSet, here usually the tablename is augmented with the actual year
              String augmentedOnOffTableName = onOffValueSet.OnOffSampleValues[i].tableName;
              if (augmentTableNameWithYear)
              {               
                augmentedOnOffTableName += (localTime.year()); 
              }

              // Create table if table doesn't exist
              if (localTime.year() != onOffValueSet.OnOffSampleValues[i].Year)
              {
                 az_http_status_code theResult = createTable(myCloudStorageAccountPtr, myX509Certificate, (char *)augmentedOnOffTableName.c_str());
                 
                 if ((theResult == AZ_HTTP_STATUS_CODE_CONFLICT) || (theResult == AZ_HTTP_STATUS_CODE_CREATED))
                 {
                    onOffDataContainer.Set_Year(i, localTime.year());
                 }
                 else
                 {
                    NVIC_SystemReset();     // Makes Code 64
                 }
              }
              
              TimeSpan TimeFromLast = onOffValueSet.OnOffSampleValues[i].TimeFromLast;



              char timefromLast[15] = {0};
              sprintf(timefromLast, "%03i-%02i:%02i:%02i", TimeFromLast.days(), TimeFromLast.hours(), TimeFromLast.minutes(), TimeFromLast.seconds());
                         
              size_t onOffPropertyCount = 5;
              OnOffPropertiesArray[0] = (EntityProperty)TableEntityProperty((char *)"ActStatus", onOffValueSet.OnOffSampleValues[i].outInverter ? (char *)(onOffValueSet.OnOffSampleValues[i].actState ? "On" : "Off") : (char *)(onOffValueSet.OnOffSampleValues[i].actState ? "Off" : "On"), (char *)"Edm.String");
              OnOffPropertiesArray[1] = (EntityProperty)TableEntityProperty((char *)"LastStatus", onOffValueSet.OnOffSampleValues[i].outInverter ? (char *)(onOffValueSet.OnOffSampleValues[i].lastState ? "On" : "Off") : (char *)(onOffValueSet.OnOffSampleValues[i].lastState ? "Off" : "On"), (char *)"Edm.String");
              OnOffPropertiesArray[2] = (EntityProperty)TableEntityProperty((char *)"OnTimeDay", (char *) OnTimeDay, (char *)"Edm.String");
              OnOffPropertiesArray[3] = (EntityProperty)TableEntityProperty((char *)"SampleTime", (char *) sampleTime, (char *)"Edm.String");
              OnOffPropertiesArray[4] = (EntityProperty)TableEntityProperty((char *)"TimeFromLast", (char *) timefromLast, (char *)"Edm.String");
          
              // Create the PartitionKey (special format)
              makePartitionKey(onOffTablePartPrefix, augmentPartitionKey, localTime, partitionKey, &partitionKeyLength);
              partitionKey = az_span_slice(partitionKey, 0, partitionKeyLength);
              
              // Create the RowKey (special format)            
              makeRowKey(localTime, rowKey, &rowKeyLength);
              
              rowKey = az_span_slice(rowKey, 0, rowKeyLength);
  
              // Create TableEntity consisting of PartitionKey, RowKey and the properties named 'SampleTime', 'T_1', 'T_2', 'T_3' and 'T_4'
              OnOffTableEntity onOffTableEntity(partitionKey, rowKey, az_span_create_from_str((char *)sampleTime),  OnOffPropertiesArray, onOffPropertyCount);
          
              onOffValueSet.OnOffSampleValues[i].insertCounter++;
              
              // Store Entity to Azure Cloud   
              __unused az_http_status_code insertResult =  insertTableEntity(myCloudStorageAccountPtr, myX509Certificate, (char *)augmentedOnOffTableName.c_str(), onOffTableEntity, (char *)EtagBuffer);
              
              delay(1000);     // wait at least 1 sec so that two uploads cannot have the same RowKey
              break;          // Send only one in each round of loop 
            }
            else
            {
              if (isLast15SecondsOfDay && !onOffValueSet.OnOffSampleValues[i].dayIsLocked)
              {
                if (onOffValueSet.OnOffSampleValues[i].actState == true)              
                {               
                   onOffDataContainer.Set_ResetToOnIsNeededFlag(i, true);                 
                   onOffDataContainer.SetNewOnOffValue(i, onOffValueSet.OnOffSampleValues[i].inputInverter ? true : false, dateTimeUTCNow, timeZoneOffsetUTC);
                   delay(1000);   // because we don't want to send twice in the same second 
                  break;
                }
                else
                {              
                  if (onOffValueSet.OnOffSampleValues[i].resetToOnIsNeeded)
                  {                  
                    onOffDataContainer.Set_DayIsLockedFlag(i, true);
                    onOffDataContainer.Set_ResetToOnIsNeededFlag(i, false);
                    onOffDataContainer.SetNewOnOffValue(i, onOffValueSet.OnOffSampleValues[i].inputInverter ? false : true, dateTimeUTCNow, timeZoneOffsetUTC);
                    break;
                  }                 
                }              
              }
            }              
          }
        } 
      }    
    }
    if (digitalRead(WIO_5S_PRESS) == LOW)    // Toggle between graphics screen and Log screen
    {
      if (showGraphScreen)
      {
        textColor = TFT_BLACK;
        screenColor = TFT_WHITE;
        backColor = TFT_WHITE;        
        textFont =  FSSB9;          
        tft.fillScreen(TFT_WHITE);
        current_text_line = 0;
        lcd_log_line((char *)"Log:");
        showGraphScreen = !showGraphScreen;
      }
      else
      {
        showGraphScreen = !showGraphScreen;
        showDisplayFrame();
      }
      // Wait until button released
      while(digitalRead(WIO_5S_PRESS) == LOW);   
    }

    if (showGraphScreen)
    {
      fillDisplayFrame(dataContainer.SampleValues[0].Value, dataContainer.SampleValues[1].Value,
                      dataContainer.SampleValues[2].Value, dataContainer.SampleValues[3].Value,
                      onOffDataContainer.ReadOnOffState(0), onOffDataContainer.ReadOnOffState(1),
                      onOffDataContainer.ReadOnOffState(2), onOffDataContainer.ReadOnOffState(3),
                      sendResultState, tryUploadCounter);
    }

  }
  loopCounter++;
}                      // End loop

void showDisplayFrame()
{
  if (showGraphScreen)
  {
    tft.fillScreen(TFT_BLUE);
    backColor = TFT_LIGHTGREY;
    textFont = FSSB9;
    textColor = TFT_BLACK;
    
    char line[35]{0};
    char label_left[15] {0};
    strncpy(label_left, ANALOG_SENSOR_01_LABEL, 13);
    char label_right[15] {0};
    strncpy(label_right, ANALOG_SENSOR_02_LABEL, 13);
    int32_t gapLength_1 = (13 - strlen(label_left)) / 2;
    int32_t gapLength_2 = (13 - strlen(label_right)) / 2; 
    sprintf(line, "%s%s%s%s%s%s%s ", spacesArray[3], spacesArray[(int)(gapLength_1 * 1.7)], label_left, spacesArray[(int)(gapLength_1 * 1.7)], spacesArray[5], spacesArray[(int)(gapLength_2 * 1.7)], label_right);
    current_text_line = 1;
    lcd_log_line((char *)line);

    strncpy(label_left, ANALOG_SENSOR_03_LABEL, 13); 
    strncpy(label_right, ANALOG_SENSOR_04_LABEL, 13);
    gapLength_1 = (13 - strlen(label_left)) / 2; 
    gapLength_2 = (13 - strlen(label_right)) / 2;
    sprintf(line, "%s%s%s%s%s%s%s ", spacesArray[3], spacesArray[(int)(gapLength_1 * 1.7)], label_left, spacesArray[(int)(gapLength_1 * 1.7)], spacesArray[5], spacesArray[(int)(gapLength_2 * 1.7)], label_right);
    current_text_line = 6;
    lcd_log_line((char *)line);
    current_text_line = 10;   
    line[0] = '\0';   
    lcd_log_line((char *)line);
  }
}

void fillDisplayFrame(double an_1, double an_2, double an_3, double an_4, bool on_1,  bool on_2, bool on_3, bool on_4, bool pSendResultState, uint32_t pTryUploadCtr)
{
  if (showGraphScreen)
  {
    static TFT_eSprite spr = TFT_eSprite(&tft);

    static uint8_t lastDateOutputMinute = 60;

    static uint32_t lastTryUploadCtr = 0;

    static bool lastSendResultState = false;
    
    an_1 = constrain(an_1, -999.9, 999.9);
    an_2 = constrain(an_2, -999.9, 999.9);
    an_3 = constrain(an_3, -999.9, 999.9);
    an_4 = constrain(an_4, -999.9, 999.9);

    char lineBuf[40] {0};

    char valueStringArray[4][8] = {{0}, {0}, {0}, {0}};

    sprintf(valueStringArray[0], "%.1f", SENSOR_1_FAHRENHEIT == 1 ?  dht.convertCtoF(an_1) :  an_1 );
    sprintf(valueStringArray[1], "%.1f", SENSOR_2_FAHRENHEIT == 1 ?  dht.convertCtoF(an_2) :  an_2 );
    sprintf(valueStringArray[2], "%.1f", SENSOR_3_FAHRENHEIT == 1 ?  dht.convertCtoF(an_3) :  an_3 );
    sprintf(valueStringArray[3], "%.1f", SENSOR_4_FAHRENHEIT == 1 ?  dht.convertCtoF(an_4) :  an_4 );

    for (int i = 0; i < 4; i++)
    {
        // 999.9 is invalid, 1831.8 is invalid when tempatures are expressed in Fahrenheit
        
        if (strcmp(valueStringArray[i], "999.9") == 0 || strcmp(valueStringArray[i], "1831.8") == 0)
        {
            strcpy(valueStringArray[i], "--.-");
        }
        
    }

    int charCounts[4];
    charCounts[0] = 6 - strlen(valueStringArray[0]);
    charCounts[1] = 6 - strlen(valueStringArray[1]);
    charCounts[2] = 6 - strlen(valueStringArray[2]);
    charCounts[3] = 6 - strlen(valueStringArray[3]);
    
    spr.createSprite(120, 30);

    spr.setTextColor(TFT_ORANGE);
    spr.setFreeFont(FSSBO18);
    
    for (int i = 0; i <4; i++)
    {
      spr.fillSprite(TFT_DARKGREEN);
      sprintf(lineBuf, "%s%s", spacesArray[charCounts[i]], valueStringArray[i]);
      spr.drawString(lineBuf, 0, 0);
      switch (i)
      {
        case 0: {spr.pushSprite(25, 54); break;}
        case 1: {spr.pushSprite(160, 54); break;}
        case 2: {spr.pushSprite(25, 138); break;}
        case 3: {spr.pushSprite(160, 138); break;}
      }     
    }
    
    dateTimeUTCNow = sysTime.getTime();
    time_helpers.update(dateTimeUTCNow);
    int timeZoneOffsetUTC = time_helpers.isDST() ? TIMEZONEOFFSET + DSTOFFSET : TIMEZONEOFFSET;
    
    DateTime localTime = dateTimeUTCNow.operator+(TimeSpan(timeZoneOffsetUTC * 60));
    
    char formattedTime[64] {0};

    time_helpers.formattedTime(formattedTime, sizeof(formattedTime), (char *)"%d. %b %Y - %A %T");
    
    volatile uint8_t actMinute = localTime.minute();
    if (lastDateOutputMinute != actMinute)
    {
      lastDateOutputMinute = actMinute;     
       current_text_line = 10;
       sprintf(lineBuf, "%s", (char *)formattedTime);
       lineBuf[strlen(lineBuf) -3] = (char)'\0';
       lcd_log_line(lineBuf);
    }

    // Show signal circle on the screen, showing if upload was successfful (green) or not (red)
    if (pTryUploadCtr != lastTryUploadCtr)    // if new upload try has happened, show actualized signal 
    {
        if (pSendResultState)
        {
          tft.fillCircle(300, 9, 8, TFT_DARKGREEN);
          lastSendResultState = true;          
        }
        else
        {
          tft.fillCircle(300, 9, 8, TFT_RED);
          lastSendResultState = false;         
        }       
        lastTryUploadCtr = pTryUploadCtr;
    }
    else                                  // if no upload try --> switch off if it was green before
    {
        if (lastSendResultState == true)
        {
          tft.fillCircle(300, 9, 8, TFT_BLUE);
        }
    }
  
    tft.fillRect(16, 12 * LCD_LINE_HEIGHT, 60, LCD_LINE_HEIGHT, on_1 ? TFT_RED : TFT_DARKGREY);
    tft.fillRect(92, 12 * LCD_LINE_HEIGHT, 60, LCD_LINE_HEIGHT, on_2 ? TFT_RED : TFT_DARKGREY);
    tft.fillRect(168, 12 * LCD_LINE_HEIGHT, 60, LCD_LINE_HEIGHT, on_3 ? TFT_RED : TFT_DARKGREY);
    tft.fillRect(244, 12 * LCD_LINE_HEIGHT, 60, LCD_LINE_HEIGHT, on_4 ? TFT_RED : TFT_DARKGREY);
  }
}

// To manage daylightsavingstime stuff convert input ("Last", "First", "Second", "Third", "Fourth") to int equivalent
int getWeekOfMonthNum(const char * weekOfMonth)
{
  for (int i = 0; i < 5; i++)
  {  
    if (strcmp((char *)time_helpers.weekOfMonth[i], weekOfMonth) == 0)
    {
      return i;
    }   
  }
  return -1;
}

int getMonNum(const char * month)
{
  for (int i = 0; i < 12; i++)
  {  
    if (strcmp((char *)time_helpers.monthsOfTheYear[i], month) == 0)
    {
      // RoSchmi
      return i;
      //return i + 1;
    }   
  }
  return -1;
}

int getDayNum(const char * day)
{
  for (int i = 0; i < 7; i++)
  {  
    if (strcmp((char *)time_helpers.daysOfTheWeek[i], day) == 0)
    {
      // RoSchmi
      return i;
      //return i + 1;
    }   
  }
  return -1;
}

unsigned long getNTPtime() 
{
    // module returns a unsigned long time valus as secs since Jan 1, 1970 
    // unix time or 0 if a problem encounted
    //only send data when connected
    if (WiFi.status() == WL_CONNECTED) 
    {
        //initializes the UDP state
        //This initializes the transfer buffer
        udp.begin(WiFi.localIP(), localPort);
 
        sendNTPpacket(timeServer); // send an NTP packet to a time server
        // wait to see if a reply is available
     
        delay(1000);
        
        if (udp.parsePacket()) 
        {
            // We've received a packet, read the data from it
            udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
 
            //the timestamp starts at byte 40 of the received packet and is four bytes,
            // or two words, long. First, extract the two words:
 
            unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
            unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
            // combine the four bytes (two words) into a long integer
            // this is NTP time (seconds since Jan 1 1900):
            unsigned long secsSince1900 = highWord << 16 | lowWord;
            // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
            const unsigned long seventyYears = 2208988800UL;
            // subtract seventy years:
            unsigned long epoch = secsSince1900 - seventyYears;
 
            // adjust time for timezone offset in secs +/- from UTC
            // WA time offset from UTC is +8 hours (28,800 secs)
            // + East of GMT
            // - West of GMT

            // RoSchmi: inactivated timezone offset
            // long tzOffset = 28800UL;
            long tzOffset = 0UL;
         
            unsigned long adjustedTime;
            return adjustedTime = epoch + tzOffset;
        }
        else {
            // were not able to parse the udp packet successfully
            // clear down the udp connection
            udp.stop();
            return 0; // zero indicates a failure
        }
        // not calling ntp time frequently, stop releases resources
        udp.stop();
    }
    else 
    {
        // network not connected
        return 0;
    }
 
}
 
// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(const char* address) {
    // set all bytes in the buffer to 0
    for (int i = 0; i < NTP_PACKET_SIZE; ++i) {
        packetBuffer[i] = 0;
    }
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;
 
    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    udp.beginPacket(address, 123); //NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
    return 0;
}

String floToStr(float value)
{
  char buf[10];
  sprintf(buf, "%.1f", (roundf(value * 10.0))/10.0);
  return String(buf);
}

float ReadAnalogSensor(int pSensorIndex)
{
#ifndef USE_SIMULATED_SENSORVALUES
            // Use values read from an analog source
            // Change the function for each sensor to your needs

            double theRead = MAGIC_NUMBER_INVALID;

            if (analogSensorMgr.HasToBeRead(pSensorIndex, dateTimeUTCNow))
            {                     
              switch (pSensorIndex)
              {
                case 0:
                    {
                        float temp_hum_val[2] = {0};
                        if (!dht.readTempAndHumidity(temp_hum_val))
                        {
                            analogSensorMgr.SetReadTimeAndValues(pSensorIndex, dateTimeUTCNow, temp_hum_val[1], temp_hum_val[0], MAGIC_NUMBER_INVALID);
                            
                            theRead = temp_hum_val[1];
                            // Take theRead (nearly) 0.0 as invalid
                            // (if no sensor is connected the function returns 0)                        
                            if (!(theRead > - 0.00001 && theRead < 0.00001))
                            {      
                                theRead += SENSOR_1_OFFSET;                                                       
                            }
                            else
                            {
                              theRead = MAGIC_NUMBER_INVALID;
                            }                            
                        }                                                           
                    }
                    break;

                case 1:
                    {
                      // Here we look if the temperature sensor was updated in this loop
                      // If yes, we can get the measured humidity value from the index 0 sensor
                      AnalogSensor tempSensor = analogSensorMgr.GetSensorDates(0);
                      if (tempSensor.LastReadTime.operator==(dateTimeUTCNow))
                      {
                          analogSensorMgr.SetReadTimeAndValues(pSensorIndex, dateTimeUTCNow, tempSensor.Value_1, tempSensor.Value_2, MAGIC_NUMBER_INVALID);
                          theRead = tempSensor.Value_2;
                            // Take theRead (nearly) 0.0 as invalid
                            // (if no sensor is connected the function returns 0)                        
                            if (!(theRead > - 0.00001 && theRead < 0.00001))
                            {      
                                theRead += SENSOR_2_OFFSET;                                                       
                            }
                            else
                            {
                              theRead = MAGIC_NUMBER_INVALID;
                            }                          
                      }                
                    }
                    break;
                case 2:
                    {
                        // Here we do not send a sensor value but the state of the upload counter
                        // Upload counter, limited to max. value of 1399
                        //theRead = (insertCounterAnalogTable % 1399) / 10.0 ;

                        // Alternative                  
                        
                        // Read the light sensor (not used here, collumn is used as upload counter)
                        theRead = analogRead(WIO_LIGHT);
                        theRead = map(theRead, 0, 1023, 0, 100);
                        theRead = theRead < 0 ? 0 : theRead > 100 ? 100 : theRead;
                                                                    
                    }
                    break;
                case 3:
                    /*                
                    {
                        // Here we do not send a sensor value but the last reset cause
                        // Read the last reset cause for dignostic purpose 
                        theRead = lastResetCause;                        
                    }
                    */


                    // Read the accelerometer (not used here)
                    // First experiments, don't work well
                    
                    {
                        ImuSampleValues sampleValues;
                        sampleValues.X_Read = lis.getAccelerationX();
                        sampleValues.Y_Read = lis.getAccelerationY();
                        sampleValues.Z_Read = lis.getAccelerationZ();
                        imuManagerWio.SetNewImuReadings(sampleValues);

                        theRead = imuManagerWio.GetVibrationValue();                                                                 
                    } 
                    
                    break;
              }
            }          
            return theRead ;
#endif

#ifdef USE_SIMULATED_SENSORVALUES
      #ifdef USE_TEST_VALUES
            // Here you can select that diagnostic values (for debugging)
            // are sent to your storage table
            double theRead = MAGIC_NUMBER_INVALID;
            switch (pSensorIndex)
            {
                case 0:
                    {
                        theRead = timeNtpUpdateCounter;
                        theRead = theRead / 10; 
                    }
                    break;

                case 1:
                    {                       
                        theRead = sysTimeNtpDelta > 140 ? 140 : sysTimeNtpDelta < - 40 ? -40 : (double)sysTimeNtpDelta;                      
                    }
                    break;
                case 2:
                    {
                        theRead = insertCounterAnalogTable;
                        theRead = theRead / 10;                      
                    }
                    break;
                case 3:
                    {
                        theRead = lastResetCause;                       
                    }
                    break;
            }

            return theRead ;

  

        #endif
            
            onOffSwitcherWio.SetActive();
            // Only as an example we here return values which draw a sinus curve            
            int frequDeterminer = 4;
            int y_offset = 1;
            // different frequency and y_offset for aIn_0 to aIn_3
            if (pSensorIndex == 0)
            { frequDeterminer = 4; y_offset = 1; }
            if (pSensorIndex == 1)
            { frequDeterminer = 8; y_offset = 10; }
            if (pSensorIndex == 2)
            { frequDeterminer = 12; y_offset = 20; }
            if (pSensorIndex == 3)
            { frequDeterminer = 16; y_offset = 30; }
             
            int secondsOnDayElapsed = dateTimeUTCNow.second() + dateTimeUTCNow.minute() * 60 + dateTimeUTCNow.hour() *60 *60;

            switch (pSensorIndex)
            {
              case 3:
              {
                return lastResetCause;
              }
              break;
            
              case 2:
              { 
                uint32_t showInsertCounter = insertCounterAnalogTable % 50;               
                double theRead = ((double)showInsertCounter) / 10;
                return theRead;
              }
              break;
              case 0:
              case 1:
              {
                return roundf((float)25.0 * (float)sin(PI / 2.0 + (secondsOnDayElapsed * ((frequDeterminer * PI) / (float)86400)))) / 10  + y_offset;          
              }
              break;
              default:
              {
                return 0;
              }
            }      
            

  #endif
}

void createSampleTime(DateTime dateTimeUTCNow, int timeZoneOffsetUTC, char * sampleTime)
{
  int hoursOffset = timeZoneOffsetUTC / 60;
  int minutesOffset = timeZoneOffsetUTC % 60;
  char sign = timeZoneOffsetUTC < 0 ? '-' : '+';
  char TimeOffsetUTCString[10];
  sprintf(TimeOffsetUTCString, " %c%03i", sign, timeZoneOffsetUTC);
  TimeSpan timespanOffsetToUTC = TimeSpan(0, hoursOffset, minutesOffset, 0);
  DateTime newDateTime = dateTimeUTCNow + timespanOffsetToUTC;
  sprintf(sampleTime, "%02i/%02i/%04i %02i:%02i:%02i%s",newDateTime.month(), newDateTime.day(), newDateTime.year(), newDateTime.hour(), newDateTime.minute(), newDateTime.second(), TimeOffsetUTCString);
}
 
void makeRowKey(DateTime actDate,  az_span outSpan, size_t *outSpanLength)
{
  // formatting the RowKey (= reverseDate) this way to have the tables sorted with last added row upmost
  char rowKeyBuf[20] {0};

  sprintf(rowKeyBuf, "%4i%02i%02i%02i%02i%02i", (10000 - actDate.year()), (12 - actDate.month()), (31 - actDate.day()), (23 - actDate.hour()), (59 - actDate.minute()), (59 - actDate.second()));
  az_span retValue = az_span_create_from_str((char *)rowKeyBuf);
  az_span_copy(outSpan, retValue);
  *outSpanLength = retValue._internal.size;         
}

void makePartitionKey(const char * partitionKeyprefix, bool augmentWithYear, DateTime dateTime, az_span outSpan, size_t *outSpanLength)
{
  // if wanted, augment with year and month (12 - month for right order)                    
  char dateBuf[20] {0};
  sprintf(dateBuf, "%s%d-%02d", partitionKeyprefix, (dateTime.year()), (12 - dateTime.month()));                  
  az_span ret_1 = az_span_create_from_str((char *)dateBuf);
  az_span ret_2 = az_span_create_from_str((char *)partitionKeyprefix);                       
  if (augmentWithYear == true)
  {
    az_span_copy(outSpan, ret_1);            
    *outSpanLength = ret_1._internal.size; 
  }
    else
  {
    az_span_copy(outSpan, ret_2);
    *outSpanLength = ret_2._internal.size;
  }    
}


az_http_status_code insertTableEntity(CloudStorageAccount *pAccountPtr, X509Certificate pCaCert, const char * pTableName, TableEntity pTableEntity, char * outInsertETag)
{ 
  #if TRANSPORT_PROTOCOL == 1
    static WiFiClientSecure wifi_client;
  #else
    static WiFiClient wifi_client;
  #endif
  
  
  #if TRANSPORT_PROTOCOL == 1
    wifi_client.setCACert(myX509Certificate);
    //wifi_client.setCACert(baltimore_corrupt_root_ca);
  #endif
  

  /*
  // For tests: Try second upload with corrupted certificate to provoke failure
  #if TRANSPORT_PROTOCOL == 1
    wifi_client.setCACert(myX509Certificate);
    if (insertCounterAnalogTable == 2)
    {
      wifi_client.setCACert(baltimore_corrupt_root_ca);
    }
  #endif
  */

  TableClient table(pAccountPtr, pCaCert,  httpPtr, &wifi_client);
  
  #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();
  #endif
  
  DateTime responseHeaderDateTime = DateTime();   // Will be filled with DateTime value of the resonse from Azure Service

  // Insert Entity
  az_http_status_code statusCode = table.InsertTableEntity(pTableName, pTableEntity, (char *)outInsertETag, &responseHeaderDateTime, contApplicationIatomIxml, acceptApplicationIjson, dont_returnContent, false);
  
  #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();
  #endif

  lastResetCause = 0;
  tryUploadCounter++;

   // RoSchmi for tests: to simulate failed upload
  //az_http_status_code   statusCode = AZ_HTTP_STATUS_CODE_UNAUTHORIZED;
  
  if ((statusCode == AZ_HTTP_STATUS_CODE_NO_CONTENT) || (statusCode == AZ_HTTP_STATUS_CODE_CREATED))
  {
    sendResultState = true;
    if (!showGraphScreen)
    { 
      char codeString[35] {0};
      sprintf(codeString, "%s %i", "Entity inserted: ", az_http_status_code(statusCode));
      lcd_log_line((char *)codeString);
    }
    
    
    #if UPDATE_TIME_FROM_AZURE_RESPONSE == 1    // System time shall be updated from the DateTime value of the response ?
    
    dateTimeUTCNow = sysTime.getTime();
    time_helpers.update(dateTimeUTCNow);
    uint32_t actRtcTime = dateTimeUTCNow.secondstime();
    dateTimeUTCNow = responseHeaderDateTime;
    sysTimeNtpDelta = actRtcTime - dateTimeUTCNow.secondstime();
    sysTime.setTime(dateTimeUTCNow); 
    char buffer[] = "Azure-Utc: YYYY-MM-DD hh:mm:ss";
    dateTimeUTCNow.toString(buffer);

    if (!showGraphScreen)
    {
    lcd_log_line((char *)buffer);
    }
    #endif   
  }
  else            // request failed
  {               // note: internal error codes from -1 to -11 were converted for tests to error codes 401 to 411 since
                  // negative values cannot be returned as 'az_http_status_code' 

    failedUploadCounter++;
    sendResultState = false;
    lastResetCause = 100;      // Set lastResetCause to arbitrary value of 100 to signal that post request failed
    
    if (!showGraphScreen)
    {
      char codeString[35] {0};
      sprintf(codeString, "%s %i", "Insertion failed: ", az_http_status_code(statusCode));
      lcd_log_line((char *)codeString);
    }
    
    #if REBOOT_AFTER_FAILED_UPLOAD == 1   // When selected in config.h -> Reboot through SystemReset after failed uoload

        #if TRANSPORT_PROTOCOL == 1
          
          // The outcommended code resets the WiFi module (did not solve problem)
          //pinMode(RTL8720D_CHIP_PU, OUTPUT); 
          //digitalWrite(RTL8720D_CHIP_PU, LOW); 
          //delay(500); 
          //digitalWrite(RTL8720D_CHIP_PU, HIGH);  
          //delay(500);

          NVIC_SystemReset();     // Makes Code 64
        #endif
        #if TRANSPORT_PROTOCOL == 0     // for http requests reboot after the second, not the first, failed request
          if(failedUploadCounter > 1)
          {
            NVIC_SystemReset();     // Makes Code 64
          }
    #endif

    #endif

    #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();   
    #endif
    delay(1000);
  }
  return statusCode;
}


az_http_status_code createTable(CloudStorageAccount *pAccountPtr, X509Certificate pCaCert, const char * pTableName)
{ 

  #if TRANSPORT_PROTOCOL == 1
    static WiFiClientSecure wifi_client;
  #else
    static WiFiClient wifi_client;
  #endif

    #if TRANSPORT_PROTOCOL == 1
    wifi_client.setCACert(myX509Certificate);
    //wifi_client.setCACert(baltimore_corrupt_root_ca);
  #endif

  #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();
  #endif

  TableClient table(pAccountPtr, pCaCert,  httpPtr, &wifi_client);

  // Create Table
  az_http_status_code statusCode = table.CreateTable(pTableName, contApplicationIatomIxml, acceptApplicationIjson, returnContent, false);
  
   // RoSchmi for tests: to simulate failed upload
   //az_http_status_code   statusCode = AZ_HTTP_STATUS_CODE_UNAUTHORIZED;

  char codeString[35] {0};
  if ((statusCode == AZ_HTTP_STATUS_CODE_CONFLICT) || (statusCode == AZ_HTTP_STATUS_CODE_CREATED))
  {
    #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();
    #endif

    if (!showGraphScreen)
    {
      sprintf(codeString, "%s %i", "Table available: ", az_http_status_code(statusCode));  
      lcd_log_line((char *)codeString);
    }
  }
  else
  {
    if (!showGraphScreen)
    {
      sprintf(codeString, "%s %i", "Table Creation failed: ", az_http_status_code(statusCode));   
      lcd_log_line((char *)codeString);
    }
    delay(1000);
    NVIC_SystemReset();     // Makes Code 64  
  }
return statusCode;
}
