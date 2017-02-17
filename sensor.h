#ifndef _ENV_SENSOR_H_
#define _ENV_SENSOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#define TIMEOUT 5000

// default wakup interval in seconds
#define DEFAULT_INTERVAL 10
#define MAX_INTERVAL 30*60

// protocol version check
#define PROTOCOL_VERSION_MIN "0.0"
// json keys
#define P_SIGNATURE "s"
#define P_VERSION "v"
#define P_KEY "k"
#define P_PAYLOAD "p"
#define P_INTERVAL "i"
#define P_THRESHOLD "th"

// error flags
#define E_SENSOR_FAILED 0b00000001
#define E_PROTOCOL_FAIL 0b00000010
#define E_SIG_VRFY_FAIL 0b00000100
#define E_JSON_FAILED   0b00001000
#define E_NO_MEMORY     0b10000000
#define E_NO_CONNECTION 0b01000000

/* Get source clock for LPTMR driver */
//#define LPTMR_SOURCE_CLOCK


// some prototypes necessary
//bool display_init(GPIO_Type *gpio, uint32_t pin);
//
//void display_print(const char *fmt, ...);
//
//void display_printf(const int x, const int y, const char *fmt, ...);
//
//void display_logo();
//
//void display_line(int x, int y, int x1, int y1);
//
//void display_circle(int x, int y, int r, bool on);
//
//void display_rect(int x, int y, int x1, int y1, bool color, bool fill);
//
//void display_clear();
//
//void display_render();
//
//void display_render_async();
//
//bool display_busy();

#ifdef __cplusplus
}
#endif

#endif // _ENV_SENSOR_H_
