#include <OneWire.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include "defines.h"
#include "settings.h"

owChannel channels[CHANNELS_COUNT];

void appendByte(String &str, uint8_t b);
void taskChannel(void *pvParameters);
void taskLED(void *pvParameters);
void sendData(byte ch);
void scanDevices(byte ch);
bool readDS2438(byte ch, String &output);
bool readDS2433(byte ch, String &output, uint8_t *rawData);
void readChannelData(byte ch);


void initChannels() {
  for (byte i = 0; i < CHANNELS_COUNT; i++) {
    channels[i].ow = new OneWire(CHANNEL_PINS[i].owPin);
    channels[i].ledPin = CHANNEL_PINS[i].ledPin;
    channels[i].ledState = LED_OFF;
    channels[i].lastToggle = 0;
    channels[i].ledLevel = false;
    memset(channels[i].ds2433Addr, 0, sizeof(channels[i].ds2433Addr));
    memset(channels[i].ds2438Addr, 0, sizeof(channels[i].ds2438Addr));
    channels[i].batteryPresent = false;
    channels[i].nextSend = 0;
    channels[i].lastRead = 0;
    channels[i].chargerLedState = CHARGER_LED_UNKNOWN;
    channels[i].dataBuffer = "";
  }
}


void setLed(byte ch, byte state) {
  if (ch == CHANNELS_ALL) {
    for (byte i = 0; i < CHANNELS_COUNT; i++)
      channels[i].ledState = state;
  } else {
    channels[ch].ledState = state;
  }
}

void resetChannelData(byte ch) {
    owChannel &c = channels[ch];

    c.dataBuffer = "";
    c.nextSend = 0;
    c.lastRead = 0;
    c.chargerLedState = CHARGER_LED_UNKNOWN;
}


void setupWifi() {
  WiFiMulti wifiMulti;

  WiFi.setHostname(HOSTNAME);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);

  addWifi(wifiMulti);

  Serial.print(F("Attempting WiFi connection ..."));

  if (wifiMulti.run(WIFI_TIMEOUT) != WL_CONNECTED) {
    Serial.println("error");
    setLed(CHANNELS_ALL, LED_BLINK_LONG);
    while (true) { }
  }
  Serial.printf_P(PSTR(" connected to %s, IP address: %s\n"), WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}


void setup() {
  initChannels();

  for (byte i = 0; i < CHANNELS_COUNT; i++) {
    pinMode(channels[i].ledPin, OUTPUT);
  }

  setLed(CHANNELS_ALL, LED_ON);
  Serial.begin(115200);

  xTaskCreate(&taskLED, "taskLED", 1024, NULL, 1, NULL);

  setLed(CHANNELS_ALL, LED_BLINK_SHORT);
  setupWifi();
  setLed(CHANNELS_ALL, LED_OFF);

  for (byte i = 0; i < CHANNELS_COUNT; i++) {
    char name[12];
    snprintf(name, sizeof(name), "taskCh%d", i);
    xTaskCreatePinnedToCore(&taskChannel, name, 8192,
        (void *)(uintptr_t)i, 2, NULL, ARDUINO_RUNNING_CORE);
  }
}

void loop() {
  vTaskDelete(NULL);
}


void taskChannel(void *pvParameters) {
  byte ch = (byte)(uintptr_t)pvParameters;

  unsigned long lastScan = 0;
  while (true) {
    unsigned long now = millis();
    if (now - lastScan >= SCAN_INTERVAL) {
      lastScan = now;
      scanDevices(ch);
    }
    if (channels[ch].batteryPresent && (now - channels[ch].lastRead >= READ_INTERVAL)) {
      channels[ch].lastRead = now;
      readChannelData(ch);
    }
    vTaskDelay(100 / portTICK_RATE_MS);
  }
}

void scanDevices(byte ch) {
  // Serial.printf("Ch%d: scan\n", ch);
  owChannel &c = channels[ch];
  uint8_t addr[8];
  uint8_t newDS2433[8] = {0};
  uint8_t newDS2438[8] = {0};

  while (true) {
    if (!c.ow->search(addr)) {
      c.ow->reset_search();
      vTaskDelay(DS_SEAT_DELAY / portTICK_RATE_MS);
      break;
    }

    if (addr[0] == DS2433_ID)
      memcpy(newDS2433, addr, 8);
    if (addr[0] == DS2438_ID)
      memcpy(newDS2438, addr, 8);
  }

  bool hadBattery = c.batteryPresent;
  bool hasBattery = (newDS2433[0] != 0x00 && newDS2438[0] != 0x00);
  bool addrChanged = (memcmp(newDS2433, c.ds2433Addr, 8) != 0) ||
                     (memcmp(newDS2438, c.ds2438Addr, 8) != 0);

  if (c.batteryPresent && !hasBattery) {
    Serial.printf("Ch%d: battery removed\n", ch);
    sendData(ch);
    resetChannelData(ch);
    c.batteryPresent = false;
    memset(c.ds2433Addr, 0, 8);
    memset(c.ds2438Addr, 0, 8);
    setLed(ch, LED_OFF);
  } else if (c.batteryPresent && hasBattery && addrChanged) {
    Serial.printf("Ch%d: battery replaced\n", ch);
    sendData(ch);
    resetChannelData(ch);
    memcpy(c.ds2433Addr, newDS2433, 8);
    memcpy(c.ds2438Addr, newDS2438, 8);
    c.batteryPresent = true;
    setLed(ch, LED_BLINK_LONG);
  } else if (!c.batteryPresent && hasBattery) {
    Serial.printf("Ch%d: battery inserted\n", ch);
    resetChannelData(ch);
    memcpy(c.ds2433Addr, newDS2433, 8);
    memcpy(c.ds2438Addr, newDS2438, 8);
    c.batteryPresent = true;
    setLed(ch, LED_BLINK_LONG);
  }
  // No change — do nothing
}


bool readDS2438(byte ch, String &output) {
  owChannel &c = channels[ch];

  c.ow->reset();
  c.ow->select(c.ds2438Addr);
  c.ow->write(0x44); // Start temperature conversion
  vTaskDelay(DS_READ_DELAY / portTICK_RATE_MS);
  c.ow->reset();
  c.ow->select(c.ds2438Addr);
  c.ow->write(0xB4); // Start voltage conversion
  vTaskDelay(DS_READ_DELAY / portTICK_RATE_MS);

  for (byte j = 0; j < 8; j++) {
    c.ow->reset();
    c.ow->select(c.ds2438Addr);
    c.ow->write(0xB8); // Recall memory
    c.ow->write(j);

    c.ow->reset();
    c.ow->select(c.ds2438Addr);
    c.ow->write(0xBE); // Read memory
    c.ow->write(j);

    uint8_t data[9];
    for (byte i = 0; i < 9; i++) {
      data[i] = c.ow->read();
      appendByte(output, data[i]);
    }

    if (c.ow->crc8(data, 8) != data[8]) {
      Serial.printf("Ch%d: invalid CRC for DS2438 page %d\n", ch, j);
      return false;
    }
  }
  return true;
}


bool readDS2433(byte ch, String &output, uint8_t *rawData) {
  owChannel &c = channels[ch];

  c.ow->reset();
  c.ow->select(c.ds2433Addr);
  c.ow->write(0xF0); // Read memory
  c.ow->write(0x00); // Offset low
  c.ow->write(0x00); // Offset high

  vTaskDelay(DS_READ_DELAY / portTICK_RATE_MS);

  bool emptyData = true;

  for (int i = 0; i < 512; i++) {
    rawData[i] = c.ow->read();

    if(emptyData && rawData[i]!=0x00 && rawData[i]!=0xFF)
      emptyData = false;
  }

  if(emptyData) {
    Serial.printf("Ch%d: DS2433 empty data\n", ch);
    return false;
  }

  // Verify with second read
  c.ow->reset();
  c.ow->select(c.ds2433Addr);
  c.ow->write(0xF0); // Read memory
  c.ow->write(0x00);
  c.ow->write(0x00);

  vTaskDelay(DS_READ_DELAY / portTICK_RATE_MS);

  bool onlyZero = true;

  for (int i = 0; i < 512; i++) {
    if (rawData[i] == c.ow->read()) {
      appendByte(output, rawData[i]);
    } else {
      Serial.printf("Ch%d: DS2433 verify failed at byte %d\n", ch, i);
      return false;
    }
  }
  return true;
}


void readChannelData(byte ch) {
  owChannel &c = channels[ch];

  for (byte attempt = 0; attempt < DS_MAX_RETRIES; attempt++) {
    if (attempt > 0) {
      Serial.printf("Ch%d: retry %d\n", ch, attempt);
      vTaskDelay(DS_RETRY_DELAY / portTICK_RATE_MS);
    }

    Serial.printf("Ch%d: reading data\n", ch);

    // Build ROM string from cached addresses
    String output = "";
    for (byte i = 0; i < 8; i++)
      appendByte(output, c.ds2438Addr[i]);
    output += "/";
    for (byte i = 0; i < 8; i++)
      appendByte(output, c.ds2433Addr[i]);
    output += "_";

    vTaskDelay(DS_SEAT_DELAY / portTICK_RATE_MS);

    if (!readDS2438(ch, output)) {
      if (attempt + 1 < DS_MAX_RETRIES) continue;
      setLed(ch, LED_BLINK_SHORT);
      return;
    }

    output += "_";

    uint8_t rawData[512];
    if (!readDS2433(ch, output, rawData)) {
      if (attempt + 1 < DS_MAX_RETRIES) continue;
      setLed(ch, LED_BLINK_SHORT);
      return;
    }

    // Success — process data
    unsigned int statusBlockOffset = (rawData[74] << 8) | rawData[75];
    byte newChargerLedState = rawData[statusBlockOffset + 1];

    if (newChargerLedState != c.chargerLedState) {
      Serial.printf("Ch%d: charger LED changed\n", ch);
      sendData(ch);
    }

    c.dataBuffer = output;

    if (c.nextSend < millis() || newChargerLedState != c.chargerLedState) {
      c.chargerLedState = newChargerLedState;
      sendData(ch);
    } else {
      setLed(ch, LED_ON);
    }

    return;
  }
}


void sendData(byte ch) {
  owChannel &c = channels[ch];

  if (c.dataBuffer.length() == 0)
    return;

  Serial.printf("Ch%d: data to send: %s\n", ch, c.dataBuffer.c_str());
  // Serial.println(c.dataBuffer);

  HTTPClient http;
  String serverPath = SERVER_URL + "?token=" + SERVER_TOKEN + "&data=" + c.dataBuffer;
  http.begin(serverPath.c_str(), le_root_ca_crt);

  int httpResponseCode = http.GET();

  String payload = http.getString();
  
  http.end();

  Serial.printf("Ch%d: HTTP %d: %s\n", ch, httpResponseCode, payload.c_str());
  

  if (httpResponseCode == 200) {
    setLed(ch, LED_ON);
    c.nextSend = millis() + SEND_INTERVAL;
  } else {
    setLed(ch, LED_BLINK_SHORT);
  }
}


void taskLED(void *pvParameters) {
  while (true) {
    unsigned long now = millis();

    for (byte i = 0; i < CHANNELS_COUNT; i++) {
      owChannel &ch = channels[i];

      switch (ch.ledState) {
        case LED_OFF:
          digitalWrite(ch.ledPin, LOW);
          break;

        case LED_ON:
          digitalWrite(ch.ledPin, HIGH);
          break;

        case LED_BLINK_SHORT:
          if (now - ch.lastToggle >= LED_BLINK_SHORT_INTERVAL) {
            ch.lastToggle = now;
            ch.ledLevel = !ch.ledLevel;
            digitalWrite(ch.ledPin, ch.ledLevel);
          }
          break;

        case LED_BLINK_LONG:
          if (now - ch.lastToggle >= LED_BLINK_LONG_INTERVAL) {
            ch.lastToggle = now;
            ch.ledLevel = !ch.ledLevel;
            digitalWrite(ch.ledPin, ch.ledLevel);
          }
          break;
      }
    }

    vTaskDelay(LED_TASK_CYCLE / portTICK_RATE_MS);
  }
}


void appendByte(String &str, uint8_t b) {
  str += "0123456789ABCDEF"[b / 16];
  str += "0123456789ABCDEF"[b % 16];
}
