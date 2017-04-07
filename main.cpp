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

#include "mbed-kinetis-lowpower/kinetis_lowpower.h"
#include "mbed-os-quectelM66-driver/M66Interface.h"

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

static int temp_threshold = TEMPERATURE_THRESHOLD;
// internal sensor state
static unsigned int interval = DEFAULT_INTERVAL;

static bool mqttConnected = false;
static bool unsuccessfulSend = false;

static char lat[32], lon[32];
static char deviceUUID[37];

static int loop_counter = 0;

int arrivedcount = 0;
int level = 0;
int voltage = 0;
uint8_t error_flag = 0x00;

//actual payload template
static const char *const payload_template = "{\"t\":%d,\"p\":%d,\"h\":%d,\"a\":%d,\"la\":\"%s\",\"lo\":\"%s\",\"ba\":%d,\"lp\":%d,\"e\":%d}";

static const char *const message_template = "POST /api/avatarService/v1/device/update HTTP/1.1\r\n"
"Host: api.demo.dev.ubirch.com:8080\r\n"
"\r\n"
"{\"v\":\"0.0.2\",\"a\":\"%s\",\"k\":\"%s\",\"s\":\"%s\",\"p\":%s}"
"\r\n";

static const char *topicTemplate = "mwc/ubirch/devices/%s/%s";

// crypto key of the board
static uc_ed25519_key uc_key;

float temperature, pressure, humidity, altitude;

BME280 bmeSensor(I2C_SDA, I2C_SCL);
M66Interface network(GSM_UART_TX, GSM_UART_RX, GSM_PWRKEY, GSM_POWER, true);

void dbg_dump(const char *prefix, const uint8_t *b, size_t size) {
    for (int i = 0; i < size; i += 16) {
        if (prefix && strlen(prefix) > 0) printf("%s %06x: ", prefix, i);
        for (int j = 0; j < 16; j++) {
            if ((i + j) < size) printf("%02x", b[i + j]); else printf("  ");
            if ((j + 1) % 2 == 0) putchar(' ');
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

void messageArrived(char *buf) {
    uc_ed25519_pub_pkcs8 response_key;
    unsigned char response_signature[SHA512_HASH_SIZE];
    memset(&response_key, 0xff, sizeof(uc_ed25519_pub_pkcs8));
    memset(response_signature, 0xf7, SHA512_HASH_SIZE);


    char *response_payload = process_response(buf, &response_key, response_signature);

    dbg_dump("Received KEY    : ", (unsigned char *) &response_key, sizeof(uc_ed25519_pub_pkcs8));
    dbg_dump("Received SIG    : ", response_signature, sizeof(response_signature));
    PRINTF("Received PAYLOAD: %s\r\n", response_payload);

    uc_ed25519_key remote_pub;
    if (uc_import_ecc_pub_key_encoded(&remote_pub, &response_key)) {
        if (uc_ecc_verify(&remote_pub, (const unsigned char *) response_payload, strlen(response_payload),
                          response_signature, sizeof(response_signature))) {
            process_payload(response_payload);
            unsuccessfulSend = false;
        } else {
            PRINTF("payload verification failed\r\n");
        }
    } else {
        PRINTF("import public key failed\r\n");
    }

    free(response_payload);
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

int pubMqttPayload(char *topic) {

    TCPSocket socket;

    int rc;
    int ret;

    uint8_t status = 0;
    bool gotLocation = false;

    rtc_datetime_t date_time;


    if (network.connect(CELL_APN, CELL_USER, CELL_PWD) != 0)
        return false;

    socket.open(&network);
    socket.set_timeout(0);

    network.getModemBattery(&status, &level, &voltage);
    printf("the battery status %d, level %d, voltage %d\r\n", status, level, voltage);

    PRINTF("Connecting to %s:%d\r\n", UMQTT_HOST, UMQTT_HOST_PORT);
    char theIP[20];
    bool ipret = network.queryIP("api.demo.dev.ubirch.com", theIP);

    ret = socket.connect(theIP, 8080);

    for (int lc = 0; lc < 3 && !gotLocation; lc++) {
        gotLocation = network.get_location_date(lat, lon, &date_time);
        PRINTF("setting current time from GSM\r\n");
        PRINTF("%04hd-%02hd-%02hd %02hd:%02hd:%02hd\r\n",
               date_time.year, date_time.month, date_time.day, date_time.hour, date_time.minute, date_time.second);
        PRINTF("lat is %s lon %s\r\n", lat, lon);
    }

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
    char *payload = (char *)malloc((size_t) payload_size);
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

    int message_size = snprintf(NULL, 0, message_template, auth_hash, pub_key_hash, payload_hash, payload);
    char *message = (char *) malloc((size_t) (message_size + 1));
    sprintf(message, message_template, auth_hash, pub_key_hash, payload_hash, payload);
    PRINTF("message_size%d\r\n", message_size);

    // free hashes
    delete (auth_hash);
    delete (pub_key_hash);
    delete (payload_hash);

    PRINTF("--MESSAGE (%d)\r\n", strlen(message));
    PRINTF(message);
    PRINTF("\r\n--MESSAGE\r\n");

//    if (ret >= 0) {

        int r = socket.send(message, strlen(message));
        if (r > 0) {
            // Recieve a simple http response and print out the response line
            int buffer_size = message_size;
            char *buffer = (char *) malloc((size_t) (message_size + 1));

            printf("now receive the data\r\n");
            r = socket.recv(buffer, 512);
            if (r >= 0) {
                printf("received %d bytes\r\n---\r\n%.*s\r\n---\r\n", r,
                       (int) (strstr(buffer, "\r\n") - buffer),
                       buffer);

            } else {
                printf("receive failed: %d\r\n", r);
            }
        } else {
            printf("send failed: %d\r\n", r);
        }

        free(message);
//    }
    // Close the socket to return its memory and bring down the network interface
    socket.close();

    network.powerDown();
    return 0;
}

//void led_thread(void const *args) {
//    while (true) {
//        led1 = !led1;
//        Thread::wait(1000);
//    }
//}

void bme_thread(void const *args) {

    while (true) {
        temperature = bmeSensor.getTemperature();
        pressure = bmeSensor.getPressure();
        humidity = bmeSensor.getHumidity();
        altitude = 44330.0f * (1.0f - (float) pow(pressure / (float) PRESSURE_SEA_LEVEL, 1 / 5.255));

        Thread::wait(10000);
    }
}

//osThreadDef(led_thread, osPriorityNormal, DEFAULT_STACK_SIZE);
osThreadDef(bme_thread, osPriorityNormal, DEFAULT_STACK_SIZE);

int main(int argc, char *argv[]) {
//    osThreadCreate(osThread(led_thread), NULL);
    osThreadCreate(osThread(bme_thread), NULL);

    getDeviceUUID(deviceUUID);
    int len = snprintf(NULL, 0, topicTemplate, deviceUUID, "out");
    char *topic_receive = (char *)malloc((size_t) len);
    sprintf(topic_receive, topicTemplate, deviceUUID, "out");
    printf("RECEIVE: \"%s\"\r\n", topic_receive);

    len = snprintf(NULL, 0, topicTemplate, deviceUUID, "");
    char *topic_send = (char *)malloc((size_t) len);
    sprintf(topic_send, topicTemplate, deviceUUID, "");
    printf("SEND: \"%s\"\r\n", topic_send);

    while (1) {
//        printf("send? %d %% ((%d / %d) == %d\r\n", loop_counter, MAX_INTERVAL, interval,
//               loop_counter % (MAX_INTERVAL / interval));
//        printf("temp (%d) > threshold (%d)?\r\n", ((int) (temperature * 100)), temp_threshold);
//        printf("unsuccessful? == %d\r\n", unsuccessfulSend);
        if (((int) (temperature * 100)) > temp_threshold || (loop_counter % (MAX_INTERVAL / interval) == 0) || unsuccessfulSend) {
            pubMqttPayload(topic_send);
        }

        loop_counter++;
        printf(".");
        powerDownWakeupOnRtc(20);

    }
}