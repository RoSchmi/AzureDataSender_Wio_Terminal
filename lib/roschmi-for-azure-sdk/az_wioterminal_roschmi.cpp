// Copyright of template
// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

// Copyright (c) RoSchmi. All rights reserved.
// SPDX-License-Identifier: MIT

#include <azure/core/az_http.h>
#include <azure/core/az_http_transport.h>
#include <azure/core/az_span.h>
#include <azure/core/internal/az_result_internal.h>
#include <azure/core/internal/az_span_internal.h>
#include <stdlib.h>
#include <Arduino.h>
#include <string.h>
#include <azure/core/az_platform.h>
#include <az_wioterminal_roschmi.h>

/*
#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
*/

HTTPClient *  deviceHttp = NULL;
WiFiClientSecure * deviceWifiClient = NULL;

const char * _caCertificate;


/**
 * @brief uses AZ_HTTP_BUILDER to set up request and perform it.
 *
 * @param request an internal http builder with data to build and send http request
 * @param ref_response pre-allocated buffer where http response will be written
 * @return az_result
 */
AZ_NODISCARD az_result
az_http_client_send_request(az_http_request const* request, az_http_response* ref_response)
{
  _az_PRECONDITION_NOT_NULL(request);
  _az_PRECONDITION_NOT_NULL(ref_response);

// Working with spans
//https://github.com/Azure/azure-sdk-for-c/tree/master/sdk/docs/core#working-with-spans
  
  az_http_method requMethod = request->_internal.method;
  int32_t max_header_count = request->_internal.max_headers;
  size_t headerCount = az_http_request_headers_count(request);

 // int32_t theQueryStart = request->_internal.query_start;

// Code to copy all headers into one string, actually not needed
/*
uint8_t headers_buffer[300] {0};
  az_span headers_span = AZ_SPAN_FROM_BUFFER(headers_buffer);
az_result myResult = dev_az_http_client_build_headers(request, headers_span);
char* theHeader_str = (char*) az_span_ptr(headers_span);
volatile size_t headerSize = strlen(theHeader_str);
*/
    
    //az_span_to_str(char* destination, int32_t destination_max_size, az_span source);
    
    az_span urlWorkCopy = request->_internal.url;

    int32_t colonIndex = az_span_find(urlWorkCopy, AZ_SPAN_LITERAL_FROM_STR(":"));
		
    char protocol[6] {0};
    urlWorkCopy = request->_internal.url;
  
    bool protocolIsHttpOrHttps = false;
    int32_t slashIndex = - 1;
    if (colonIndex != -1)
    {
        az_span_to_str(protocol, 6, az_span_slice(urlWorkCopy, 0, colonIndex));
        if ((strcmp(protocol, (const char *)"https") == 0) || (strcmp(protocol, (const char *)"http") == 0))
        {
            protocolIsHttpOrHttps = true;
        }

        slashIndex = az_span_find(az_span_slice_to_end(urlWorkCopy, colonIndex + 3), AZ_SPAN_LITERAL_FROM_STR("/"));
        
        slashIndex = (slashIndex != -1) ? slashIndex + colonIndex + 3 : -1;       
    }
    

    char workBuffer[100] {0};
    
    if (slashIndex == -1)
    {
      az_span_to_str(workBuffer, sizeof(workBuffer), az_span_slice_to_end(urlWorkCopy, colonIndex + 3));
    }
    else
    {
       az_span_to_str(workBuffer, sizeof(workBuffer), az_span_slice_to_end(az_span_slice(urlWorkCopy, 0, slashIndex), colonIndex + 3));
    }
    String host = (const char *)workBuffer;

    if (slashIndex != -1)
    {
      memset(workBuffer, 0, sizeof(workBuffer));
      az_span_to_str(workBuffer, sizeof(workBuffer), az_span_slice_to_end(urlWorkCopy, slashIndex));
    }
    String resource = slashIndex != -1 ? (const char *)workBuffer : "";


    uint16_t port = (strcmp(protocol, (const char *)"http") == 0) ? 80 : 443;

    //deviceHttp->setReuse(true);

    if (port == 80)
    {
      
      deviceHttp->begin(host, port, resource);
    }
    else
    {
      deviceHttp->begin(host, port, resource, _caCertificate);
    }
    
    char name_buffer[MAX_HEADERNAME_LENGTH +2] {0};
    char value_buffer[MAX_HEADERVALUE_LENGTH +2] {0};
    az_span head_name = AZ_SPAN_FROM_BUFFER(name_buffer);
    az_span head_value = AZ_SPAN_FROM_BUFFER(value_buffer);
    
    String nameString = "";
    String valueString = "";

    for (int32_t offset = (headerCount - 1); offset >= 0; offset--)
    {
      _az_RETURN_IF_FAILED(az_http_request_get_header(request, offset, &head_name, &head_value));
      
      az_span_to_str((char *)name_buffer, MAX_HEADERNAME_LENGTH -1, head_name);
      az_span_to_str((char *)value_buffer, MAX_HEADERVALUE_LENGTH -1, head_value);
      nameString = (char *)name_buffer;
      valueString = (char *)value_buffer;

      deviceHttp->addHeader(nameString, valueString, true, true);   
    }

    int32_t bodySize = request->_internal.body._internal.size;
    char theBody[bodySize + 10] {0};
    
    az_span_to_str((char *)theBody, bodySize + 1, request->_internal.body);
    
    if (az_span_is_content_equal(requMethod, AZ_SPAN_LITERAL_FROM_STR("POST")))
    {       
        const char * headerKeys[] = {"ETag", "Date", "x-ms-request-id", "x-ms-version", "Content-Type"};       
        deviceHttp->collectHeaders(headerKeys, 5);
      
        int httpCode = deviceHttp->POST((char *)theBody);
          
        delay(1); 
        

        volatile size_t responseBodySize = deviceHttp->getSize();
        size_t respHeaderCount = deviceHttp->headers();      
       

        int indexCtr = 0;
        int pageWidth = 50;
        
        delay(2000);
        
        az_result appendResult;
        char httpStatusLine[20] {0};
        if (httpCode > 0)  // Request was successful
        {      
          sprintf((char *)httpStatusLine, "%s%i%s", "HTTP/1.1 ", httpCode, " ***\r\n");
          appendResult = az_http_response_append(ref_response, az_span_create_from_str((char *)httpStatusLine));
           
          for (size_t i = 0; i < respHeaderCount; i++)
          {       
            appendResult = az_http_response_append(ref_response, az_span_create_from_str((char *)deviceHttp->headerName(i).c_str()));          
            appendResult = az_http_response_append(ref_response, az_span_create_from_str((char *)": "));
            appendResult = az_http_response_append(ref_response, az_span_create_from_str((char *)deviceHttp->header(i).c_str()));
            appendResult = az_http_response_append(ref_response, az_span_create_from_str((char *)"\r\n"));
          }
          appendResult = az_http_response_append(ref_response, az_span_create_from_str((char *)"\r\n"));
          appendResult = az_http_response_append(ref_response, az_span_create_from_str((char *)deviceHttp->getString().c_str()));
        }
        else
        {
          // Request failed, set StatusCode to 400
          sprintf((char *)httpStatusLine, "%s%i%s", "HTTP/1.1 ", 400, " ***\r\n");
          appendResult = az_http_response_append(ref_response, az_span_create_from_str((char *)httpStatusLine));
          appendResult = az_http_response_append(ref_response, az_span_create_from_str((char *)"\r\n"));
          appendResult = az_http_response_append(ref_response, az_span_create_from_str((char *)"Request failed\r\n\0"));
        }

        deviceHttp->end();
        
        /*
        char buffer[1000];
        az_span content = AZ_SPAN_FROM_BUFFER(buffer);

        az_http_response_get_body(ref_response, &content);

        volatile int dummy349 = 1;
        */

        //  Here you can see the received payload in chunks 
        // if you set a breakpoint in the loop 
        
        /*
        String partMessage;
        while (indexCtr < length)
        {
          partMessage = payload.substring(indexCtr, indexCtr + pageWidth);
          indexCtr += pageWidth;
        } 
        */
    }
    else
    {
      if (az_span_is_content_equal(requMethod, AZ_SPAN_LITERAL_FROM_STR("GET")))
      {
        volatile int dummy7jsgj = 1;
      }
      else
      {
        volatile int dummy7jdgj = 1;
      }
      
    }
    
    
    //delay(500);
   return AZ_OK;
}

// AZ_NODISCARD int64_t az_platform_clock_msec() { return 0; }

// For Arduino (Wio Terminal)
// this defines clockCyclesPerMicrosecond if its not already defined
AZ_NODISCARD int64_t az_platform_clock_msec() 
{
  //#ifndef clockCyclesPerMicrosecond()
  //  #define clockCyclesPerMicrosecond() ( F_CPU / 1000000L )
  //#endif
  
   return (int64_t)((int64_t)clockCyclesPerMicrosecond() * 1000L); 

}

//void az_platform_sleep_msec(int32_t milliseconds) { (void)milliseconds; }
// For Arduino (Wio Terminal):
void az_platform_sleep_msec(int32_t milliseconds) 
{ 
  delay(milliseconds); 
}

void setHttpClient(HTTPClient * httpClient)
{
  if (deviceHttp == NULL)
  {
      deviceHttp = httpClient;
  }
}

void setCaCert(const char * caCert)
{
  _caCertificate = caCert;
}

/**
 * @brief loop all the headers from a HTTP request and compine all headers into one az_span
 *
 * @param request an http builder request reference
 * @param ref_headers list of headers
 * @return az_result
 */

static AZ_NODISCARD az_result
dev_az_http_client_build_headers(az_http_request const* request, az_span ref_headers)
{
  _az_PRECONDITION_NOT_NULL(request);
  
  az_span header_name = { 0 };
  az_span header_value = { 0 };
  uint8_t header_buffer[request->_internal.url_length - 20 + 60] {0};
  az_span header_span = AZ_SPAN_FROM_BUFFER(header_buffer);
  az_span separator = AZ_SPAN_LITERAL_FROM_STR(": ");
  

  for (int32_t offset = 0; offset < az_http_request_headers_count(request); ++offset)
  {
    _az_RETURN_IF_FAILED(az_http_request_get_header(request, offset, &header_name, &header_value));   
    _az_RETURN_IF_FAILED(dev_az_span_append_header_to_buffer(header_span, header_name, header_value, separator));
    char* header_str = (char*) az_span_ptr(header_span); // str points to a 0-terminated string
    ref_headers = az_span_copy(ref_headers, az_span_create_from_str(header_str));
    ref_headers = az_span_copy(ref_headers, AZ_SPAN_LITERAL_FROM_STR("\r\n"));
  }
  az_span_copy_u8(ref_headers, 0);
  return AZ_OK;
}

/**
 * @brief writes a header key and value to a buffer as a 0-terminated string and using a separator
 * span in between. Returns error as soon as any of the write operations fails
 *
 * @param writable_buffer pre allocated buffer that will be used to hold header key and value
 * @param header_name header name
 * @param header_value header value
 * @param separator symbol to be used between key and value
 * @return az_result
 */
static AZ_NODISCARD az_result dev_az_span_append_header_to_buffer(
    az_span writable_buffer,
    az_span header_name,
    az_span header_value,
    az_span separator)
{
  int32_t required_length
      = az_span_size(header_name) + az_span_size(separator) + az_span_size(header_value) + 1;

  _az_RETURN_IF_NOT_ENOUGH_SIZE(writable_buffer, required_length);

  writable_buffer = az_span_copy(writable_buffer, header_name);
  writable_buffer = az_span_copy(writable_buffer, separator);
  writable_buffer = az_span_copy(writable_buffer, header_value);
  az_span_copy_u8(writable_buffer, 0);
  return AZ_OK;
}
