#!/bin/bash

if [ -z "$1" ]
then
    curl -X GET -H "Authorization: Bearer $SPARK_TOKEN" https://api.spark.io/v1/devices/$SPARK_DEVICE_ID/
else
    curl -X POST -H "Authorization: Bearer $SPARK_TOKEN" -d "args=$2" https://api.spark.io/v1/devices/$SPARK_DEVICE_ID/$1
fi

