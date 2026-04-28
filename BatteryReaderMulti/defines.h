#ifndef DEFINES_H
#define DEFINES_H

// DS OneWire device family IDs
#define DS2433_ID 0xA3
#define DS2438_ID 0xA6

// LED states
#define LED_OFF         0
#define LED_BLINK_SHORT 1
#define LED_BLINK_LONG  2
#define LED_ON          3

// LED blink intervals (ms)
#define LED_BLINK_SHORT_INTERVAL 100
#define LED_BLINK_LONG_INTERVAL  800
#define LED_TASK_CYCLE           50

// Channel config
#define CHANNELS_ALL    0xFF

// Timing
#define WIFI_TIMEOUT     15000
#define SEND_INTERVAL    1800000  // 30 min
#define SCAN_INTERVAL    1000     // 1 sec
#define READ_INTERVAL    10000     // 5 sec

// OneWire delays (ms)
#define DS_SEAT_DELAY    250
#define DS_READ_DELAY    10
#define DS_RETRY_DELAY   500
#define DS_MAX_RETRIES   2

// FreeRTOS
#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#define CHARGER_LED_UNKNOWN 0xFF

// Structs
struct ChannelPins { byte owPin; byte ledPin; };

typedef struct {
  OneWire *ow;
  byte ledPin;
  volatile byte ledState;
  unsigned long lastToggle;
  bool ledLevel;
  uint8_t ds2433Addr[8];
  uint8_t ds2438Addr[8];
  bool batteryPresent;
  unsigned long nextSend;
  unsigned long lastRead;
  byte chargerLedState;
  String dataBuffer;
} owChannel;

#endif
