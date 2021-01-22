#include <HTTPClient.h>
#include <Arduino.h>
#include <azure/core/az_http_transport.h>

#ifndef _AZ_WIO_TERMINAL_ROSCHMI_H_
#define _AZ_WIO_TERMINAL_ROSCHMI_H_

#define MAX_HEADERNAME_LENGTH 30
#define MAX_HEADERVALUE_LENGTH 120





void setHttpClient(HTTPClient * httpClient);
void setCaCert(const char * caCert);
void setWiFiClient(WiFiClient * wifiClient);

static az_result dev_az_http_client_build_headers(
    az_http_request const* request, 
    az_span headers_span); 
    

static az_result dev_az_span_append_header_to_buffer(
    az_span writable_buffer,
    az_span header_name,
    az_span header_value,
    az_span separator);
    
#endif