#if 1

#include "mbed.h"
#include "http_request.h"
#include "../mbed-kinetis-lowpower/kinetis_lowpower.h"
#include "../mbed-os-quectelM66-driver/M66Interface.h"
#include "../config.h"
#include "BME280.h"


#include "../crypto/crypto.h"
#include "response.h"
#include "sensor.h"
#include "../config.h"
#include "../jsmn/jsmn.h"

#ifndef MAINDEBUG
#define PRINTF printf
#else
#define PRINTF(...)
#endif

#define PRESSURE_SEA_LEVEL 101325
#define TEMPERATURE_THRESHOLD 4000


bool unsuccessfulSend = false;
static int temp_threshold = TEMPERATURE_THRESHOLD;
// internal sensor state
static unsigned int interval = DEFAULT_INTERVAL;
static int loop_counter = 0;

uint8_t error_flag = 0x00;

BME280 bmeSensor(I2C_SDA, I2C_SCL);
M66Interface modem(GSM_UART_TX, GSM_UART_RX, GSM_PWRKEY, GSM_POWER, true);
DigitalOut led(LED1);

static float temperature, pressure, humidity, altitude;


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

void dump_response(HttpResponse* res) {
    printf("Status: %d - %s\n", res->get_status_code(), res->get_status_message().c_str());

    printf("Headers:\n");
    for (size_t ix = 0; ix < res->get_headers_length(); ix++) {
        printf("\t%s: %s\n", res->get_headers_fields()[ix]->c_str(), res->get_headers_values()[ix]->c_str());
    }
    printf("\nBody (%d bytes):\n\n%s\n", res->get_body_length(), res->get_body_as_string().c_str());
}

int HTTPSession() {

    // Create a TCP socket
    printf("\n----- Setting up TCP connection -----\r\n");

    char theIP[20];
    bool ipret = modem.queryIP("api.demo.dev.ubirch.com", theIP);

    TCPSocket *socket = new TCPSocket();
    nsapi_error_t open_result = socket->open(&modem);

    if (open_result != 0) {
        printf("Opening TCPSocket failed... %d\n", open_result);
        return 1;
    }

    nsapi_error_t connect_result = socket->connect(theIP, 8080);
    if (connect_result != 0) {
        printf("Connecting over TCPSocket failed... %d\n", connect_result);
        return 1;
    }
    int rc;
    int ret;

    int level = 0;
    int voltage = 0;

    static char lat[32], lon[32];

    uint8_t status = 0;
    bool gotLocation = false;

    // crypto key of the board
    static uc_ed25519_key uc_key;

    rtc_datetime_t date_time;

    //actual payload template
    static const char *const payload_template = "{\"t\":%d,\"p\":%d,\"h\":%d,\"a\":%d,\"la\":\"%s\",\"lo\":\"%s\",\"ba\":%d,\"lp\":%d,\"e\":%d}";

    static const char *const message_template = "{\"v\":\"0.0.2\",\"a\":\"%s\",\"k\":\"%s\",\"s\":\"%s\",\"p\":%s}";

    modem.getModemBattery(&status, &level, &voltage);
    printf("the battery status %d, level %d, voltage %d\r\n", status, level, voltage);

    for (int lc = 0; lc < 3 && !gotLocation; lc++) {
        gotLocation = modem.get_location_date(lat, lon, &date_time);
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
    char *payload = (char *) malloc((size_t) payload_size);
    sprintf(payload, payload_template,
            (int) (temperature * 100.0f), (int) (pressure), (int) ((humidity) * 100.0f), (int) (altitude * 100.0f),
            lat, lon, level, loop_counter, error_flag);

    error_flag = 0x00;

    const char *imei = modem.get_imei();

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

    printf("Connected over TCP to httpbin.org:80\n");

    // POST request to httpbin.org
    {
        HttpRequest *post_req = new HttpRequest(socket, HTTP_POST,
                                                "http://api.demo.dev.ubirch.com/api/avatarService/v1/device/update");
        post_req->set_header("Content-Type", "application/json");

        HttpResponse *post_res = post_req->send(message, strlen(message));
        if (!post_res) {
            printf("HttpRequest failed (error code %d)\n", post_req->get_error());
            return 1;
        }

        printf("\n----- HTTP POST response -----\n");
        dump_response(post_res);

        free(message);

        delete post_req;
    }
    delete socket;

    modem.powerDown();
    powerDownWakeupOnRtc(30);

    return 0;
}

void bme_thread(void const *args) {

    while (true) {
        temperature = bmeSensor.getTemperature();
        pressure = bmeSensor.getPressure();
        humidity = bmeSensor.getHumidity();
        altitude = 44330.0f * (1.0f - (float) pow(pressure / (float) PRESSURE_SEA_LEVEL, 1 / 5.255));

        Thread::wait(10000);
    }
}
osThreadDef(bme_thread, osPriorityNormal, DEFAULT_STACK_SIZE);

int main() {

    printf("Env-sensor Test\r\n");
    osThreadCreate(osThread(bme_thread), NULL);

    while (1) {
        const int r = modem.connect(CELL_APN, CELL_USER, CELL_PWD);
        if (r != 0) {
            printf("Cannot connect to the network, see serial output");
        } else {
            HTTPSession();
        }
    }
}

#endif
