cmake_minimum_required(VERSION 3.5)
set(CMAKE_BUILD_TYPE Debug)

project(mbedEnvSensor C CXX)

# == MBED OS 5 settings ==
set(FEATURES netsocket)

add_definitions(
        -DTOOLCHAIN_GCC
        -DCPU_MK82FN256VLL15
        -DTARGET_K82F
        -DTARGET_UBIRCH1
        -DMBED_CONF_RTOS_PRESENT
        -DMBED_CONF_NSAPI_PRESENT
        -DMBED_CONF_EVENTS_PRESENT
        -D__MBED__
        -DDEVICE_I2C
        -DDEVICE_SERIAL
)

set(MBED_OS
        mbed-os
        mbed-os/cmsis
        mbed-os/drivers
        mbed-os/events
        mbed-os/events/enqueue
        mbed-os/hal
        mbed-os/hal/storage_abstraction
        mbed-os/platform
        mbed-os/rtos
        mbed-os/features
        mbed-os/rtos/rtx/TARGET_CORTEX_M
        mbed-os/rtos/rtx/TARGET_CORTEX_M/TARGET_RTOS_M4_M7
        mbed-os/targets/TARGET_Freescale
        mbed-os/targets/TARGET_Freescale/TARGET_KSDK2_MCUS/api
        mbed-os/targets/TARGET_Freescale/TARGET_KSDK2_MCUS/TARGET_K82F
        mbed-os/targets/TARGET_Freescale/TARGET_KSDK2_MCUS/TARGET_K82F/device
        mbed-os/targets/TARGET_Freescale/TARGET_KSDK2_MCUS/TARGET_K82F/drivers
        mbed-os/targets/TARGET_Freescale/TARGET_KSDK2_MCUS/TARGET_K82F/device/TOOLCHAIN_GCC_ARM
        mbed-os/targets/TARGET_Freescale/TARGET_KSDK2_MCUS/TARGET_K82F/TARGET_UBIRCH1
        )

set(MBED_OS_SRCS "")
foreach (DIR in ${MBED_OS})
    if (DIR MATCHES "mbed-os/features")
        foreach(FEATURE in ${FEATURES})
            file(GLOB_RECURSE includes
                    ${DIR}/${FEATURE}/*.cpp ${DIR}/${FEATURE}/*.c ${DIR}/${FEATURE}/*.s ${DIR}/${FEATURE}/*.S)
            list(APPEND MBED_OS_SRCS ${includes})
        endforeach()
    else ()
        file(GLOB includes ${DIR}/*.cpp ${DIR}/*.c ${DIR}/*.s ${DIR}/*.S)
        list(APPEND MBED_OS_SRCS ${includes})
    endif ()
endforeach ()

add_library(mbed-os ${MBED_OS_SRCS})
include_directories(${MBED_OS})
# == END MBED OS 5 ==

add_library(BME280 BME280/BME280.cpp)
target_include_directories(BME280 PUBLIC BME280)

add_library(mbed-os-quectelM66-driver
        mbed-os-quectelM66-driver/M66Interface.cpp
        mbed-os-quectelM66-driver/M66ATParser/M66ATParser.cpp
        mbed-os-quectelM66-driver/M66ATParser/BufferedSerial/BufferedPrint.c
        mbed-os-quectelM66-driver/M66ATParser/BufferedSerial/BufferedSerial.cpp
        mbed-os-quectelM66-driver/M66ATParser/BufferedSerial/Buffer/MyBuffer.cpp
        )
target_include_directories(mbed-os-quectelM66-driver PUBLIC
        mbed-os-quectelM66-driver
        mbed-os-quectelM66-driver/M66ATParser
        mbed-os-quectelM66-driver/M66ATParser/BufferedSerial
        mbed-os-quectelM66-driver/M66ATParser/BufferedSerial/Buffer
        )

add_library(MQTT
        MQTT/MQTTPacket/MQTTConnectClient.c
        MQTT/MQTTPacket/MQTTConnectServer.c
        MQTT/MQTTPacket/MQTTPacket.c
        MQTT/MQTTPacket/MQTTSerializePublish.c
        MQTT/MQTTPacket/MQTTSubscribeClient.c
        MQTT/MQTTPacket/MQTTSubscribeServer.c
        MQTT/MQTTPacket/MQTTUnsubscribeClient.c
        MQTT/MQTTPacket/MQTTUnsubscribeServer.c
        )
target_include_directories(MQTT PUBLIC
        MQTT
        MQTT/MQTTPacket
        )

add_library(CRYPTO
        crypto/crypto.c
        )
target_include_directories(CRYPTO PUBLIC
        crypto
        )
add_library(JSMN
        jsmn/jsmn.c
        )
target_include_directories(JSMN PUBLIC
        jsmn
        )

add_executable(mbed-os-porting
        main.cpp
        )
target_link_libraries(mbed-os-porting mbed-os BME280)

add_custom_target(mbed-os-porting-compile ALL
        COMMAND mbed compile --profile mbed-os/tools/profiles/debug.json
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

