/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *    Ian Craggs - make sure QoS2 processing works, and add device headers
 *******************************************************************************/

/**
 This is a sample program to illustrate the use of the MQTT Client library
 on the mbed platform.  The Client class requires two classes which mediate
 access to system interfaces for networking and timing.  As long as these two
 classes provide the required public programming interfaces, it does not matter
 what facilities they use underneath. In this program, they use the mbed
 system libraries.

*/

#include <BME280.h>

#include "mbed-os-quectelM66-driver/M66Interface.h"
#include "mbed-os-quectelM66-driver/M66MQTT.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "config.h"
#include "crypto/crypto.h"

#define logMessage printf
#define PRINTF printf

bool mqtt_connected = false;
int arrivedcount = 0;

static char *const message_template = "{\"v\":\"0.0.2\",\"a\":\"%s\",\"k\":\"%s\",\"s\":\"%s\",\"p\":%s}";
static char *const payload_template = "{\"t\":%ld,\"p\":%ld,\"h\":%ld,\"a\":%ld}";

static uc_ed25519_key uc_key;

DigitalOut led1(LED1);
BME280       bmeSensor(I2C_SDA, I2C_SCL);

M66Interface network(GSM_UART_TX, GSM_UART_RX, GSM_PWRKEY, GSM_POWER, true);
MQTTNetwork mqttNetwork(&network);
MQTT::Client <MQTTNetwork, Countdown> client = MQTT::Client<MQTTNetwork, Countdown>(mqttNetwork);


void messageArrived(MQTT::MessageData& md)
{
    MQTT::Message &message = md.message;
    logMessage("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    logMessage("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;
}

void led_thread(void const *args) {
    while (true) {
        led1 = !led1;
        Thread::wait(1000);
    }
}
osThreadDef(led_thread,   osPriorityNormal, DEFAULT_STACK_SIZE);


int get_mqtt_payload(char *buf) {

    float temperature, pressure, humidity, altitude;

    temperature = bmeSensor.getTemperature();
    pressure = bmeSensor.getPressure();
    humidity = bmeSensor.getHumidity();
    altitude = temperature;

//    sprintf(buf, "EnvSensor(%d):: [T -> %2.2fC] [P -> %2.2fhPa] [H -> %2.2f%%]\r\n", msgCount,
//            temperature,
//            pressure,
//            humidity);

    uc_init();
    uc_import_ecc_key(&uc_key, device_ecc_key, device_ecc_key_len);


    //++++++++++++++++++++++++++++++++++++++++++
    //++++++++++++++++++++++++++++++++++++++++
    // payload structure to be signed
    // Example: '{"t":22.0,"p":1019.5,"h":40.2,"lat":"12.475886","lon":"51.505264","bat":100,"lps":99999}'
    int payload_size = snprintf(NULL, 0, payload_template,
                                (int) temperature, (int) pressure, (int) ((humidity / 1024.0f) * 100.0f),
                                (int) (altitude * 100.0f));
    char payload[payload_size];
    sprintf(payload, payload_template,
            (int) temperature, (int) pressure,
            (int) ((humidity / 1024.0f) * 100.0f),
            (int) (altitude * 100.0f));

    char imei[16] = {"qwertyuiopasdfg"};


    // be aware that you need to free these strings after use
    char *auth_hash = uc_sha512_encoded((const unsigned char *) imei, strnlen(imei, 15));
    char *pub_key_hash = uc_base64_encode(uc_key.p, 32);
    char *payload_hash = uc_ecc_sign_encoded(&uc_key, (const unsigned char *) payload, strlen(payload));

    PRINTF("PUBKEY   : %s\r\n", pub_key_hash);
    PRINTF("AUTH     : %s\r\n", auth_hash);
    PRINTF("SIGNATURE: %s\r\n", payload_hash);

    int message_size = snprintf(NULL, 0, message_template, auth_hash, pub_key_hash, payload_hash, payload);
//    char *message = (char *)malloc((size_t) (message_size + 1));
    buf = (char *)malloc((size_t) (message_size + 1));
    sprintf(buf, message_template, auth_hash, pub_key_hash, payload_hash, payload);

    // free hashes
    delete(auth_hash);
    delete(pub_key_hash);
    delete(payload_hash);

    PRINTF("--MESSAGE (%d)\r\n", strlen(buf));
    PRINTF(buf);
    PRINTF("\r\n--MESSAGE\r\n");
}


int mqtt_send_data() {

    int rc;
    float version = 0.6;
    char *topic = "EnvSensor";

    MQTT::Message message;
    uint32_t msgCount = 0;

    logMessage("HelloMQTT: version is %.2f\r\n", version);

    if (!mqtt_connected) {

        network.connect(CELL_APN, CELL_USER, CELL_PWD);

        const char *hostname = UMQTT_HOST;
        int port = UMQTT_HOST_PORT;

        logMessage("Connecting to %s:%d\r\n", hostname, port);
        rc = mqttNetwork.connect(hostname, port);
        if (rc != 0) {
            logMessage("rc from TCP connect is %d\r\n", rc);
            mqtt_connected = false;
        }

        MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
        data.MQTTVersion = 3;
        data.clientID.cstring = UMQTT_CLIENTID;
        data.username.cstring = UMQTT_USER;
        data.password.cstring = UMQTT_PWD;


        if ((rc = client.connect(data)) == 0) {
            if ((rc = client.subscribe(topic, MQTT::QOS0, messageArrived)) == 0) {
                logMessage("Connected and subscribed\r\n");
                mqtt_connected = true;
            } else {
                logMessage("rc from MQTT subscribe is %d\r\n", rc);
                mqtt_connected = false;
            }
        } else {
            logMessage("rc from MQTT connect is %d\r\n", rc);
            mqtt_connected = false;
        }
    }

    while (true) {
        char buf[300];

        get_mqtt_payload(buf);

        message.qos = MQTT::QOS0;
        message.retained = false;
        message.dup = false;
        message.payload = (void *) buf;
        message.payloadlen = strlen(buf) + 1;

        rc = client.publish(topic, message);
        if (rc != 0) {
            mqtt_connected = false;
            printf("pub fail value %d\r\n", rc);
            break;
        }
        printf("pub value %d\r\n", rc);

        while (arrivedcount < 1)
            client.yield(100);

        msgCount++;
        wait(10);
    }
}

int main(int argc, char* argv[]) {
    osThreadCreate(osThread(led_thread), NULL);

    while(1) {
        mqtt_send_data();
        wait(10);
    }
}
