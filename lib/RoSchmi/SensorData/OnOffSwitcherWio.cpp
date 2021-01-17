#include <Arduino.h>
#include "OnOffSwitcherWio.h"
#include "DateTime.h"


DateTime  lastSwitchTime = DateTime();
TimeSpan  switchInterval = TimeSpan(60);
//bool state = false;
//bool isActive = false;

OnOffSwitcherWio::OnOffSwitcherWio()
{}

void OnOffSwitcherWio::begin(TimeSpan interval)
{
    switchInterval = interval;
}

bool OnOffSwitcherWio::hasToggled(DateTime actUtcTime)
{
    if (isActive)
    {
        if (actUtcTime.operator>=(lastSwitchTime.operator+(switchInterval)))
        {
            lastSwitchTime = actUtcTime;
            state = !state;
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool OnOffSwitcherWio::GetState()
{
    return state;
}

void OnOffSwitcherWio::SetInactive()
{
    isActive = false;
}

void OnOffSwitcherWio::SetActive()
{
    isActive = true;
}