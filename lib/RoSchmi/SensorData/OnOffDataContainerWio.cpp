#include <Arduino.h>
#include "OnOffDataContainerWio.h"
//#include "math.h"
//#include "stdlib.h"
#include "DateTime.h"

OnOffSampleValueSet onOffSampleValueSet;


OnOffDataContainerWio::OnOffDataContainerWio()
{}

void OnOffDataContainerWio::begin(DateTime lastSwitchTime, const char * tableName_1, 
const char * tableName_2, const char * tableName_3, const char * tableName_4)
{  
    strcpy(onOffSampleValueSet.OnOffSampleValues[0].tableName, tableName_1);
    strcpy(onOffSampleValueSet.OnOffSampleValues[1].tableName, tableName_2);
    strcpy(onOffSampleValueSet.OnOffSampleValues[2].tableName, tableName_3);
    strcpy(onOffSampleValueSet.OnOffSampleValues[3].tableName, tableName_4);

    for (int i = 0; i < PROPERTY_COUNT; i++)
    {
        onOffSampleValueSet.OnOffSampleValues[i].actState = false;
        onOffSampleValueSet.OnOffSampleValues[i].lastState = true;
        onOffSampleValueSet.OnOffSampleValues[i].hasToBeSent = false;
        onOffSampleValueSet.OnOffSampleValues[i].LastSwitchTime = lastSwitchTime;
    }
}


void OnOffDataContainerWio::SetNewOnOffValue(int sensorIndex, bool state, DateTime time)
{
     if (state != onOffSampleValueSet.OnOffSampleValues[sensorIndex].actState)
     {
        onOffSampleValueSet.OnOffSampleValues[sensorIndex].lastState = !state;
        onOffSampleValueSet.OnOffSampleValues[sensorIndex].actState = state;
        onOffSampleValueSet.OnOffSampleValues[sensorIndex].LastSwitchTime = time;
        onOffSampleValueSet.OnOffSampleValues[sensorIndex].hasToBeSent = true;
     }
} 

OnOffSampleValueSet OnOffDataContainerWio::GetOnOffValueSet()
{
    return onOffSampleValueSet;
}

void OnOffDataContainerWio::Reset_hasToBeSent(int sensorIndex)
{
    onOffSampleValueSet.OnOffSampleValues[sensorIndex].hasToBeSent = false;  
}

bool OnOffDataContainerWio::One_hasToBeBeSent()
{
    bool ret = false;
    for (int i = 0; i < PROPERTY_COUNT; i++)
    {
        ret = onOffSampleValueSet.OnOffSampleValues[i].hasToBeSent == true ? true : ret;
    }
    return ret;
}

