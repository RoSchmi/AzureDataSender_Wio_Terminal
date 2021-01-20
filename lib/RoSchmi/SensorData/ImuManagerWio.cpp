#include <Arduino.h>
#include "ImuManagerWio.h"
//#include "DateTime.h"

ImuSampleValueSet sampleSet;

ImuManagerWio::ImuManagerWio()
{}

void ImuManagerWio::begin()
{}

ImuSampleValues   ImuManagerWio::GetStates()
{

}

void ImuManagerWio::SetNewImuReadings(ImuSampleValues imuReadings)
{
    if (currentIndex == IMU_ARRAY_ELEMENT_COUNT - 1)
    {
         averageIsReady = true;
         currentIndex = 0;
    }
    else
    {
        currentIndex++;
    }
    sampleSet.ImuSampleSet[currentIndex].X_Read = imuReadings.X_Read;
    sampleSet.ImuSampleSet[currentIndex].Y_Read = imuReadings.Y_Read;
    sampleSet.ImuSampleSet[currentIndex].Z_Read = imuReadings.Z_Read;
    /*
    Serial.println(sampleSet.ImuSampleSet[currentIndex].X_Read);
    Serial.println(sampleSet.ImuSampleSet[currentIndex].Y_Read);
    Serial.println(sampleSet.ImuSampleSet[currentIndex].Z_Read);
    Serial.println("");
    */


}

ImuSampleValues ImuManagerWio::GetLastImuReadings()
{
    ImuSampleValues retValues;
    retValues.X_Read = 0;
    retValues.Y_Read = 0;
    retValues.Z_Read = 0;

    return isActive ? sampleSet.ImuSampleSet[currentIndex] : retValues;
}

float ImuManagerWio::GetVibrationValue()
{
    float retValue = 0;
    if (!isActive)
    {
        return 0;
    }
    else
    {
        
        for (int i = 0; i < IMU_ARRAY_ELEMENT_COUNT; i++)
        {
            //retValue += abs(sampleSet.ImuSampleSet[i].X_Read);
            retValue += abs(sampleSet.ImuSampleSet[i].Y_Read);
            //retValue += abs(sampleSet.ImuSampleSet[i].Z_Read);
        }
        return retValue / IMU_ARRAY_ELEMENT_COUNT;

    } 
}

void ImuManagerWio::SetInactive()
{
    isActive = false;
}

void ImuManagerWio::SetActive()
{
    isActive = true;
}



