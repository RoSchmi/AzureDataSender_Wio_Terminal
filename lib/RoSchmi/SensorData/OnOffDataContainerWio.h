#include <Arduino.h>
#include <Datetime.h>

#ifndef _ON_OFF_DATACONTAINERWIO_H_
#define _ON_OFF_DATACONTAINERWIO_H_

#define PROPERTY_COUNT 4

typedef struct
{
    bool  actState = false;
    bool  lastState = true;
    bool  hasToBeSent = false;
    DateTime LastSwitchTime;
    char tableName[50];
}
OnOffSampleValue; 
    
typedef struct
{  
    //DateTime LastSendTime;
    OnOffSampleValue OnOffSampleValues[PROPERTY_COUNT];
}
OnOffSampleValueSet;

class OnOffDataContainerWio
{

public:
    OnOffDataContainerWio();
    
    void begin(DateTime time, const char * tableName_1, const char * tableName_2, const char * tableName_3, const char * tableName_4);
    void SetNewOnOffValue(int sensorIndex, bool state, DateTime time);
    void Reset_hasToBeSent(int sensorIndex);
    bool One_hasToBeBeSent();
     
    OnOffSampleValueSet GetOnOffValueSet();   
};

#endif  // _ON_OFF_DATACONTAINERWIO_H_