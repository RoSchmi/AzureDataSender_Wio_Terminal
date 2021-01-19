// Application AzureDataSender_Wio_Terminal
// Copyright RoSchmi 2020, 2021. License Apache 2.0

// Set WiFi and Azure credentials in file include/config_secret.h  (take config_secret_template.h as a template)
// Settings like sendinterval, transport protocol, tablenames and so on are to be defined in /include/config.h


  // I started to make this App from the Azure Storage Blob Example see: 
  // https://github.com/Azure/azure-sdk-for-c/blob/5c7444dfcd5f0b3bcf3aec2f7b62639afc8bd664/sdk/samples/storage/blobs/src/blobs_client_example.c

  //For a BLOB the Basis-URI consists of the name of the account, the namen of the Container and the namen of the BLOB:
  //https://myaccount.blob.core.windows.net/mycontainer/myblob
  //https://docs.microsoft.com/de-de/rest/api/storageservices/naming-and-referencing-containers--blobs--and-metadata

// Memory usage
// 20000000 	First 64 K Ram Block
// 2000FFFF
// 20010000	  Second 64 K Ram Block
// 2001FFFF
// 20020000	  Third 64 K Ram Block
// 2002FFFF

// Heap goes upwards from about 2000A4A0
// Stack goes downwards from 2000FFFF

// The following memory areas are extraordinarily used as buffers from this program
// 20029000	300 Bytes PROPERTIES_BUFFER_MEMORY_ADDR
// 20029200          900 Bytes Request Buffer
// 2002A000	2000 Bytes RESPONSE_BUFFER_MEMORY_ADDR
// 2002FFFF           Ende des Ram


#include <Arduino.h>
#include <config.h>
#include <config_secret.h>

#include <AzureStorage/CloudStorageAccount.h>
#include <AzureStorage/TableClient.h>
#include <AzureStorage/TableEntityProperty.h>
#include <AzureStorage/TableEntity.h>
#include <AzureStorage/AnalogTableEntity.h>
#include <AzureStorage/OnOffTableEntity.h>

#include <rpcWiFi.h>

#include "SAMCrashMonitor.h"
#include "DateTime.h"
#include <time.h>

#include <Time/SysTime.h>

#include <SensorData/DataContainerWio.h>
#include "SensorData/OnOffDataContainerWio.h"
#include "SensorData/OnOffSwitcherWio.h"
#include "SensorData/ImuManagerWio.h"

#include <azure/core/az_platform.h>
//#include <platform.h>
#include <azure/core/az_config_internal.h>
#include <azure/core/az_context.h>
#include <azure/core/az_http.h>

#include <az_wioterminal_roschmi.h> 

#include <stdio.h>
#include <stdlib.h>

#include "HTTPClient.h"
#include "DHT.h"
#include"LIS3DHTR.h"


#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"

#include "TFT_eSPI.h"
#include "Free_Fonts.h" //include the header file

#include <azure/core/az_json.h>

#include <azure/core/az_result.h>
#include <azure/core/az_span.h>
#include <azure/core/az_config.h>

#include <azure/core/az_http.h>
#include <azure/core/az_context.h>

#include <azure/core/az_http_transport.h>

//#include <azure/core/_az_cfg_prefix.h>   // may not be included

#include <azure/iot/az_iot_hub_client.h>

#include <azure/iot/az_iot_common.h>

#include <azure/core/az_platform.h>
#include <azure/core/az_retry_internal.h>

#include <azure/iot/az_iot_hub_client.h>

#include "Time/Rs_time_helpers.h"

const char analogTableName[45] = ANALOG_TABLENAME;

const char OnOffTableName_1[45] = ON_OFF_TABLENAME_01;
const char OnOffTableName_2[45] = ON_OFF_TABLENAME_02;
const char OnOffTableName_3[45] = ON_OFF_TABLENAME_03;
const char OnOffTableName_4[45] = ON_OFF_TABLENAME_04;

// The PartitionKey for the analog table may have a prefix to be distinguished, here: "Y2_" 
const char * analogTablePartPrefix = (char *)"Y2_";

// The PartitionKey for the On/Off-tables may have a prefix to be distinguished, here: "Y2_" 
const char * onOffTablePartPrefix = (char *)"Y2_";

// The PartitinKey can be augmented with a string representing year and month (recommended)
const bool augmentPartitionKey = true;

// The TableName can be augmented with the actual year (recommended)
const bool augmentTableNameWithYear = true;

// Define Datacontainer with SendInterval and InvalidateInterval as defined in config.h
int sendIntervalSeconds = (SENDINTERVAL_MINUTES * 60) < 1 ? 1 : (SENDINTERVAL_MINUTES * 60);

DataContainerWio dataContainer(TimeSpan(sendIntervalSeconds), TimeSpan(0, 0, INVALIDATEINTERVAL_MINUTES % 60, 0), (float)MIN_DATAVALUE, (float)MAX_DATAVALUE, (float)MAGIC_NUMBER_INVALID);

OnOffDataContainerWio onOffDataContainer;

OnOffSwitcherWio onOffSwitcherWio;

ImuManagerWio imuManagerWio;

LIS3DHTR<TwoWire> lis;

#define DHTPIN 0
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

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
uint32_t timeNtpUpdateCounter = 0;
volatile int32_t sysTimeNtpDelta = 0;

volatile uint32_t previousNtpMillis = 0;
volatile uint32_t previousSensorReadMillis = 0;

uint32_t ntpUpdateInterval = 60000;
uint32_t analogSensorReadInterval = 100;

char timeServer[] = "pool.ntp.org"; // external NTP server e.g. better pool.ntp.org
unsigned int localPort = 2390;      // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

bool ledState = false;
uint8_t lastResetCause = -1;

uint32_t failedUploadCounter = 0;

unsigned long utcTime;
DateTime dateTimeUTCNow;
volatile uint32_t dateTimeUtcOldSeconds;
volatile uint32_t dateTimeUtcNewSeconds;

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

static void button_handler_right() 
{
  int state = digitalRead(BUTTON_1);
  DateTime utcNow = sysTime.getTime();
  time_helpers.update(utcNow);
  int timeZoneOffsetUTC = time_helpers.isDST() ? TIMEZONE + DSTOFFSET : TIMEZONE;
  onOffDataContainer.SetNewOnOffValue(2, state == 0, utcNow, timeZoneOffsetUTC); 
}

static void button_handler_mid() 
{
  volatile int state = digitalRead(BUTTON_2);
  DateTime utcNow = sysTime.getTime();
  time_helpers.update(utcNow);
  int timeZoneOffsetUTC = time_helpers.isDST() ? TIMEZONE + DSTOFFSET : TIMEZONE;
  onOffDataContainer.SetNewOnOffValue(1, state == 0, utcNow, timeZoneOffsetUTC);
}

static void button_handler_left() 
{
  volatile int state = digitalRead(BUTTON_3);
  DateTime utcNow = sysTime.getTime();
  time_helpers.update(utcNow);
  int timeZoneOffsetUTC = time_helpers.isDST() ? TIMEZONE + DSTOFFSET : TIMEZONE;
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
float ReadAnalogSensor(int pAin);
DateTime actualizeSysTimeFromNtp();
void createSampleTime(DateTime dateTimeUTCNow, int timeZoneOffsetUTC, char * sampleTime);
az_http_status_code  createTable(CloudStorageAccount * myCloudStorageAccountPtr, X509Certificate myX509Certificate, const char * tableName);
az_http_status_code CreateTable( const char * tableName, ContType pContentType, AcceptType pAcceptType, ResponseType pResponseType, bool);
az_http_status_code insertTableEntity(CloudStorageAccount *myCloudStorageAccountPtr, X509Certificate pCaCert, const char * pTableName, TableEntity pTableEntity, char * outInsertETag);
void makePartitionKey(const char * partitionKeyprefix, bool augmentWithYear, az_span outSpan, size_t *outSpanLength);
void makeRowKey(DateTime actDate, az_span outSpan, size_t *outSpanLength);
void showDisplayFrame();
void fillDisplayFrame(double an_1, double an_2, double an_3, double an_4, bool on_1,  bool on_2, bool on_3, bool on_4);


// Seems not to work
void myCrashHandler(SAMCrashReport &report)
{
  int pcState = report.pc;
  char buf[100];
  sprintf(buf, "Pc: %i", pcState);
  lcd_log_line(buf);
  SAMCrashMonitor::dumpCrash(report);
  int dummy56738 = 1;

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
  Serial.println("\r\nStarting");

  pinMode(WIO_5S_PRESS, INPUT_PULLUP);

  pinMode(BUTTON_1, INPUT);
  pinMode(BUTTON_3, INPUT);
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
  
  //Initialize OnOffSwitcher
  onOffSwitcherWio.begin(TimeSpan(60 * 60));   // Toggle every 30 sec
  onOffSwitcherWio.SetActive();
  
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

  delay(1000);
  char buf[100];
  
  sprintf(buf, "RTL8720 Firmware: %s", rpc_system_version());
  lcd_log_line(buf);
  lcd_log_line((char *)"Initial WiFi-Status:");
  lcd_log_line(itoa((int)WiFi.status(), buf, 10));
    
  delay(500);
  
  //Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  lcd_log_line((char *)"First disconnecting, Status:");
  while (WiFi.status() != WL_DISCONNECTED)
  {
    WiFi.disconnect();
    delay(200); 
  }

  lcd_log_line(itoa((int)WiFi.status(), buf, 10));
  delay(500);

  sprintf(buf, "Connecting to SSID: %s", ssid);
  lcd_log_line(buf);

  if (!ssid || *ssid == 0x00 || strlen(ssid) > 31)
  {
    // Stay in endless loop
      while (true)
      {
        lcd_log_line((char *)"Invalid: SSID or PWD");
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
    int timeout = SAMCrashMonitor::enableWatchdog(4000);
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
   
// Wait for 2000 ms
  for (int i = 0; i < 3; i++)
  {
    delay(1000);
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
  
  analogSensorReadInterval =  ANALOG_SENSOR_READ_INTERVAL_MILLIS < 10 ? 10 : ANALOG_SENSOR_READ_INTERVAL_MILLIS > 5000 ? 5000: ANALOG_SENSOR_READ_INTERVAL_MILLIS;                                                                                     // not below 5 min           
     
  //lcd_log_line((char *)ntp.formattedTime("%d. %B %Y"));    // dd. Mmm yyyy
  //lcd_log_line((char *)ntp.formattedTime("%A %T"));        // Www hh:mm:ss
  
  /*
  DateTime now = DateTime(F(__DATE__), F(__TIME__));
  dateTimeUTCNow = DateTime((uint16_t) now.year(), (uint8_t)now.month(), (uint8_t)now.day(),
                (uint8_t)now.hour(), (uint8_t)now.minute(), (uint8_t)now.second());
  */

  //DateTime now = DateTime(F((char *)ntp.formattedTime("%d. %B %Y")), F((char *)ntp.formattedTime("%A %T")));
  
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
  time_helpers.update(dateTimeUTCNow);

  // Set Daylightsavingtime for central europe
  // The following two lines must be adapted to your DayLightSavings zone
  time_helpers.ruleDST("CEST", Last, Sun, Mar, 2, 120); // last sunday in march 2:00, timezone +120min (+1 GMT + 1h summertime offset)
  time_helpers.ruleSTD("CET", Last, Sun, Oct, 3, 60); // last sunday in october 3:00, timezone +60min (+1 GMT)

  lcd_log_line((char *)time_helpers.formattedTime("%d. %B %Y"));    // dd. Mmm yyyy
  lcd_log_line((char *)time_helpers.formattedTime("%A %T"));        // Www hh:mm:ss
  
  dht.begin();

  lis.begin(Wire1);
  lis.setOutputDataRate(LIS3DHTR_DATARATE_25HZ); //Data output rate
  lis.setFullScaleRange(LIS3DHTR_RANGE_2G); //Scale range set to 2g
  //lis.setFullScaleRange(LIS3DHTR_RANGE_8G); //Scale range set to 2g
  imuManagerWio.begin();
  imuManagerWio.SetActive();



  // Wait for 2000 ms
  for (int i = 0; i < 3; i++)
  {
    delay(1000);
    #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();
    #endif
  }
  
  // Clear screen
  current_text_line = 0;
  tft.fillScreen(screenColor);
  

  dateTimeUTCNow = sysTime.getTime();

  String augmentedAnalogTableName = analogTableName;  
  if (augmentTableNameWithYear)
  {
    augmentedAnalogTableName += (dateTimeUTCNow.year());
  }
  

  #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();
    #endif

  // RoSchmi: do not delete
  // The following line creates a table in the Azure Storage Account defined in config.h
  //az_http_status_code theResult = createTable(myCloudStorageAccountPtr, myX509Certificate, (char *)augmentedAnalogTableName.c_str());


  showDisplayFrame();
  fillDisplayFrame(999.9, 999.9, 999.9, 999.9, false, false, false, false);


  delay(50);
  previousNtpMillis = millis();
}

void showDisplayFrame()
{
  if (showGraphScreen)
  {
    tft.fillScreen(TFT_BLUE);
    backColor = TFT_LIGHTGREY;
    textFont = FSSB9;
    textColor = TFT_BLACK;
    current_text_line = 1;
    const char * PROGMEM line_1 = (char *)"  Temperature      Humidity";
    lcd_log_line((char *)line_1);
    current_text_line = 6;
    const char * PROGMEM line_2 = (char *)"        Light            Movement";
    lcd_log_line((char *)line_2);
    current_text_line = 10;
    const char * PROGMEM line_3 = (char *)"      (1)           (2)           (3)           (4)";
    lcd_log_line((char *)line_3);
  }
}

void fillDisplayFrame(double an_1, double an_2, double an_3, double an_4, bool on_1,  bool on_2, bool on_3, bool on_4)
{
  if (showGraphScreen)
  {
    an_1 = constrain(an_1, -999.9, 999.9);
    an_2 = constrain(an_2, -999.9, 999.9);
    an_3 = constrain(an_3, -999.9, 999.9);
    an_4 = constrain(an_4, -999.9, 999.9);

    typedef char * postGap[4];
    postGap postGapArray[4] {(char *)"\0", (char *)" \0", (char *)"  \0", (char *)"   \0"};

    const char* Gap1 = "    ";
    const char* Gap2 = "    ";
    char lineBuf[40] {0};
  
    char an_left_Str[7] {0};
    char an_right_Str[7] {0};
    sprintf(an_left_Str, "%.1f", an_1);
    sprintf(an_right_Str, "%.1f", an_2);
  
    if (an_1 == 999.9)
    {
      strcpy(an_left_Str, (char *)"--.-");
    }
    if (an_2 == 999.9)
    {
      strcpy(an_right_Str, (char *)"--.-");
    }
  
    int i_1 = 6 - strlen(an_left_Str);
    int i_2 = 6 - strlen(an_right_Str);
   
    sprintf(lineBuf, "%s%s%s%s%s%s ", (char *)Gap1, (char *)postGapArray[i_1], (char *)an_left_Str, (char *)Gap2, (char *)postGapArray[i_2], (char *)an_right_Str);
    current_text_line = 3;
    backColor = TFT_BLUE;
    textFont = FSSBO18;
    textColor = TFT_ORANGE;
    lcd_log_line((char *)"");
    lcd_log_line((char *)"");
    current_text_line = 3;
    lcd_log_line(lineBuf);

    sprintf(an_left_Str, "%.1f", an_3);
    sprintf(an_right_Str, "%.1f", an_4);

    if (an_3 == 999.9)
    {
      strcpy(an_left_Str, (char *)"--.-");
    }
    if (an_4 == 999.9)
    {
      strcpy(an_right_Str, (char *)"--.-");
    }

    i_1 = 6 - strlen(an_left_Str);
    i_2 = 6 - strlen(an_right_Str);
   
    sprintf(lineBuf, "%s%s%s%s%s%s ", (char *)Gap1, (char *)postGapArray[i_1], (char *)an_left_Str, (char *)Gap2, (char *)postGapArray[i_2], (char *)an_right_Str);
    current_text_line = 8;
    lcd_log_line((char *)"");
    lcd_log_line((char *)"");
    current_text_line = 8;
    lcd_log_line(lineBuf);
  
    tft.fillRect(16, 12 * LCD_LINE_HEIGHT, 60, LCD_LINE_HEIGHT, on_1 ? TFT_GREEN : TFT_DARKGREY);
    tft.fillRect(92, 12 * LCD_LINE_HEIGHT, 60, LCD_LINE_HEIGHT, on_2 ? TFT_GREEN : TFT_DARKGREY);
    tft.fillRect(168, 12 * LCD_LINE_HEIGHT, 60, LCD_LINE_HEIGHT, on_3 ? TFT_GREEN : TFT_DARKGREY);
    tft.fillRect(244, 12 * LCD_LINE_HEIGHT, 60, LCD_LINE_HEIGHT, on_4 ? TFT_GREEN : TFT_DARKGREY);
  }
}


void loop() 
{ 
  if (loopCounter++ % 10000 == 0)   // Make decisions to send data every 10000 th round and toggle Led to signal that App is running
  {
    volatile uint32_t currentMillis = millis();
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);

    #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();
    #endif

    // Update RTC from Ntp when ntpUpdateInterval has expired
    if ((currentMillis - previousNtpMillis) >= ntpUpdateInterval) 
    {
        previousNtpMillis = currentMillis;
        dateTimeUTCNow = sysTime.getTime();
        uint32_t actRtcTime = dateTimeUTCNow.secondstime();

        int loopCtr = 0;
        unsigned long  utcNtpTime = getNTPtime();   // Get NTP time, try up to 4 times        
        while ((loopCtr < 4) && utcTime == 0)
        {
          loopCtr++;
          utcTime = getNTPtime();
        }
        
        if (utcNtpTime != 0 )       // if NTP request was successful --> synchronize RTC 
        {       
            dateTimeUTCNow = utcNtpTime;
            sysTimeNtpDelta = actRtcTime - dateTimeUTCNow.secondstime();
            //lastNtpUpdate = dateTimeUTCNow.secondstime();
            char buffer[] = "Utc: YYYY-MM-DD hh:mm:ss";
            dateTimeUTCNow.toString(buffer);
            lcd_log_line((char *)buffer);  
            
            sysTime.setTime(dateTimeUTCNow);
            timeNtpUpdateCounter++;   
        }  
    }
    else            // it was not NTP Update, proceed with send to analog table or On/Off-table
    {
      dateTimeUTCNow = sysTime.getTime();     
      // Get offset in minutes between UTC and local time with consideration of DST
      time_helpers.update(dateTimeUTCNow);
      int timeZoneOffsetUTC = time_helpers.isDST() ? TIMEZONE + DSTOFFSET : TIMEZONE;
      DateTime localTime = dateTimeUTCNow.operator+(TimeSpan(timeZoneOffsetUTC * 60));
      // In the last 15 sec of each day we set a pulse to Off-State when we had On-State before
      bool isLast15SecondsOfDay = (localTime.hour() == 23 && localTime.minute() == 59 &&  localTime.second() > 45) ? true : false;
      //bool isLast15SecondsOfDay = (localTime.second() > 45) ? true : false;

      dataContainer.SetNewValue(0, dateTimeUTCNow, ReadAnalogSensor(0));
      dataContainer.SetNewValue(1, dateTimeUTCNow, ReadAnalogSensor(1));
      dataContainer.SetNewValue(2, dateTimeUTCNow, ReadAnalogSensor(2));
      dataContainer.SetNewValue(3, dateTimeUTCNow, ReadAnalogSensor(3));
      
      // Check if automatic OnOfSwitcher has toggled
      if (onOffSwitcherWio.hasToggled(dateTimeUTCNow))
          {
            bool state = onOffSwitcherWio.GetState();
            time_helpers.update(dateTimeUTCNow);
            int timeZoneOffsetUTC = time_helpers.isDST() ? TIMEZONE + DSTOFFSET : TIMEZONE;
            onOffDataContainer.SetNewOnOffValue(2, state, dateTimeUTCNow, timeZoneOffsetUTC);
          }
      
      // Check if something is to do: send analog data ? send On/Off-Data ? Handle EndOfDay stuff ?
      if (dataContainer.hasToBeSent() || onOffDataContainer.One_hasToBeBeSent() || isLast15SecondsOfDay)
      {    
        //Create some buffer
        char sampleTime[25] {0};    // Buffer to hold sampletime        
        char strData[100];          // Buffer to hold display message  
        char EtagBuffer[50] {0};    // Buffer to hold returned Etag

        // Create az_span to hold partitionkey
        char partKeySpan[25] {0};
        size_t partitionKeyLength = 0;
        az_span partitionKey = AZ_SPAN_FROM_BUFFER(partKeySpan);
        
        // Create az_span to hold partitionkey
        char rowKeySpan[25] {0};
        size_t rowKeyLength = 0;
        az_span rowKey = AZ_SPAN_FROM_BUFFER(rowKeySpan);

        if (dataContainer.hasToBeSent())       // have to send analog values ?
        {
          // Retrieve edited sample values from container
          SampleValueSet sampleValueSet = dataContainer.getCheckedSampleValues(dateTimeUTCNow);
     
          createSampleTime(sampleValueSet.LastSendTime, timeZoneOffsetUTC, (char *)sampleTime);

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
                    NVIC_SystemReset();     // Makes Code 64
                 }
              }


          // Create an Array of, here, 5 Properties
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
          makePartitionKey(analogTablePartPrefix, augmentPartitionKey, partitionKey, &partitionKeyLength);
          partitionKey = az_span_slice(partitionKey, 0, partitionKeyLength);

          // Create the RowKey (special format)
          makeRowKey(dateTimeUTCNow, rowKey, &rowKeyLength);
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

          // RoSchmi Todo: event. include code to check for memory leaks here


          // Store Entity to Azure Cloud   
          az_http_status_code insertResult = insertTableEntity(myCloudStorageAccountPtr, myX509Certificate, (char *)augmentedAnalogTableName.c_str(), analogTableEntity, (char *)EtagBuffer);
        }
        else     // Task to was not NTP and not send analog table, so it is Send On/Off values or End of day stuff?
        {
          

          OnOffSampleValueSet onOffValueSet = onOffDataContainer.GetOnOffValueSet();
        
          for (int i = 0; i < 4; i++)    // Do for 4 OnOff-Tables
          {
            if (onOffValueSet.OnOffSampleValues[i].hasToBeSent)
            {
              onOffDataContainer.Reset_hasToBeSent(i);     
              EntityProperty OnOffPropertiesArray[5];
              TimeSpan  onTime =  onOffValueSet.OnOffSampleValues[i].OnTimeDay;
                           
              char OnTimeDay[15] = {0};
              sprintf(OnTimeDay, "%03i-%02i:%02i:%02i", onTime.days(), onTime.hours(), onTime.minutes(), onTime.seconds());
              createSampleTime(dateTimeUTCNow, timeZoneOffsetUTC, (char *)sampleTime);

              // Tablenames come from the onOffValueSet, here usually the tablename is augmented with the actual year
              String augmentedOnOffTableName = onOffValueSet.OnOffSampleValues[i].tableName;
              if (augmentTableNameWithYear)
              {
                augmentedOnOffTableName += (dateTimeUTCNow.year());     
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
           
              //OnOffTablesParamsArray[0].LastSendTime = dateTimeUTCNow;
              size_t onOffPropertyCount = 5;
              OnOffPropertiesArray[0] = (EntityProperty)TableEntityProperty((char *)"ActStatus", onOffValueSet.OnOffSampleValues[i].outInverter ? (char *)(onOffValueSet.OnOffSampleValues[i].actState ? "On" : "Off") : (char *)(onOffValueSet.OnOffSampleValues[i].actState ? "Off" : "On"), (char *)"Edm.String");
              OnOffPropertiesArray[1] = (EntityProperty)TableEntityProperty((char *)"LastStatus", onOffValueSet.OnOffSampleValues[i].outInverter ? (char *)(onOffValueSet.OnOffSampleValues[i].lastState ? "On" : "Off") : (char *)(onOffValueSet.OnOffSampleValues[i].lastState ? "Off" : "On"), (char *)"Edm.String");
              OnOffPropertiesArray[2] = (EntityProperty)TableEntityProperty((char *)"OnTimeDay", (char *) OnTimeDay, (char *)"Edm.String");
              OnOffPropertiesArray[3] = (EntityProperty)TableEntityProperty((char *)"SampleTime", (char *) sampleTime, (char *)"Edm.String");
              OnOffPropertiesArray[4] = (EntityProperty)TableEntityProperty((char *)"TimeFromLast", (char *) timefromLast, (char *)"Edm.String");
          
              // Create the PartitionKey (special format)
              makePartitionKey(onOffTablePartPrefix, augmentPartitionKey, partitionKey, &partitionKeyLength);
              partitionKey = az_span_slice(partitionKey, 0, partitionKeyLength);

              // Create the RowKey (special format)
              makeRowKey(dateTimeUTCNow, rowKey, &rowKeyLength);
              rowKey = az_span_slice(rowKey, 0, rowKeyLength);
  
              // Create TableEntity consisting of PartitionKey, RowKey and the properties named 'SampleTime', 'T_1', 'T_2', 'T_3' and 'T_4'
              OnOffTableEntity onOffTableEntity(partitionKey, rowKey, az_span_create_from_str((char *)sampleTime),  OnOffPropertiesArray, onOffPropertyCount);
          
              onOffValueSet.OnOffSampleValues[i].insertCounter++;
              

              // Store Entity to Azure Cloud   
              az_http_status_code insertResult = insertTableEntity(myCloudStorageAccountPtr, myX509Certificate, (char *)augmentedOnOffTableName.c_str(), onOffTableEntity, (char *)EtagBuffer);
              break; // Send only one in each round of loop 
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
    if (digitalRead(WIO_5S_PRESS) == LOW)
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
      
      while(digitalRead(WIO_5S_PRESS) == LOW);   
    }


    if (showGraphScreen)
    {
      fillDisplayFrame(dataContainer.SampleValues[0].Value, dataContainer.SampleValues[1].Value,
                      dataContainer.SampleValues[2].Value, dataContainer.SampleValues[2].Value,
                      onOffDataContainer.ReadOnOffState(0), onOffDataContainer.ReadOnOffState(1),
                      onOffDataContainer.ReadOnOffState(2), onOffDataContainer.ReadOnOffState(3));
    }
    
  }
  loopCounter++;
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
            //Serial.println("udp packet received");
            //Serial.println("");

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

            // WA local time 
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
}

String floToStr(float value)
{
  char buf[10];
  sprintf(buf, "%.1f", (roundf(value * 10.0))/10.0);
  return String(buf);
}

float ReadAnalogSensor(int pAin)
{
#ifndef USE_SIMULATED_SENSORVALUES
            // Use values read from an analog source
            // Change the function for each sensor to your needs

            double theRead = MAGIC_NUMBER_INVALID;
            switch (pAin)
            {
                case 0:
                    {   
                        // Take theRead == nan or (nearly) exactly 0.0 as invalid
                        // (if no sensor is connected the function returns 0) 
                        theRead = dht.readTemperature();
                        if (isnan(theRead) || (theRead > - 0.00001 && theRead < 0.00001))
                        {                         
                          theRead = MAGIC_NUMBER_INVALID;                      
                        }

                    }
                    break;

                case 1:
                    {
                        // Take theRead for temperature == nan or (nearly) exactly 0.0 as invalid
                        // (if no sensor is connected the function returns 0)                        
                        theRead = dht.readTemperature();         
                        if (isnan(theRead) || (theRead > - 0.00001 && theRead < 0.00001))
                        {
                          volatile int dummy1234 = 1;
                          theRead = MAGIC_NUMBER_INVALID;
                          
                        }
                        else
                        {
                          volatile int dummy2234 = 1;
                          theRead = dht.readHumidity();
                         
                        }
                    }
                    break;
                case 2:
                    {
                        /*
                        theRead = analogRead(WIO_LIGHT);
                        theRead = map(theRead, 0, 1023, 0, 100);
                        theRead = theRead < 0 ? 0 : theRead > 100 ? 100 : theRead;
                        */

                        theRead = insertCounterAnalogTable;
                        theRead = theRead / 10;                          
                    }
                    break;
                case 3:
                    /*
                    // Read the accelerometer
                    {
                        ImuSampleValues sampleValues;
                        sampleValues.X_Read = lis.getAccelerationX();
                        sampleValues.Y_Read = lis.getAccelerationY();
                        sampleValues.Z_Read = lis.getAccelerationZ() + 1;
                        imuManagerWio.SetNewImuReadings(sampleValues);
                        theRead = imuManagerWio.GetLastImuReadings().Z_Read;
                        // Serial.println(theRead * 10);
                        // constrain(x, a, b);                      
                    }
                    */
                    
                    {
                        theRead = lastResetCause;                        
                    }
                    
                    break;
            }
            theRead = isnan(theRead) ? MAGIC_NUMBER_INVALID : theRead;
            return theRead ;
#endif

#ifdef USE_SIMULATED_SENSORVALUES
      #ifdef USE_TEST_VALUES
            // Here you can select that diagnostic values (for debugging)
            // are sent to your storage table
            double theRead = MAGIC_NUMBER_INVALID;
            switch (pAin)
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

  

  #elif
            // Only as an example we here return values which draw a sinus curve
            // Console.WriteLine("entering Read analog sensor");
            int frequDeterminer = 4;
            int y_offset = 1;
            // different frequency and y_offset for aIn_0 to aIn_3
            if (pAin == 0)
            { frequDeterminer = 4; y_offset = 1; }
            if (pAin == 1)
            { frequDeterminer = 8; y_offset = 10; }
            if (pAin == 2)
            { frequDeterminer = 12; y_offset = 20; }
            if (pAin == 3)
            { frequDeterminer = 16; y_offset = 30; }
             
            int secondsOnDayElapsed = dateTimeUTCNow.second() + dateTimeUTCNow.minute() * 60 + dateTimeUTCNow.hour() *60 *60;
                    
            return roundf((float)25.0 * (float)sin(PI / 2.0 + (secondsOnDayElapsed * ((frequDeterminer * PI) / (float)86400)))) / 10  + y_offset;          

  #endif
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

void makePartitionKey(const char * partitionKeyprefix, bool augmentWithYear, az_span outSpan, size_t *outSpanLength)
{
  // if wanted, augment with year and month (12 - month for right order)
  DateTime dateTimeUTCNow = sysTime.getTime();                      
  char dateBuf[20] {0};
  sprintf(dateBuf, "%s%d-%02d", partitionKeyprefix, (dateTimeUTCNow.year()), (12 - dateTimeUTCNow.month()));                  
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
  //TableClient table(pAccountPtr, pCaCert,  httpPtr, wifi_client);


  #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();
  #endif
  // Insert Entity
  DateTime responseHeaderDateTime = DateTime();
  az_http_status_code statusCode = table.InsertTableEntity(pTableName, pTableEntity, (char *)outInsertETag, &responseHeaderDateTime, contApplicationIatomIxml, acceptApplicationIjson, dont_returnContent, false);
  
  #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();
  #endif

  // RoSchmi for tests: to simulate failed upload
  // statusCode = AZ_HTTP_STATUS_CODE_UNAUTHORIZED;

  lastResetCause = 0;

  char codeString[35] {0};
  if ((statusCode == AZ_HTTP_STATUS_CODE_NO_CONTENT) || (statusCode == AZ_HTTP_STATUS_CODE_CREATED))
  { 
    sprintf(codeString, "%s %i", "Entity inserted: ", az_http_status_code(statusCode));

    if (!showGraphScreen)
    {   
      lcd_log_line((char *)codeString);
    }
    

    #if UPDATE_TIME_FROM_AZURE_RESPONSE == 1
    
    dateTimeUTCNow = sysTime.getTime();
    uint32_t actRtcTime = dateTimeUTCNow.secondstime();
    //unsigned long  utcHeaderDateTime = responseHeaderDateTime.secondstime();
    dateTimeUTCNow = responseHeaderDateTime;
    sysTimeNtpDelta = actRtcTime - dateTimeUTCNow.secondstime();
    sysTime.setTime(dateTimeUTCNow); 
    char buffer[] = "AzureUtc: YYYY-MM-DD hh:mm:ss";
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
    lastResetCause = 100;
    
    if (!showGraphScreen)
    {
      sprintf(codeString, "%s %i", "Insertion failed: ", az_http_status_code(statusCode));
      lcd_log_line((char *)codeString);
    }
    
    
    #if REBOOT_AFTER_FAILED_UPLOAD == 1   // Reboot through SystemReset

        #if TRANSPORT_PROTOCOL == 1

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

     /*
        while(true)               // Makes WatchDog Reset: Code 32
        {
          delay(1000);
        }
      */     
    #endif
    delay(1000);
  }
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
  }
return statusCode;
}






