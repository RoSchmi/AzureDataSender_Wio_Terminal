#include <Arduino.h>
#include <Datetime.h>

#ifndef _ON_OFF_SWITCHER_H_
#define _ON_OFF_SWITCHER_H_




class OnOffSwitcherWio
{
public:
    OnOffSwitcherWio();
    
    void begin(TimeSpan interval);

    void SetInactive();
    void SetActive();
    bool hasToggled(DateTime actUtcTime);
    bool GetState();

};


#endif  // _ON_OFF_DATACONTAINERWIO_H_