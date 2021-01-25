#include <AzureStorage/TableEntityProperty.h>
#include <azure/core/az_span.h>
#include <DateTime.h>

#ifndef _TABLE_ENTITY_H_
#define _TABLE_ENTITY_H_

//az_span PartitionKey; 
//az_span   RowKey;
//az_span   ETag;
//DateTime TimeStamp;
//static bool DisablePropertyResolverCache;

//az_span JsonString = az_span_create_from_str((char *)"");

//EntityProperty * Properties;

class TableEntity
{

    public:

        EntityProperty * Properties;
        size_t PropertyCount;
        az_span     SampleTime;
        az_span     PartitionKey; 
        az_span     RowKey;
        TableEntity();
        TableEntity(az_span partitionKey, az_span rowKey, az_span sampleTime);
        //~TableEntity();

};

#endif
