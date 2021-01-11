#include <Arduino.h>
#include <Datetime.h>

#ifndef _ON_OFF_DATACONTAINERWIO_H_
#define _ON_OFF_DATACONTAINERWIO_H_

#define PROPERTY_COUNT 4

typedef struct
{
    bool inverter = false;
    bool actState = false; 
    bool lastState = true;
    bool  hasToBeSent = false;
    DateTime LastSwitchTime;
    char tableName[50];
    uint16_t Year = 1900;
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


/**
 * @brief Sets the 'state' variable if the new value is different to the value before.
 *        If the 'state' is changed, 'lastState' is set to the former value of 'state'
 *        The 'hasToBeSent' flag is set
 *
 * @param[in] sensorIndex The index of 4 OnOff-Tables (0 - 3)
 * @param[in] state Sets the 'state'-variable of the selected OnOff-Sensor representation
 * @param[in] time Sets the 'LastSwitchTime'-variable of the selected OnOff-Sensor representation.
 *              If time = nullptr or time is not passed, 'LastSwitchTime' is not changed
 */
    void SetNewOnOffValue(int sensorIndex, bool state, DateTime time);

/**
 * @brief Sets State and LastState without affecting 'hasToBeSent"-State.
 *
 * @param[in] sensorIndex The index of 4 OnOff-Tables (0 - 3)
 * @param[in] state Sets the 'state'-variable of the selected OnOff-Sensor representation
 * @param[in] lastState Sets the 'lastState'-variable of the selected OnOff-Sensor representation
 * @param[in] time Sets the 'LastSwitchTime'-variable of the selected OnOff-Sensor representation.
 *              If time = nullptr or time is not passed, 'LastSwitchTime' is not changed
 * 
 */
    void PresetOnOffState(int sensorIndex, bool state, bool lastState, DateTime time = nullptr);

/**
 * @brief Resets the 'hasToBeSent"-flag of the selected sensor representation.
 *
 * @param[in] sensorIndex The index of 4 OnOff-Tables (0 - 3)
 */    
    void Reset_hasToBeSent(int sensorIndex);

/**
 * @brief Sets a flag which indicates whether the state has to be inverted for use.
 *
 * @param[in] sensorIndex The index of 4 OnOff-Tables (0 - 3)
 * 
 */  
    void Set_Inverter(int sensorIndex, bool invertState);

/**
 * @brief Sets the Year (of the last upload)
 *
 * @param[in] sensorIndex The index of 4 OnOff-Tables (0 - 3)
 * @param[in] year The year 
 * 
 */  
    void Set_Year(int sensorIndex, int year);


/**
 * @brief Returns true if at least in one of the sensor representations the 'hasToBeSent'-flag is set
 *
 * @param[in] sensorIndex The index of 4 OnOff-Tables (0 - 3)
 * @return Indicating if at least for one Sensor representation the 'hasToBeSent'-flag is set.
 */  
    bool One_hasToBeBeSent();
     
    OnOffSampleValueSet GetOnOffValueSet();   
};

#endif  // _ON_OFF_DATACONTAINERWIO_H_