#include <Arduino.h>
#include <AzureStorage/CloudStorageAccount.h>
#include <HTTPClient.h>
#include <AzureStorage/TableClient.h>
#include <DateTime.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <Encryption/RoSchmi_encryption_helpers.h>
#include <AzureStorage/roschmi_az_storage_tables.h>

#include <AzureStorage/TableEntityProperty.h>
#include <AzureStorage/TableEntity.h>

#include <azure/core/az_span.h>

WiFiClientSecure * _wifiClient;

CloudStorageAccount  * _accountPtr;
HTTPClient * _httpPtr;
const char * _caCert;

static SysTime sysTime;
      
        char * _OperationResponseBody;
        char * _OperationResponseMD5; 
        char * _OperationResponseETag; 

        //private Hashtable _OperationResponseSingleQuery = null;
        //private ArrayList _OperationResponseQueryList = null;

// forward declarations
void GetDateHeader(DateTime, char * stamp, char * x_ms_time);
void appendCharArrayToSpan(az_span targetSpan, const size_t maxTargetLength, const size_t startIndex, size_t *outEndIndex, const char * stringToAppend);
az_span  getContentType_az_span(ContType pContentType);
az_span  getResponseType_az_span(ResponseType pResponseType);
az_span  getAcceptType_az_span(AcceptType pAcceptType);
int base64_decode(const char * input, char * output);
int32_t dow(int32_t year, int32_t month, int32_t day);
void GetTableXml(EntityProperty EntityProperties[], size_t propertyCount, az_span outSpan, size_t *outSpanLength);

void TableClient::CreateTableAuthorizationHeader(const char * content, const char * canonicalResource, const char * ptimeStamp, const char * pHttpVerb, az_span pContentType, char * pMD5HashHex, char * pAutorizationHeader, bool useSharedKeyLite)
{  
    char contentTypeString[25] {0};

    az_span_to_str(contentTypeString, (az_span_size(pContentType) + 1), pContentType);                                                                              
    
    if (!useSharedKeyLite)
    {               
        // How to produce Md5 hash:
        //https://community.mongoose-os.com/t/md5-library-setup-and-config-examples/856

        // create Md5Hash           
        const size_t Md5HashStrLenght = 16 + 1;
        char md5HashStr[Md5HashStrLenght] {0};
        int create_Md5_result = createMd5Hash(md5HashStr, Md5HashStrLenght, content);
                
        // Convert to hex-string
        stringToHexString(pMD5HashHex, md5HashStr, (const char *)"");
        String theString = pMD5HashHex;   
    }
                        
    char toSign[(strlen(canonicalResource) + 100)];  // if counted correctly, at least 93 needed
    if (useSharedKeyLite)
    {       
        sprintf(toSign, "%s\%s", (char *)ptimeStamp, canonicalResource);                      
    }
    else
    {     
        sprintf(toSign, "%s\n%s\n%s\n%s\n%s", pHttpVerb, pMD5HashHex , (char *)contentTypeString, (char *)ptimeStamp, canonicalResource);                       
    }
            
    // Produce Authentication Header
    // 1) Base64 decode the Azure Storage Key
           
    // How to decode Base 64 String
    //https://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/lxr/source/src/connect/mbedtls/
            
    // Base64-decode (Azure Storage Key)

             
    char base64DecOut[80] {0};
    int decodeResult = base64_decode(_accountPtr->AccountKey.c_str(), base64DecOut);               
    size_t decodedKeyLen = (decodeResult != -1) ? decodeResult : 0;
    
                                   
    // 2) SHA-256 encode the canonical resource (Here string of: Accountname and 'Tables')
    //    with base-64 deoded Azure Storage Account Key
    // HMAC SHA-256 encoding
    // https://techtutorialsx.com/2018/01/25/esp32-arduino-applying-the-hmac-sha-256-mechanism/
            
    // create SHA256Hash
             
    const size_t sha256HashBufferLength = 32 + 1;
    char sha256HashStr[sha256HashBufferLength] {0};
    int create_SHA256_result = createSHA256Hash(sha256HashStr, sha256HashBufferLength, toSign, strlen(toSign), base64DecOut, decodedKeyLen); 
    

    // 3) Base-64 encode the SHA-265 encoded canonical resorce
    
    const size_t resultBase64Size = 32 + 20;
    char hmacResultBase64[resultBase64Size] {0};
    base64_encode(sha256HashStr, 32, hmacResultBase64, resultBase64Size);
       
    char retBuf[MAX_ACCOUNTNAME_LENGTH + resultBase64Size + 20] {0};
    

    if (useSharedKeyLite)
    {   
        
        sprintf(retBuf, "%s %s:%s", (char *)"SharedKeyLite", (char *)_accountPtr->AccountName.c_str(), hmacResultBase64);               
        char * ptr = &pAutorizationHeader[0];
        for (size_t i = 0; i < strlen(retBuf); i++) {
            ptr[i] = retBuf[i];
        }
        ptr[strlen(retBuf)] = '\0'; 
        
    }
    else
    {    
           
        sprintf(retBuf, "%s %s:%s", (char *)"SharedKey", (char *)_accountPtr->AccountName.c_str(), hmacResultBase64);                
        char * ptr = &pAutorizationHeader[0];
        for (size_t i = 0; i < strlen(retBuf); i++) {
            ptr[i] = retBuf[i];
        }
        ptr[strlen(retBuf)] = '\0'; 
            
    } 
         
 }


// Constructor
TableClient::TableClient(CloudStorageAccount * account, const char * caCert, HTTPClient * httpClient)

{
    _accountPtr = account;
    _caCert = caCert;
    _httpPtr = httpClient;
    //_wifiClient = wifiClient;
}
TableClient::~TableClient()
{};


void TableClient::send()
{

   
    /*
    String theUrl = "jsonplaceholder.typicode.com";
    String thePath = "/posts?userId=1";


    String theUrl = "prax47.table.core.windows.net";
    String thePath = "/";     
    
    _httpPtr->begin(theUrl, (uint16_t)443, thePath, _caCert);
    int httpCode = _httpPtr->GET();
    if (httpCode > 0) { //Check for the returning code 
      String payload = _httpPtr->getString();
      //lcd_log_line("Http-Code:");
      //lcd_log_line(itoa((int)httpCode, buf, 10));
      int length = payload.length();
      int indexCtr = 0;
      int pageWidth = 30;
      Serial.println(httpCode);
      delay(2000);
      while (indexCtr < length)
      {
        indexCtr += pageWidth;
      } 
     // Serial.println(payload);
    }
    else {
     // Serial.println("Error on HTTP request");
      volatile int dummy2 = 1;
      dummy2++;
    }
    */
}

az_http_status_code TableClient::CreateTable(const char * tableName, ContType pContentType, AcceptType pAcceptType, ResponseType pResponseType, bool useSharedKeyLite)
{
   // limit length of tablename to max_tablename_length
   char * validTableName = (char *)tableName;
   if (strlen(tableName) >  MAX_TABLENAME_LENGTH)
   {
      validTableName[MAX_TABLENAME_LENGTH] = '\0';
   }
   

  //OperationResultsClear();
            
  char x_ms_timestamp[35] {0};
  char timestamp[22] {0};

  GetDateHeader(sysTime.getTime(), timestamp, x_ms_timestamp);

  String timestampUTC = timestamp;
  timestampUTC += ".0000000Z";

  az_span contentTypeAzSpan = getContentType_az_span(pContentType);
  az_span responseTypeAzSpan = getResponseType_az_span(pResponseType);
  az_span acceptTypeAzSpan = getAcceptType_az_span(pAcceptType);

    const char * PROGMEM li1 = "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>";
    const char * PROGMEM li2 = "<entry xmlns:d=\"http://schemas.microsoft.com/ado/2007/08/dataservices\"  ";
    const char * PROGMEM li3 = "xmlns:m=\"http://schemas.microsoft.com/ado/2007/08/dataservices/metadata\" ";
    const char * PROGMEM li4 = "xmlns=\"http://www.w3.org/2005/Atom\"> <id>http://";
          char * li5 = (char *)_accountPtr->AccountName.c_str();
    const char * PROGMEM li6 = ".table.core.windows.net/Tables('";
          char * li7 = (char *)validTableName;
    const char * PROGMEM li8 = "')</id><title /><updated>";
          char * li9 = (char *)timestampUTC.c_str();
    const char * PROGMEM li10 = "</updated><author><name/></author> ";
    const char * PROGMEM li11 = "<content type=\"application/xml\"><m:properties><d:TableName>";
          char * li12 = (char *)validTableName;
    const char * PROGMEM li13 = "</d:TableName></m:properties></content></entry>";
           
  //char addBuffer[600];
  //sprintf(addBuffer, "%s%s%s%s%s%s%s%s%s%s%s%s%s", li1, li2, li3, li4,li5, li6, li7, li8, li9, li10,li11, li12, li13);
  
  
  // Fills memory from 0x20029200 -  with pattern FF
  // So you can see at breakpoints how much of heap was used
  
  uint32_t * ptr_one;
  uint32_t * last_ptr_one;
  ptr_one = (uint32_t *)0x20029200;
  while (ptr_one < (uint32_t *)0x20029580)
  {
    *ptr_one = (uint32_t)0xAA55AA55;
     ptr_one++;
  }
  







  // Create the body of the request
   // To save memory for heap, allocate buffer which can hold 900 bytes at adr 0x20029200
   uint8_t addBuffer[1];
   uint8_t * addBufAddress = (uint8_t *)addBuffer;
   addBufAddress = (uint8_t *)REQUEST_BODY_BUFFER_MEMORY_ADDR;

   az_span startContent_to_upload = az_span_create(addBufAddress, REQUEST_BODY_BUFFER_LENGTH);


   uint8_t remainderBuffer[1];
   uint8_t * remainderBufAddress = (uint8_t *)remainderBuffer;
   az_span remainder = az_span_create(remainderBufAddress, 900);

            remainder = az_span_copy(startContent_to_upload, az_span_create_from_str((char *)li1));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li2));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li3));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li4));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li5));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li6));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li7));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li8));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li9));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li10));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li11));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li12));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li13));
            
   //remainder =  az_span_copy_u8(remainder, 0);
   az_span_copy_u8(remainder, 0);



az_span content_to_upload = az_span_create_from_str((char *)addBufAddress);  




  //az_span content_to_upload = az_span_create_from_str(addBuffer);

  String urlPath = validTableName;
  String TableEndPoint = _accountPtr->UriEndPointTable;
  String Host = _accountPtr->HostNameTable;
  String Url = TableEndPoint + "/Tables()";
  const char * HttpVerb = "POST";

  char accountName_and_Tables[MAX_ACCOUNTNAME_LENGTH + 12];
  sprintf(accountName_and_Tables, "/%s/%s", (char *)_accountPtr->AccountName.c_str(), (char *)"Tables()");
  
  char md5Buffer[32 +1] {0};

  char authorizationHeaderBuffer[100] {0};

  //CreateTableAuthorizationHeader((char *)addBuffer, accountName_and_Tables, (const char *)x_ms_timestamp, HttpVerb, contentTypeAzSpan, md5Buffer, authorizationHeaderBuffer, useSharedKeyLite);
  CreateTableAuthorizationHeader((char *)addBufAddress, accountName_and_Tables, (const char *)x_ms_timestamp, HttpVerb, contentTypeAzSpan, md5Buffer, authorizationHeaderBuffer, useSharedKeyLite);


  //String authorizationHeader = String((char *)authorizationHeaderBuffer);
           
  az_storage_tables_client tabClient;        
  az_storage_tables_client_options options = az_storage_tables_client_options_default();
                        
  if (az_storage_tables_client_init(
          &tabClient, az_span_create_from_str((char *)Url.c_str()), AZ_CREDENTIAL_ANONYMOUS, &options)
      != AZ_OK)
  {
      volatile int dummy643 = 1; 
  }

  uint8_t * responseBufferAddr = (uint8_t *)RESPONSE_BUFFER_MEMORY_ADDR;
  az_span response_az_span = az_span_create(responseBufferAddr, RESPONSE_BUFFER_LENGTH);
  
  az_http_response http_response;
  if (az_http_response_init(&http_response, response_az_span) != AZ_OK)
  {
    volatile int dummy645 = 1; 
  }

  az_storage_tables_upload_options uploadOptions = az_storage_tables_upload_options_default();
  
  uploadOptions._internal.acceptType = acceptTypeAzSpan;
  uploadOptions._internal.contentType = contentTypeAzSpan;
  uploadOptions._internal.perferType = responseTypeAzSpan;

  setHttpClient(_httpPtr);
  setCaCert(_caCert);

  az_result const table_create_result
      = az_storage_tables_upload(&tabClient, content_to_upload, az_span_create_from_str(md5Buffer), az_span_create_from_str((char *)authorizationHeaderBuffer), az_span_create_from_str((char *)x_ms_timestamp), &uploadOptions, &http_response);

  az_http_response_status_line statusLine;

  az_result result = az_http_response_get_status_line(&http_response, &statusLine);
  
 return statusLine.status_code;          
}
        

 az_http_status_code TableClient::InsertTableEntity(const char * tableName, TableEntity pEntity, ContType pContentType, AcceptType pAcceptType, ResponseType pResponseType, bool useSharedKeyLite)
{
  char * validTableName = (char *)tableName;
  if (strlen(tableName) >  MAX_TABLENAME_LENGTH)
  {
    validTableName[MAX_TABLENAME_LENGTH] = '\0';
  }

  char PartitionKey[15] {0};
  az_span_to_str(PartitionKey, sizeof(PartitionKey) - 1, pEntity.PartitionKey);
 
  char RowKey[16] {0};
  az_span_to_str(RowKey, sizeof(RowKey), pEntity.RowKey);
  
  char SampleTime[26] {0};
  az_span_to_str(SampleTime, sizeof(SampleTime), pEntity.SampleTime);
    
  char x_ms_timestamp[35] {0};
  char timestamp[22] {0};

  GetDateHeader(sysTime.getTime(), timestamp, x_ms_timestamp);
  String timestampUTC = timestamp;
  timestampUTC += ".0000000Z";

  az_span contentTypeAzSpan = getContentType_az_span(pContentType);
  az_span responseTypeAzSpan = getResponseType_az_span(pResponseType);
  az_span acceptTypeAzSpan = getAcceptType_az_span(pAcceptType);

// To save memory allocate buffer to address 0x20029000
uint8_t * BufAddress = (uint8_t *)PROPERTIES_BUFFER_MEMORY_ADDR;
az_span outPropertySpan = az_span_create(BufAddress, PROPERTIES_BUFFER_LENGTH);
size_t outBytesWritten = 0;

GetTableXml(pEntity.Properties, pEntity.PropertyCount, outPropertySpan, &outBytesWritten);
   
char * _properties = (char *)az_span_ptr(outPropertySpan);

const char * HttpVerb = "POST";
   
  const char * PROGMEM li1 = "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>";
  const char * PROGMEM li2 = "<entry xmlns:d=\"http://schemas.microsoft.com/ado/2007/08/dataservices\"  ";
  const char * PROGMEM li3 = "xmlns:m=\"http://schemas.microsoft.com/ado/2007/08/dataservices/metadata\" ";    
  const char * PROGMEM li4  = "xmlns=\"http://www.w3.org/2005/Atom\"> <id>http://";
        char * li5  = (char *)_accountPtr->AccountName.c_str();
  const char * PROGMEM li6  = ".table.core.windows.net/";
        char *  li7  = (char *)validTableName;
  const char * PROGMEM li8  = "(PartitionKey='";
        char * li9  = (char *)PartitionKey; 
  const char * PROGMEM li10  = "',RowKey='";
        char * li11  = (char *)RowKey; 
  const char * PROGMEM li12  = "')</id><title /><updated>";
        char * li13  = (char *)x_ms_timestamp;
  const char * PROGMEM li14  = "</updated><author><name /></author><content type=\"application/atom+xml\">";
  const char * PROGMEM li15  = "<m:properties><d:PartitionKey>";
        char * li16  = (char *)PartitionKey;
  const char * PROGMEM li17  =  "</d:PartitionKey><d:RowKey>";
        char * li18  = (char *)RowKey;
  const char * PROGMEM li19  = "</d:RowKey>";  
        char * li20  = _properties;
  const char * PROGMEM li21  = "</m:properties></content></entry>";


  // Fills memory from 0x20029200 -  with pattern FF
  // So you can see at breakpoints how much of heap was used
  
  uint32_t * ptr_one;
  uint32_t * last_ptr_one;
  ptr_one = (uint32_t *)0x20029200;
  while (ptr_one < (uint32_t *)0x20029580)
  {
    *ptr_one = (uint32_t)0xAA55AA55;
     ptr_one++;
  }
  
   // Create the body of the request
   // To save memory for heap, allocate buffer which can hold 900 bytes at adr 0x20029200
   uint8_t addBuffer[1];
   uint8_t * addBufAddress = (uint8_t *)addBuffer;
   addBufAddress = (uint8_t *)REQUEST_BODY_BUFFER_MEMORY_ADDR;

   az_span startContent_to_upload = az_span_create(addBufAddress, REQUEST_BODY_BUFFER_LENGTH);

   uint8_t remainderBuffer[1];
   uint8_t * remainderBufAddress = (uint8_t *)remainderBuffer;
   az_span remainder = az_span_create(remainderBufAddress, 900);

            remainder = az_span_copy(startContent_to_upload, az_span_create_from_str((char *)li1));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li2));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li3));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li4));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li5));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li6));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li7));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li8));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li9));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li10));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li11));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li12));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li13));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li14));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li15));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li16));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li17));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li18));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li19));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li20));
            remainder = az_span_copy(remainder, az_span_create_from_str((char *)li21));
    
//remainder = az_span_copy_u8(remainder, 0);
az_span_copy_u8(remainder, 0);
  
  // This is how concatenation of the strings was done before
  // It made the application crash in building sha512 hash
  //char * contToUpl = (char *)addBufAddress;
  //String toUpload = contToUpl;
  //char addBuffer[650];
  //char addBuffer[700];
  //sprintf(addBuffer, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", li1, li2, li3, li4,li5, li6, li7, li8, li9, li10,li11, li12, li13, li14, li15, li16, li17, li18, li19, li20, li21); 
 
  az_span content_to_upload = az_span_create_from_str((char *)addBufAddress);   
             
   String urlPath = validTableName;
   String TableEndPoint = _accountPtr->UriEndPointTable;
   String Host = _accountPtr->HostNameTable;

   String Url = TableEndPoint + "/" + urlPath + "()";
   
  char accountName_and_Tables[MAX_ACCOUNTNAME_LENGTH + MAX_TABLENAME_LENGTH + 10];
  sprintf(accountName_and_Tables, "/%s/%s%s", (char *)_accountPtr->AccountName.c_str(), validTableName, (const char *)"()");
  
  // Create buffers to hold the results of MD5-hash and the value of the authorizationheader
  char md5Buffer[32 +1] {0};
  char authorizationHeaderBuffer[65] {0};

  CreateTableAuthorizationHeader((char *)addBufAddress, accountName_and_Tables, (const char *)x_ms_timestamp, HttpVerb, contentTypeAzSpan, md5Buffer, authorizationHeaderBuffer, useSharedKeyLite);

  // Create client to handle request    
  az_storage_tables_client tabClient;        
  az_storage_tables_client_options options = az_storage_tables_client_options_default();

  // Init Client (set Url)                      
  if (az_storage_tables_client_init(
      &tabClient, az_span_create_from_str((char *)Url.c_str()), AZ_CREDENTIAL_ANONYMOUS, &options)
      != AZ_OK)
  {
      // possible breakpoint, if some something went wrong
      volatile int dummy643 = 1;    
  }
  
  // To save memory set buffer address to 0x2002A000
   uint8_t * responseBufferAddr = (uint8_t *)RESPONSE_BUFFER_MEMORY_ADDR;
  az_span response_az_span = az_span_create(responseBufferAddr, RESPONSE_BUFFER_LENGTH);
  //az_span_fill(response_az_span, 0);

  az_http_response http_response;
  if (az_result_failed(az_http_response_init(&http_response, response_az_span)))
  {
     volatile int dummy644 = 1;
  }
   
  az_storage_tables_upload_options uploadOptions = az_storage_tables_upload_options_default();
  
  uploadOptions._internal.acceptType = acceptTypeAzSpan;
  uploadOptions._internal.contentType = contentTypeAzSpan;
  uploadOptions._internal.perferType = responseTypeAzSpan;

  //set HTTPClient and certificate
  setHttpClient(_httpPtr);
  setCaCert(_caCert);

  az_result const entity_upload_result
      = az_storage_tables_upload(&tabClient, content_to_upload, az_span_create_from_str(md5Buffer), az_span_create_from_str((char *)authorizationHeaderBuffer), az_span_create_from_str((char *)x_ms_timestamp), &uploadOptions, &http_response);
    
    az_http_response_status_line statusLine;

    az_result result = az_http_response_get_status_line(&http_response, &statusLine);    

    az_span etag = AZ_SPAN_FROM_STR("ETag");
    char keyBuf[20] {0};
    az_span headerKey = AZ_SPAN_FROM_BUFFER(keyBuf);
    char valueBuf[50] {0};
    az_span headerValue = AZ_SPAN_FROM_BUFFER(valueBuf);
    
    for (int i = 0; i < 5; i++)
    {
      az_result headerResult = az_http_response_get_next_header(&http_response, &headerKey, &headerValue);
      if (az_span_is_content_equal(headerKey, etag))
      {
        break;
      }
    }

    
    return statusLine.status_code; 
}      

void appendCharArrayToSpan(az_span targetSpan, const size_t maxTargetLength, const size_t startIndex, size_t *outEndIndex, const char * stringToAppend)
{
    uint8_t * lastPtr = targetSpan._internal.ptr;
    int32_t lastSize = targetSpan._internal.size;
    az_span_copy(targetSpan, az_span_create_from_str((char *)stringToAppend));
    targetSpan = (az_span){ ._internal = { .ptr = lastPtr + strlen(stringToAppend), .size = lastSize - (int32_t)strlen(stringToAppend) } };   
}

void GetTableXml(EntityProperty EntityProperties[], size_t propertyCount, az_span outSpan, size_t *outSpanLength)
{ 
  char prop[(MAX_ENTITYPROPERTY_NAME_LENGTH * 2) + MAX_ENTITYPROPERTY_VALUE_LENGTH + MAX_ENTITYPROPERTY_TYPE_LENGTH + 20] {0};          
                
  size_t outLength = 0;
  for (size_t i = 0; i < propertyCount; i++)
  {
    // RoSchmi ToDo: define precondition                  
    sprintf(prop, "<d:%s m:type=%c%s%c>%s</d:%s>", EntityProperties[i].Name, '"', EntityProperties[i].Type, '"', EntityProperties[i].Value, EntityProperties[i].Name);
    if ((prop != NULL) && (strlen(prop) > 0))
    {                 
      outSpan = az_span_copy(outSpan, az_span_create_from_str((char *)prop));
      outLength += strlen((char *)prop);    
    }           
  }
  az_span_copy_u8(outSpan, 0);
  *outSpanLength = outLength;
}
        
void GetDateHeader(DateTime time, char * stamp, char * x_ms_time)
{
  int32_t dayOfWeek = dow((int32_t)time.year(), (int32_t)time.month(), (int32_t)time.day());

  dayOfWeek = dayOfWeek == 7 ? 0 : dayOfWeek;  // Switch Sunday, it comes as 7 and must be 0

  struct tm timeinfo {
                      (int)time.second(),
                      (int)time.minute(),
                      (int)time.hour(),
                      (int)time.day(),
                      (int)time.month(),
                      (int)(time.year() - 1900),                       
                      (int)dayOfWeek,
                      0,                               
                      0};

  timeinfo.tm_mon =  (int)time.month() -1;
                  
  strftime((char *)x_ms_time, 35, "%a, %d %b %Y %H:%M:%S GMT", &timeinfo);
  strftime((char *)stamp, 22, "%Y-%m-%dT%H:%M:%S", &timeinfo);
       
}

az_span getContentType_az_span(ContType pContentType)
{           
  if (pContentType == contApplicationIatomIxml)
  { 
    return AZ_SPAN_LITERAL_FROM_STR("application/atom+xml");          
  }
  else
  { 
    return AZ_SPAN_LITERAL_FROM_STR("application/json"); 
  }
}

az_span getAcceptType_az_span(AcceptType pAcceptType)
{
  if (pAcceptType == acceptApplicationIatomIxml)
  { return AZ_SPAN_LITERAL_FROM_STR("application/atom+xml"); }
  else
  { return AZ_SPAN_LITERAL_FROM_STR("application/json"); }
}
        
az_span getResponseType_az_span(ResponseType pResponseType)
{
  if (pResponseType == returnContent)
  { return AZ_SPAN_LITERAL_FROM_STR("return-content"); }
  else
  { return AZ_SPAN_LITERAL_FROM_STR("return-no-content"); }
}

/* Returns the number of days to the start of the specified year, taking leap
 * years into account, but not the shift from the Julian calendar to the
 * Gregorian calendar. Instead, it is as though the Gregorian calendar is
 * extrapolated back in time to a hypothetical "year zero".
 */
int leap (int year)
{
  return year*365 + (year/4) - (year/100) + (year/400);
}

/* Returns a number representing the number of days since March 1 in the
 * hypothetical year 0, not counting the change from the Julian calendar
 * to the Gregorian calendar that occured in the 16th century. This
 * algorithm is loosely based on a function known as "Zeller's Congruence".
 * This number MOD 7 gives the day of week, where 0 = Monday and 6 = Sunday.
 */
int32_t zeller (int32_t year, int32_t month, int32_t day)
{
  year += ((month+9)/12) - 1;
  month = (month+9) % 12;
  return leap (year) + month*30 + ((6*month+5)/10) + day + 1;
}

/* Returns the day of week (1=Monday, 7=Sunday) for a given date.
 */
int32_t dow (int32_t year, int32_t month, int32_t day)
{
  return (zeller (year, month, day) % 7) + 1;
}
