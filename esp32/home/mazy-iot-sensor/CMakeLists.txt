cmake_minimum_required(VERSION 3.5)

set(PARTITION_TABLE_CSV_PATH "partitions/partitions_esp32.csv")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mazy-iot-sensor)
