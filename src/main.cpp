// Application AzureDataSender_Wio_Terminal
// Copyright RoSchmi 2020. License Apache 2.0

// Set WiFi and Azure credentials in file include/config_secret.h  (take config_secret_template.h as a template)
// Settings like sendinterval, transport protocol and so on are to be made in /include/config.h


  // I started from Azure Storage Blob Example see: 
  // https://github.com/Azure/azure-sdk-for-c/blob/5c7444dfcd5f0b3bcf3aec2f7b62639afc8bd664/sdk/samples/storage/blobs/src/blobs_client_example.c

  //For a BLOB the Basis-URI consists of the name of the account, the namen of the Container and the namen of the BLOB:
  //https://myaccount.blob.core.windows.net/mycontainer/myblob
  //https://docs.microsoft.com/de-de/rest/api/storageservices/naming-and-referencing-containers--blobs--and-metadata


#include <Arduino.h>
#include <config.h>
#include <config_secret.h>

#include <AzureStorage/CloudStorageAccount.h>
#include <AzureStorage/TableClient.h>
#include <AzureStorage/TableEntityProperty.h>
#include <AzureStorage/TableEntity.h>
#include <AzureStorage/AnalogTableEntity.h>

#include <rpcWiFi.h>

#include "DateTime.h"
#include <time.h>

#include <Time/SysTime.h>

#include <SensorData/DataContainerWio.h>

#include <azure/core/az_platform.h>
//#include <platform.h>
#include <azure/core/az_config_internal.h>
#include <azure/core/az_context.h>
#include <azure/core/az_http.h>

#include <az_wioterminal_roschmi.h> 

#include <stdio.h>
#include <stdlib.h>

#include "HTTPClient.h"

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


// The PartitionKey may have a prefix to be distinguished, here: "Y2_" 
const char * analogTablePartPrefix = (char *)"Y2_";

// The PartitinKey can be augmented with a string representing year and month (recommended)
const bool augmentPartitionKey = true;

// The TableName can be augmented with the actual year (recommended)
const bool augmentTableNameWithYear = true;

// Define Datacontainer with SendInterval and InvalidateInterval as defined in config.h
int sendIntervalSeconds = (SENDINTERVAL_MINUTES * 60) < 1 ? 1 : (SENDINTERVAL_MINUTES * 60);
DataContainerWio dataContainer(TimeSpan(sendIntervalSeconds), TimeSpan(0, 0, INVALIDATEINTERVAL_MINUTES % 60, 0), (float)MIN_DATAVALUE, (float)MAX_DATAVALUE, (float)MAGIC_NUMBER_INVALID);

TFT_eSPI tft;
int current_text_line = 0;

#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define LCD_FONT FreeSans9pt7b
#define LCD_LINE_HEIGHT 18

uint32_t loopCounter = 0;
unsigned int insertCounter = 0;
uint32_t timeNtpUpdateCounter = 0;
volatile int32_t sysTimeNtpDelta = 0;

volatile uint32_t previousNtpMillis = 0;
volatile uint32_t previousSensorReadMillis = 0;

uint32_t ntpUpdateInterval = 60000;
uint32_t analogSensorReadInterval = 100;

//uint32_t lastNtpUpdate = 0;

bool actualTimeIsDST = false;


char timeServer[] = "pool.ntp.org"; // external NTP server e.g. better pool.ntp.org
unsigned int localPort = 2390;      // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets



bool ledState = false;


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

/*
#if TRANSPORT_PROTOCOL == 1
  WiFiClientSecure wifi_client;
#else
  WiFiClient wifi_client;
#endif
*/

// Set transport protocol as defined in config.h
static bool UseHttps_State = TRANSPORT_PROTOCOL == 0 ? false : true;
CloudStorageAccount myCloudStorageAccount(AZURE_CONFIG_ACCOUNT_NAME, AZURE_CONFIG_ACCOUNT_KEY, UseHttps_State);
CloudStorageAccount * myCloudStorageAccountPtr = &myCloudStorageAccount;

void lcd_log_line(char* line) {
    // clear line
    tft.fillRect(0, current_text_line * LCD_LINE_HEIGHT, 320, LCD_LINE_HEIGHT, TFT_WHITE);
    tft.drawString(line, 5, current_text_line * LCD_LINE_HEIGHT);

    current_text_line++;
    current_text_line %= ((LCD_HEIGHT-20)/LCD_LINE_HEIGHT);
    if (current_text_line == 0) {
      tft.fillScreen(TFT_WHITE);
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

void setup() 
{ 
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_WHITE);
  tft.setFreeFont(&LCD_FONT);
  tft.setTextColor(TFT_BLACK);

  pinMode(LED_BUILTIN, OUTPUT);
  lcd_log_line((char *)"Starting");
  
  Serial.begin(9600);
  Serial.println("\r\nStarting");

  char buf[100];
  sprintf(buf, "RTL8720 Firmware: %s", rpc_system_version());
  lcd_log_line(buf);
  lcd_log_line((char *)"Initial WiFi-Status:");
  lcd_log_line(itoa((int)WiFi.status(), buf, 10));
    
  delay(1000);
  //Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  lcd_log_line((char *)"First disconnecting, Status:");
  while (WiFi.status() != WL_DISCONNECTED)
  {
    WiFi.disconnect();
    delay(200);
    
  }
  lcd_log_line(itoa((int)WiFi.status(), buf, 10));
  delay(1000);
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
  while (true)
  {
    // Stay in endless loop
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
    delay(2000);
    lcd_log_line(itoa((int)WiFi.status(), buf, 10));
    WiFi.begin(ssid, password);
  }

  

   lcd_log_line((char *)"Connected, new Status:");
    lcd_log_line(itoa((int)WiFi.status(), buf, 10));
  
  IPAddress localIpAddress = WiFi.localIP();
  IPAddress gatewayIp =  WiFi.gatewayIP();
  IPAddress subNetMask =  WiFi.subnetMask();
  IPAddress dnsServerIp = WiFi.dnsIP();
  
  delay(1000);
  current_text_line = 0;
  tft.fillScreen(TFT_WHITE);
    
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
  

  /*
  #if TRANSPORT_PROTOCOL == 1
  wifi_client.setCACert(baltimore_root_ca);
  #endif
  */
  
  /*
  int ntpCounter = 0;
  while (!ntp.update())
  {
    ntpCounter++;
    previousMillis = millis();
    
    while (millis() - previousMillis <= 200)
    { }   
  }
  lcd_log_line(itoa(ntpCounter, buf, 10));
  */
  
  
  
  //ntp.updateInterval(_UPDATE_INTERVAL_MINUTES(NTP < 1 ? 1 : NTP_UPDATE_INTERVAL_MINUTES) * 60 * 1000);  // Update from ntp (e.g. every 10 minutes)
  
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
  
  //dateTimeUTCNow = actualizeSysTimeFromNtpIfNeeded();

  //dateTimeUTCNow = DateTime((uint16_t) ntp.year(), (uint8_t)ntp.month(), (uint8_t)ntp.day(),
  //             (uint8_t)ntp.hours(), (uint8_t)ntp.minutes(), (uint8_t)ntp.seconds());
  
  //ntp.stop();

  

  int getTimeCtr = 0; 
  utcTime = getNTPtime();
  while ((getTimeCtr < 5) && utcTime == 0)
  {   
      getTimeCtr++;
      utcTime = getNTPtime();
  }

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
  time_helpers.ruleDST("CEST", Last, Sun, Mar, 2, 120); // last sunday in march 2:00, timezone +120min (+1 GMT + 1h summertime offset)
  time_helpers.ruleSTD("CET", Last, Sun, Oct, 3, 60); // last sunday in october 3:00, timezone +60min (+1 GMT)

  lcd_log_line((char *)time_helpers.formattedTime("%d. %B %Y"));    // dd. Mmm yyyy
  lcd_log_line((char *)time_helpers.formattedTime("%A %T"));        // Www hh:mm:ss

  delay(5000);
  // Clear screen
  current_text_line = 0;
  tft.fillScreen(TFT_WHITE);
  

  // This code snippet can be used to get the addresses of the heap
  // and to 
  uint32_t * ptr_one;
  uint32_t * last_ptr_one;
    
  /*
   for (volatile int i = 0; 1 < 100000; i++)
   {
     last_ptr_one = ptr_one;
     ptr_one = 0;
     ptr_one = (uint32_t *)malloc(1);
     if (ptr_one == 0)
     {
       ptr_one = last_ptr_one;
       volatile int dummy68424 = 1;
     }
     else
     {
       *ptr_one = (uint32_t)0xAA55AA55;
       char printBuf[25];
       
       if (((uint32_t)ptr_one % 256)== 0)
       {
         sprintf(printBuf, "%09x", (uint32_t)ptr_one);
          lcd_log_line((char*)printBuf);
       }
     } 
   }
   */
   
  // Fills memory from 0x20028F80 - 0x2002FE00 with pattern AA55
  // So you can see at breakpoints how much of heap was used
  /*
  ptr_one = (uint32_t *)0x20028F80;
  while (ptr_one < (uint32_t *)0x2002fe00)
  {
    *ptr_one = (uint32_t)0xAA55AA55;
     ptr_one++;
  }
  */

  dateTimeUTCNow = sysTime.getTime();
  

  String tableName = "AnalogTestValues";
  if (augmentTableNameWithYear)
  {
  tableName += (dateTimeUTCNow.year());
  }

  // RoSchmi: do not delete
  // The following line creates a table in the Azure Storage Account defined in config.h
  az_http_status_code theResult = createTable(myCloudStorageAccountPtr, myX509Certificate, (char *)tableName.c_str());
  

  previousNtpMillis = millis();
}

void loop() 
{
  
  if (loopCounter++ % 10000 == 0)   // Make decisions to send data every 10000 th round and toggle Led to signal that App is running
  {
    volatile uint32_t currentMillis = millis();
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);

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
    else
    {
      dateTimeUTCNow = sysTime.getTime();
      dataContainer.SetNewValue(0, dateTimeUTCNow, ReadAnalogSensor(0));
      dataContainer.SetNewValue(1, dateTimeUTCNow, ReadAnalogSensor(1));
      dataContainer.SetNewValue(2, dateTimeUTCNow, ReadAnalogSensor(2));
      dataContainer.SetNewValue(3, dateTimeUTCNow, ReadAnalogSensor(3));

      if (dataContainer.hasToBeSent())
      {
        // Retrieve edited sample values from container
        SampleValueSet sampleValueSet = dataContainer.getCheckedSampleValues(dateTimeUTCNow);

        // Create time value to be stored in each table row (local time and offset to UTC)

        int timeZoneOffsetUTC = actualTimeIsDST ? TIMEZONE + DSTOFFSET : TIMEZONE;
        //int timeZoneOffsetUTC = ntp.isDST() ? TIMEZONE + DSTOFFSET : TIMEZONE;


        char sampleTime[25] {0};
        createSampleTime(sampleValueSet.LastSendTime, timeZoneOffsetUTC, (char *)sampleTime);

        // Define name of the table (arbitrary name + actual year, like: AnalogTestValues2020)
        String tableName = "AnalogTestValues";  
        if (augmentTableNameWithYear)
        {
          tableName += (dateTimeUTCNow.year());     
        }

    // Create an Array of, here, 5 Properties
    // Each Property consists of the Name, the Value and the Type (here only Edm.String is supported)

    // Besides PartitionKey and RowKey we have 5 properties to be stored in a table row
    // (SampleTime and 4 samplevalues)
    size_t propertyCount = 5;
    EntityProperty AnalogPropertiesArray[5];
    AnalogPropertiesArray[0] = (EntityProperty)TableEntityProperty((char *)"SampleTime", (char *) sampleTime, (char *)"Edm.String");
    AnalogPropertiesArray[1] = (EntityProperty)TableEntityProperty((char *)"T_1", (char *)floToStr(sampleValueSet.SampleValues[0].Value).c_str(), (char *)"Edm.String");
    AnalogPropertiesArray[2] = (EntityProperty)TableEntityProperty((char *)"T_2", (char *)floToStr(sampleValueSet.SampleValues[1].Value).c_str(), (char *)"Edm.String");
    AnalogPropertiesArray[3] = (EntityProperty)TableEntityProperty((char *)"T_3", (char *)floToStr(sampleValueSet.SampleValues[2].Value).c_str(), (char *)"Edm.String");
    AnalogPropertiesArray[4] = (EntityProperty)TableEntityProperty((char *)"T_4", (char *)floToStr(sampleValueSet.SampleValues[3].Value).c_str(), (char *)"Edm.String");
  
    // Create the PartitionKey (special format)
    char partKeySpan[25] {0};
    size_t partitionKeyLength = 0;
    az_span partitionKey = AZ_SPAN_FROM_BUFFER(partKeySpan);
    makePartitionKey(analogTablePartPrefix, augmentPartitionKey, partitionKey, &partitionKeyLength);
    partitionKey = az_span_slice(partitionKey, 0, partitionKeyLength);

    // Create the RowKey (special format)
    char rowKeySpan[25] {0};
    size_t rowKeyLength = 0;
    az_span rowKey = AZ_SPAN_FROM_BUFFER(rowKeySpan);
    makeRowKey(dateTimeUTCNow, rowKey, &rowKeyLength);
    rowKey = az_span_slice(rowKey, 0, rowKeyLength);
  
    // Create TableEntity consisting of PartitionKey, RowKey and the properties named 'SampleTime', 'T_1', 'T_2', 'T_3' and 'T_4'
    AnalogTableEntity analogTableEntity(partitionKey, rowKey, az_span_create_from_str((char *)sampleTime),  AnalogPropertiesArray, propertyCount);
     
    // Print message on display
    char strData[100];
    sprintf(strData, "   Trying to insert %u", insertCounter);   
    lcd_log_line(strData);
    
    // Keep track of tries to insert
    insertCounter++;

    char EtagBuffer[20] {0};
    // Store Entity to Azure Cloud   
    az_http_status_code insertResult = insertTableEntity(myCloudStorageAccountPtr, myX509Certificate, (char *)tableName.c_str(), analogTableEntity, (char *)EtagBuffer);
    }
    
    
    /*
    while (dateTimeUtcNewSeconds != dateTimeUTCNow.secondstime())
    {
      dateTimeUTCNow = actualizeSysTimeFromNtpIfNeeded();
      dateTimeUtcNewSeconds = dateTimeUTCNow.secondstime();
    }
    */

    //systimeNtpDelta = dateTimeUTCOldSeconds - dateTimeUTCNow.secondstime(); 
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
            Serial.println("udp packet received");
            Serial.println("");
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
                        theRead = 0.10;
                        //theRead = analog0.ReadRatio();
                    }
                    break;

                case 1:
                    {
                        theRead = 180.20;
                        //theRead = analog1.ReadRatio();
                    }
                    break;
                case 2:
                    {
                        theRead = -0.30;
                        //theRead = analog2.ReadRatio();
                    }
                    break;
                case 3:
                    {
                        theRead = -65.40;
                        //theRead = analog3.ReadRatio();
                    }
                    break;
            }

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
                        theRead = insertCounter;
                        theRead = theRead / 10;                      
                    }
                    break;
                case 3:
                    {
                        theRead = -65.40;
                        //theRead = analog3.ReadRatio();
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
    wifi_client.setCACert(baltimore_root_ca);
    //wifi_client.setCACert(baltimore_corrupt_root_ca);
  #endif


/*
#if TRANSPORT_PROTOCOL == 1
  wifi_client.setCACert(baltimore_root_ca);
  if (insertCounter == 2)
  {
    wifi_client.setCACert(baltimore_corrupt_root_ca);
  }
#endif
*/


  TableClient table(pAccountPtr, pCaCert,  httpPtr, &wifi_client);
  //TableClient table(pAccountPtr, pCaCert,  httpPtr, wifi_client);

  // Insert Entity
  az_http_status_code statusCode = table.InsertTableEntity(pTableName, pTableEntity,  contApplicationIatomIxml, acceptApplicationIjson, dont_returnContent, false);

  char codeString[35] {0};
  if ((statusCode == AZ_HTTP_STATUS_CODE_NO_CONTENT) || (statusCode == AZ_HTTP_STATUS_CODE_CREATED))
  { 
    sprintf(codeString, "%s %i", "Entity inserted: ", az_http_status_code(statusCode));    
    lcd_log_line((char *)codeString);
    //delete &wifi_client;
    Serial.println((char *)codeString);
  }
  else
  {
    sprintf(codeString, "%s %i", "Insertion failed: ", az_http_status_code(statusCode));   
    lcd_log_line((char *)codeString);
    //delete &wifi_client;
    Serial.println((char *)codeString);
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
  wifi_client.setCACert(baltimore_root_ca);
  //wifi_client.setCACert(baltimore_corrupt_root_ca);
#endif


  TableClient table(pAccountPtr, pCaCert,  httpPtr, &wifi_client);

  // Create Table
  az_http_status_code statusCode = table.CreateTable(pTableName, contApplicationIatomIxml, acceptApplicationIjson, returnContent, false);
  
  char codeString[35] {0};
  if ((statusCode == AZ_HTTP_STATUS_CODE_CONFLICT) || (statusCode == AZ_HTTP_STATUS_CODE_CREATED))
  { 
    sprintf(codeString, "%s %i", "Table available: ", az_http_status_code(statusCode));    
    lcd_log_line((char *)codeString);
  }
  else
  {
    sprintf(codeString, "%s %i", "Table Creation failed: ", az_http_status_code(statusCode));    
    lcd_log_line((char *)codeString);
    delay(5000);
  }
return statusCode;
}






