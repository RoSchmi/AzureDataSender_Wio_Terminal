#include <AzureStorage/AnalogTableEntity.h>
#include <AzureStorage/TableEntityProperty.h>
#include <AzureStorage/TableEntity.h>

        
        //EntityProperty * Properties;

// Your entity type must expose a parameter-less constructor
        AnalogTableEntity::AnalogTableEntity() { }


        // Define the PK and RK
        AnalogTableEntity::AnalogTableEntity(az_span partitionKey, az_span rowKey, az_span sampleTime, EntityProperty *pProperties, size_t propertyCount)
            : TableEntity(partitionKey, rowKey, sampleTime)
        {
            Properties = pProperties;    // store the ArrayList

            PropertyCount = propertyCount;
        
            PropertyClass myProperties;           
            myProperties.PartitionKey = partitionKey;           
            myProperties.RowKey = rowKey;
              
            myProperties.SampleTime = SampleTime;
            myProperties.T_1 = az_span_create_from_str(pProperties[1].Value);
            myProperties.T_2 = az_span_create_from_str(pProperties[2].Value);
            myProperties.T_3 = az_span_create_from_str(pProperties[3].Value);
            myProperties.T_4 = az_span_create_from_str(pProperties[4].Value);
            
           // {
                
            //    PartitionKey = partitionKey,
            //    RowKey = rowKey,
                /*
                SampleTime = (EntityProperty)Properties[0].
                SampleTime = ((string[]) this.Properties[0])[2],
                T_1 = ((string[])this.Properties[1])[2],
                T_2 = ((string[])this.Properties[2])[2],
                T_3 = ((string[])this.Properties[3])[2],
                T_4 = ((string[])this.Properties[4])[2],
                */
           // };
            

        //  this.JsonString = JsonConverter.Serialize(myProperties).ToString();
        }

        AnalogTableEntity::PropertyClass::PropertyClass(){};
        
       // AnalogTableEntity::~AnalogTableEntity()
       // {};
        
        /*
        class PropertyClass
        {
            
           az_span RowKey;
           az_span PartitionKey;
           az_span SampleTime;
           az_span T_1;
           az_span T_2;
           az_span T_3;
           az_span T_4;

           PropertyClass();
           
           PropertyClass(az_span RowKey, az_span PartitionKey)
           {

           }
           
        };
        */

        