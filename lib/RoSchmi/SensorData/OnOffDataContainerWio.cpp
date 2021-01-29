#include "OnOffDataContainerWio.h"

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

// If state is different to the value it had before, actstate is changed and lastState
// is set to the value which actstate was before
// LastSwitchTime is set to the value passed in parameter time
// The hasToBeSent flag is set
// If we have a new day (local time), a new OnTimeDay is calculated
// If we have a new day the 'dayIsLocked' flag is cleared 
void OnOffDataContainerWio::SetNewOnOffValue(int sensorIndex, bool state, DateTime time, int offsetUtcMinutes)
{
    // change incoming state if inputInverter is active
    bool _state = onOffSampleValueSet.OnOffSampleValues[sensorIndex].inputInverter ? !state : state;
    DateTime _localTime = time.operator+(TimeSpan(offsetUtcMinutes * 60));
    DateTime _localTimeOfLastSwitch = onOffSampleValueSet.OnOffSampleValues[sensorIndex].LastSwitchTime.operator+(TimeSpan(offsetUtcMinutes * 60));
    TimeSpan _timeFromLast = time.operator-(onOffSampleValueSet.OnOffSampleValues[sensorIndex].LastSwitchTime);
    
    // only do anything if state has changed
    if (_state != onOffSampleValueSet.OnOffSampleValues[sensorIndex].actState)
    {  
        // if the day (local time) has changed we calculate a new OnTimeDay value
        if (_localTimeOfLastSwitch.day() != _localTime.day())
        {
            // if we switch from true (on) to off (false) we calculate new OnTimeDay
            if (onOffSampleValueSet.OnOffSampleValues[sensorIndex].actState == true)
            {
                onOffSampleValueSet.OnOffSampleValues[sensorIndex].OnTimeDay = time.operator-(DateTime(time.year(), time.month(), time.day(), 0, 0, 0));
            }
            else
            {
                onOffSampleValueSet.OnOffSampleValues[sensorIndex].OnTimeDay = TimeSpan(0);
            }
            onOffSampleValueSet.OnOffSampleValues[sensorIndex].dayIsLocked = false;
            onOffSampleValueSet.OnOffSampleValues[sensorIndex].resetToOnIsNeeded = false;
        }
        else    // not a new day
        {
            if (onOffSampleValueSet.OnOffSampleValues[sensorIndex].actState == true)
            {
                onOffSampleValueSet.OnOffSampleValues[sensorIndex].OnTimeDay = 
                onOffSampleValueSet.OnOffSampleValues[sensorIndex].OnTimeDay.operator+(time.operator-(onOffSampleValueSet.OnOffSampleValues[sensorIndex].LastSwitchTime));
            }          
        }
        
        onOffSampleValueSet.OnOffSampleValues[sensorIndex].lastState = 
        onOffSampleValueSet.OnOffSampleValues[sensorIndex].actState;               
        onOffSampleValueSet.OnOffSampleValues[sensorIndex].actState = _state;
        onOffSampleValueSet.OnOffSampleValues[sensorIndex].TimeFromLast = _timeFromLast.days() < 100 ? _timeFromLast : TimeSpan(0);
        onOffSampleValueSet.OnOffSampleValues[sensorIndex].LastSwitchTime = time;
        onOffSampleValueSet.OnOffSampleValues[sensorIndex].hasToBeSent = true;
    }
}

void OnOffDataContainerWio::PresetOnOffState(int sensorIndex, bool state, bool lastState, DateTime time)
{ 
    onOffSampleValueSet.OnOffSampleValues[sensorIndex].lastState = lastState;
    onOffSampleValueSet.OnOffSampleValues[sensorIndex].actState = state;
    if(time != nullptr)
    {
        onOffSampleValueSet.OnOffSampleValues[sensorIndex].LastSwitchTime = time;
    }
}

bool OnOffDataContainerWio::ReadOnOffState(int sensorIndex)
{
    return onOffSampleValueSet.OnOffSampleValues[sensorIndex].actState;
}

OnOffSampleValueSet OnOffDataContainerWio::GetOnOffValueSet()
{
    return onOffSampleValueSet;
}

void OnOffDataContainerWio::Reset_hasToBeSent(int sensorIndex)
{
    onOffSampleValueSet.OnOffSampleValues[sensorIndex].hasToBeSent = false;  
}

void OnOffDataContainerWio::Set_OutInverter(int sensorIndex, bool invertState)
{
    onOffSampleValueSet.OnOffSampleValues[sensorIndex].outInverter = invertState;  
}

void OnOffDataContainerWio::Set_InputInverter(int sensorIndex, bool invertState)
{
    onOffSampleValueSet.OnOffSampleValues[sensorIndex].inputInverter = invertState;
}

void OnOffDataContainerWio::Set_DayIsLockedFlag(int sensorIndex, bool isLockedState)
{
    onOffSampleValueSet.OnOffSampleValues[sensorIndex].dayIsLocked = isLockedState;
}

void OnOffDataContainerWio::Set_ResetToOnIsNeededFlag(int sensorIndex, bool state)
{
    onOffSampleValueSet.OnOffSampleValues[sensorIndex].resetToOnIsNeeded = state;
}

void OnOffDataContainerWio::Set_Year(int sensorIndex, int year)

{
    onOffSampleValueSet.OnOffSampleValues[sensorIndex].Year = year;
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

