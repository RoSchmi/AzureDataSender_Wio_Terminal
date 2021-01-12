#include <Arduino.h>
#include "DataContainerWio.h"
//#include "math.h"
//#include "stdlib.h"
#include "DateTime.h"

SampleValue DataContainerWio::checkedSampleValue(SampleValue inSampleValue, float lowerLimit, float upperLimit, float invalidateSubstitute, DateTime actDateTime, TimeSpan invalidateTime)
{
    SampleValue workSampleValue = inSampleValue;
    workSampleValue.Value = ((inSampleValue.Value < lowerLimit) || (inSampleValue.Value > upperLimit)) ? invalidateSubstitute : inSampleValue.Value;
    if (actDateTime.operator-(invalidateTime).operator>=(workSampleValue.LastSendTime))
    {
        workSampleValue.Value = invalidateSubstitute;
    }
    return workSampleValue;
}

DataContainerWio::DataContainerWio(TimeSpan pSendInterval, TimeSpan pInvalidateInterval, float pLowerLimit, float pUpperLimit, float pMagicNumberInvalid)
{
    SendInterval = pSendInterval,
    InvalidateInterval = pInvalidateInterval;
    LowerLimit = pLowerLimit;
    UpperLimit = pUpperLimit;
    MagicNumberInvalid = pMagicNumberInvalid;
}

void DataContainerWio::SetNewValue(uint32_t pIndex, DateTime pActDateTime, float pSampleValue)
{
    SampleValues[pIndex].Value = pSampleValue;
    SampleValues[pIndex].LastSendTime = pActDateTime;

    if (_isFirstTransmission)
    {       
        _hasToBeSent = true;
        _lastSentTime = pActDateTime;         
    }
    else
    {
        if (_lastSentTime.operator<=(pActDateTime.operator-(SendInterval)))
        {
            _hasToBeSent = true;
        }       
    }   
}

void DataContainerWio::Set_Year(uint16_t year)
{
    Year = year;
}
void DataContainerWio::setUpperLimit(float pUpperLimit)
{
    UpperLimit = pUpperLimit;
}

void DataContainerWio::setLowerLimit(float pLowerLimit)
{
    LowerLimit = pLowerLimit;
}

void DataContainerWio::setMagigNumberInvalid(float pMagicNumberInvalid)
{
   MagicNumberInvalid = pMagicNumberInvalid;
}

bool DataContainerWio::hasToBeSent()
{
    return _hasToBeSent;
}

SampleValueSet DataContainerWio::getSampleValues(DateTime pActDateTime)
{
     _hasToBeSent = false;
    _isFirstTransmission = false;
    _SampleValuesSet.LastSendTime = _lastSentTime;
    _lastSentTime = pActDateTime;
    _SampleValuesSet.SampleValues[0] = SampleValues[0];
    _SampleValuesSet.SampleValues[1] = SampleValues[1];
    _SampleValuesSet.SampleValues[2] = SampleValues[2];
    _SampleValuesSet.SampleValues[3] = SampleValues[3];
    return _SampleValuesSet;
}

SampleValueSet DataContainerWio::getCheckedSampleValues(DateTime pActDateTime)
{
    _hasToBeSent = false;
    _isFirstTransmission = false;
    _SampleValuesSet.LastSendTime = _lastSentTime;
    _lastSentTime = pActDateTime;
   
   _SampleValuesSet.SampleValues[0] = checkedSampleValue(SampleValues[0], LowerLimit, UpperLimit, MagicNumberInvalid, pActDateTime, InvalidateInterval);
   _SampleValuesSet.SampleValues[1] = checkedSampleValue(SampleValues[1], LowerLimit, UpperLimit, MagicNumberInvalid, pActDateTime, InvalidateInterval);
   _SampleValuesSet.SampleValues[2] = checkedSampleValue(SampleValues[2], LowerLimit, UpperLimit, MagicNumberInvalid, pActDateTime, InvalidateInterval);
   _SampleValuesSet.SampleValues[3] = checkedSampleValue(SampleValues[3], LowerLimit, UpperLimit, MagicNumberInvalid, pActDateTime, InvalidateInterval);
     
    return _SampleValuesSet;        
}

