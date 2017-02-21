/*!
 * @file
 * @brief Ubirch Board Environmental Sensor.
 *
 *Reads the environmental sensor values, signs the payload and sends it
 * to the backend using MQTT protocol
 *
 * @author Niranjan Rao
 * @date 2017-02-15
 *
 * @copyright &copys; 2015, 2016, 2017 ubirch GmbH (https://ubirch.com)
 *
 * ```
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * ```
 */


#include <BME280.h>

#include "mbed-os-quectelM66-driver/M66Interface.h"
#include "mbed-os-quectelM66-driver/M66MQTT.h"

#include "MQTT/MQTTmbed.h"
#include "MQTT/MQTTClient.h"

#include "crypto/crypto.h"
#include "response.h"
#include "sensor.h"
#include "config.h"
#include "jsmn/jsmn.h"

#ifndef MAINDEBUG
#define PRINTF printf
#else
#define PRINTF(...)
#endif

#define MQTT_PAYLOAD_LENGTH 512
#define PRESSURE_SEA_LEVEL 101325
#define TEMPERATURE_THRESHOLD 4000

// default wakup interval in seconds
#define DEFAULT_INTERVAL 30
#define MAX_INTERVAL 30*60

static int temp_threshold = TEMPERATURE_THRESHOLD;
// internal sensor state
static unsigned int interval = DEFAULT_INTERVAL;

static bool mqttConnected = false;
static bool unsuccessfulSend =  false;

static char lat[32];
static char lon[32];

static int loop_counter = 0;

int arrivedcount = 0;
int level = 0;
int voltage = 0;
uint8_t error_flag = 0x00;

//actual payload template
static char *const message_template = "{\"id\":\"%s\",\"v\":\"0.0.2\",\"a\":\"%s\",\"k\":\"%s\",\"s\":\"%s\",\"p\":%s}";
static char *const payload_template = "{\"t\":%ld,\"p\":%ld,\"h\":%ld,\"a\":%ld,\"la\":\"%s\",\"lo\":\"%s\",\"ba\":%d,\"lp\":%d,\"e\":%d}";

static char *topicTemplate = "ubirch/devices/%s/in/raw";

// crypto key of the board
static uc_ed25519_key uc_key;

float temperature, pressure, humidity, altitude;

DigitalOut    led1(LED1);
BME280        bmeSensor(I2C_SDA, I2C_SCL);
M66Interface  network(GSM_UART_TX, GSM_UART_RX, GSM_PWRKEY, GSM_POWER, true);
MQTTNetwork   mqttNetwork(&network);
MQTT::Client  <MQTTNetwork, Countdown, MQTT_PAYLOAD_LENGTH> client = MQTT::Client<MQTTNetwork, Countdown, MQTT_PAYLOAD_LENGTH>(mqttNetwork);

char lastSentMessage[MQTT_PAYLOAD_LENGTH];

void dbg_dump(const char *prefix, const uint8_t *b, size_t size) {
    for (int i = 0; i < size; i += 16) {
        if (prefix && strlen(prefix) > 0) printf("%s %06x: ", prefix, i);
        for (int j = 0; j < 16; j++) {
            if ((i + j) < size) printf("%02x", b[i + j]); else printf("  ");
            if ((j+1) % 2 == 0) putchar(' ');
        }
        putchar(' ');
        for (int j = 0; j < 16 && (i + j) < size; j++) {
            putchar(b[i + j] >= 0x20 && b[i + j] <= 0x7E ? b[i + j] : '.');
        }
        printf("\r\n");
    }
}

// convert a number of characters into an unsigned integer value
static unsigned int to_uint(const char *ptr, size_t len) {
    unsigned int ret = 0;
    for (uint8_t i = 0; i < len; i++) {
        ret = (ret * 10) + (ptr[i] - '0');
    }
    return ret;
}

/*!
 * Process payload and set configuration parameters from it.
 * @param payload the payload to use, should be checked
 */
void process_payload(char *payload) {
    jsmntok_t *token;
    jsmn_parser parser;
    jsmn_init(&parser);

    // identify the number of tokens in our response, we expect 13
    const uint8_t token_count = (const uint8_t) jsmn_parse(&parser, payload, strlen(payload), NULL, 0);
    token = (jsmntok_t *) malloc(sizeof(*token) * token_count);

    // reset parser, parse and store tokens
    jsmn_init(&parser);
    if (jsmn_parse(&parser, payload, strlen(payload), token, token_count) == token_count &&
        token[0].type == JSMN_OBJECT) {
        uint8_t index = 0;
        PRINTF("we have the json object\r\n");
        while (++index < token_count) {
            if (jsoneq(payload, &token[index], P_INTERVAL) == 0 && token[index + 1].type == JSMN_PRIMITIVE) {
                index++;
                interval = to_uint(payload + token[index].start, (size_t) token[index].end - token[index].start);
                PRINTF("Interval: %ds\r\n", interval);
            } else if (jsoneq(payload, &token[index], P_THRESHOLD) == 0 && token[index + 1].type == JSMN_PRIMITIVE) {
                index++;
                temp_threshold = to_uint(payload + token[index].start, (size_t) token[index].end - token[index].start);
                PRINTF("Threshold: %d\r\n", temp_threshold);
            } else {
                print_token("unknown key:", payload, &token[index]);
                index++;
            }
        }
    } else {
        error_flag |= E_JSON_FAILED;
    }

    free(token);
}

void messageArrived(MQTT::MessageData& md) {
    MQTT::Message &message = md.message;
    PRINTF("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup,
           message.id);
    PRINTF("Payload %.*s\r\n", message.payloadlen, (char *) message.payload);
    ++arrivedcount;

    if (!strncmp(lastSentMessage, (const char *) message.payload, message.payloadlen)) {

        uc_ed25519_pub_pkcs8 response_key;
        unsigned char response_signature[SHA512_HASH_SIZE];
        memset(&response_key, 0xff, sizeof(uc_ed25519_pub_pkcs8));
        memset(response_signature, 0xf7, SHA512_HASH_SIZE);

        char *response_payload = process_response(lastSentMessage, &response_key, response_signature);

        dbg_dump("Received KEY    : ", (unsigned char *) &response_key, sizeof(uc_ed25519_pub_pkcs8));
        dbg_dump("Received SIG    : ", response_signature, sizeof(response_signature));
        PRINTF("Received PAYLOAD: %s\r\n", response_payload);

        uc_ed25519_key remote_pub;
        if (uc_import_ecc_pub_key_encoded(&remote_pub, &response_key)) {
            if (uc_ecc_verify(&remote_pub, (const unsigned char *) response_payload, strlen(response_payload),
                              response_signature, sizeof(response_signature))) {
                process_payload(response_payload);
                unsuccessfulSend = false;
            }
        }
        else {
            PRINTF("import public key failed\r\n");
        }
    }
}

int getDeviceUUID(char *deviceID) {
    uint32_t uuid[4];

    uuid[0] = SIM->UIDH;
    uuid[1] = SIM->UIDMH;
    uuid[2] = SIM->UIDML;
    uuid[3] = SIM->UIDL;

    sprintf(deviceID, "%08lX-%04lX-%04lX-%04lX-%04lX%08lX",
            uuid[0],                     // 8
            uuid[1] >> 16,               // 4
            uuid[1] & 0xFFFF,            // 4
            uuid[2] >> 16,               // 4
            uuid[2] & 0xFFFF, uuid[3]);  // 4+8

    return true;
}

int pubMqttPayload() {

    uc_init();
    uc_import_ecc_key(&uc_key, device_ecc_key, device_ecc_key_len);

    //++++++++++++++++++++++++++++++++++++++++++
    //++++++++++++++++++++++++++++++++++++++++
    // payload structure to be signed
    // Example: '{"t":22.0,"p":1019.5,"h":40.2,"lat":"12.475886","lon":"51.505264","bat":100,"lps":99999}'
    int payload_size = snprintf(NULL, 0, payload_template,
                                (int) (temperature * 100.0f), (int) pressure, (int) ((humidity) * 100.0f),
                                (int) (altitude * 100.0f),
                                lat, lon, level, loop_counter, error_flag);
    char payload[payload_size];
    sprintf(payload, payload_template,
            (int) (temperature * 100.0f), (int) (pressure), (int) ((humidity) * 100.0f), (int) (altitude * 100.0f),
            lat, lon, level, loop_counter, error_flag);

    error_flag = 0x00;

    const char *imei = network.get_imei();

    // be aware that you need to free these strings after use
    char *auth_hash = uc_sha512_encoded((const unsigned char *) imei, strnlen(imei, 15));
    char *pub_key_hash = uc_base64_encode(uc_key.p, 32);
    char *payload_hash = uc_ecc_sign_encoded(&uc_key, (const unsigned char *) payload, strlen(payload));

    PRINTF("PUBKEY   : %s\r\n", pub_key_hash);
    PRINTF("AUTH     : %s\r\n", auth_hash);
    PRINTF("SIGNATURE: %s\r\n", payload_hash);

    static char deviceUUID[128];
    static char topic[256];

    getDeviceUUID(deviceUUID);
    sprintf(topic, topicTemplate, deviceUUID);

    int message_size = snprintf(NULL, 0, message_template, deviceUUID, auth_hash, pub_key_hash, payload_hash, payload);
    char *message = (char *) malloc((size_t) (message_size + 1));
    sprintf(message, message_template, deviceUUID, auth_hash, pub_key_hash, payload_hash, payload);
    strncpy(lastSentMessage, message, message_size);

    // free hashes
    delete (auth_hash);
    delete (pub_key_hash);
    delete (payload_hash);

    PRINTF("--MESSAGE (%d)\r\n", strlen(message));
    PRINTF(message);
    PRINTF("\r\n--MESSAGE\r\n");

    MQTT::Message mqmessage;
    int rc;

    mqmessage.qos = MQTT::QOS0;
    mqmessage.retained = false;
    mqmessage.dup = false;
    mqmessage.payload = (void *) message;
    mqmessage.payloadlen = strlen(message) + 1;

    printf("\r\nthe pub topic: %s\r\n", topic);
    rc = client.publish(topic, mqmessage);
    if (rc != 0) {
        unsuccessfulSend = true;
        mqttConnected = false;

        printf("Failed to publish: %d\r\n", rc);
        return -1;
    }

    unsuccessfulSend = false;

    while (arrivedcount < 1)
        client.yield(100);

    return 0;
}


int mqttConnect() {

    int rc;

    uint8_t status = 0;
    bool gotLocation = false;

    rtc_datetime_t date_time;

    if (!mqttConnected) {

        if (network.connect(CELL_APN, CELL_USER, CELL_PWD) != 0)
            return false;

        network.getModemBattery(&status, &level, &voltage);
        printf("the battery status %d, level %d, voltage %d\r\n", status, level, voltage);

        PRINTF("Connecting to %s:%d\r\n", UMQTT_HOST, UMQTT_HOST_PORT);
        rc = mqttNetwork.connect(UMQTT_HOST, UMQTT_HOST_PORT);
        if (rc != 0) {
            PRINTF("rc from TCP connect is %d\r\n", rc);
            mqttConnected = false;
            return false;
        }

        MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
        data.MQTTVersion = 3;
        data.clientID.cstring = UMQTT_CLIENTID;
        data.username.cstring = UMQTT_USER;
        data.password.cstring = UMQTT_PWD;

        static char deviceUUID[128];
        static char topic[256];

        getDeviceUUID(deviceUUID);
        sprintf(topic, topicTemplate, deviceUUID);
        printf("\r\nTopic to publish in: \"%s\"\r\n", topic);

        if ((rc = client.connect(data)) == 0) {
            if ((rc = client.subscribe(topic, MQTT::QOS1, messageArrived)) == 0) {
                PRINTF("Connected and subscribed\r\n");
                mqttConnected = true;
            } else {
                PRINTF("rc from MQTT subscribe is %d\r\n", rc);
                mqttConnected = false;
                return false;
            }
        } else {
            PRINTF("rc from MQTT connect is %d\r\n", rc);
            mqttConnected = false;
            return false;
        }
    }

    for(int lc = 0; lc < 3 && !gotLocation; lc++) {
        gotLocation = network.get_location_date(lat, lon, &date_time);
                PRINTF("setting current time from GSM\r\n");
                PRINTF("%04hd-%02hd-%02hd %02hd:%02hd:%02hd\r\n",
                       date_time.year, date_time.month, date_time.day, date_time.hour, date_time.minute, date_time.second);
                PRINTF("lat is %s lon %s\r\n", lat, lon);
    }
    return true;
}

void led_thread(void const *args) {
    while (true) {
        led1 = !led1;
        Thread::wait(1000);
    }
}

void bme_thread(void const * args) {

    while (true) {
        temperature = bmeSensor.getTemperature();
        pressure = bmeSensor.getPressure();
        humidity = bmeSensor.getHumidity();
        altitude = 44330.0f * (1.0f - (float) pow(pressure / (float) PRESSURE_SEA_LEVEL, 1 / 5.255));

        Thread::wait(10000);
    }
}

osThreadDef(led_thread,   osPriorityNormal, DEFAULT_STACK_SIZE);
osThreadDef(bme_thread,   osPriorityNormal, DEFAULT_STACK_SIZE);

int main(int argc, char* argv[]) {

    osThreadCreate(osThread(led_thread), NULL);
    osThreadCreate(osThread(bme_thread), NULL);

    mqttConnect();

    while (1) {

        if (temperature > temp_threshold || ((MAX_INTERVAL / interval) % loop_counter == 0) || unsuccessfulSend) {

            if (!mqttConnected)
                mqttConnect();

            if (pubMqttPayload() != -1)
                client.yield();
        }
        Thread::wait(10000);
        loop_counter++;
        printf("\r\nLoop counter: %d\r\n", loop_counter);
    }
}