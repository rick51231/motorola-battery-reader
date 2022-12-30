#include <OneWire.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include "defines.h"
#include "settings.h"

OneWire ds(DS_PIN);

void taskDS(void *pvParameters);
void taskLED(void *pvParameters);
void setColor(byte color, bool blink);
void appendByte(String &str, uint8_t b);
byte dsDeviceCount();

byte ledColor = COLOR_NONE;
bool ledBlink = false;

void setupWifi() {
    WiFiMulti wifiMulti;

    WiFi.setHostname("BatteryReader");
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);

    addWifi(wifiMulti);

    Serial.print(F("Attempting WiFi connection ..."));

    if (wifiMulti.run(WIFI_TIMEOUT) != WL_CONNECTED) {
        Serial.println("error");

        setColor(COLOR_RED, true);
        while(true) {

        }
    }
    Serial.printf_P(PSTR(" connected to %s, IP address: %s\n"), WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

void setup() {
    Serial.begin(115200);
    // ds2438.begin();
    pinMode(PULLUP_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_RED_PIN, OUTPUT);

    xTaskCreate(&taskLED, "taskLED", 1024, NULL, 1, NULL);

    setColor(COLOR_RED, false);

    setupWifi();

    setColor(COLOR_GREEN, false);
    delay(1000);
    setColor(COLOR_NONE, false);

    digitalWrite(PULLUP_PIN, HIGH);
    
    xTaskCreatePinnedToCore(&taskDS, "taskDS", 8192, NULL, 2, NULL, ARDUINO_RUNNING_CORE);
}



void loop() {
    vTaskDelete(NULL);
}

void taskDS(void *pvParameters) {
    // while(true) {
    //     byte addr[8];
    //     uint8_t bb = 0;
    //     if (!ds.search(addr))
    //     {
    //         Serial.println(" No more addresses.");
    //         Serial.println();
    //         ds.reset_search();
    //         delay(250);
    //         continue;
    //     }
    //     Serial.print(" ROM =");
    //     for (byte i = 0; i < 8; i++)
    //     {
    //         Serial.write(' ');
    //         Serial.print(addr[i], HEX);
    //     }
    //     Serial.println("!");
    // }
    uint8_t addr[8];
    uint8_t DS2433_addr[8];
    uint8_t DS2438_addr[8];
    String output = "";

    loop: while(true) {
        vTaskDelay(250 / portTICK_RATE_MS);

        setColor(COLOR_NONE, false);

        memset(DS2433_addr, 0, sizeof(DS2433_addr));
        memset(DS2438_addr, 0, sizeof(DS2438_addr));

        output = "";

        while(true) { //Search devices
            if (!ds.search(addr)) {
                ds.reset_search();
                vTaskDelay(250 / portTICK_RATE_MS);
                break;
            }
            setColor(COLOR_RED, false);

            Serial.print("ROM =");
            for (byte i = 0; i < 8; i++) {
              Serial.write(' ');
              Serial.print(addr[i], HEX);

              appendByte(output, addr[i]);
            }
            output += "/";
            Serial.println();

            if(addr[0] == DS2433_ID)
                memcpy(DS2433_addr, addr, sizeof(addr));

            if(addr[0] == DS2438_ID)
                memcpy(DS2438_addr, addr, sizeof(addr));
        }


        if(DS2433_addr[0] == 0x00 || DS2438_addr[0] == 0x00)
            continue;

        output.remove(output.length() - 1);

        output += "_";

        setColor(COLOR_ORANGE, false);

        vTaskDelay(250 / portTICK_RATE_MS); //Ensure proper seat

        // Read DS2438 chip
        ds.reset();
        ds.select(DS2438_addr);
        ds.write(0x44); // Start temperature conversion
        vTaskDelay(10 / portTICK_RATE_MS);
        ds.reset();
        ds.select(DS2438_addr);
        ds.write(0xB4); // Start voltage conversion
        vTaskDelay(10 / portTICK_RATE_MS);

        //Read all pages
        for (byte j = 0; j < 8; j++)  {
            ds.reset();
            ds.select(DS2438_addr);
            ds.write(0xB8); // Recall memory
            ds.write(j);

            ds.reset();
            ds.select(DS2438_addr);
            ds.write(0xBE); // Read memory
            ds.write(j);

            uint8_t data[9];

            for (byte i = 0; i < 9; i++) {
                data[i] = ds.read();

                appendByte(output, data[i]);
            }
            
            if(ds.crc8(data, 8) != data[8]) {
                Serial.print("Invalid CRC for PAGE ");
                Serial.println(j);
                goto loop;
            }
        }

        output += "_";

        // Read DS2433 chip
        ds.reset();
        ds.select(DS2433_addr);
        ds.write(0xF0); // Read memory
        ds.write(0x00); // Offset
        ds.write(0x00); // Offset

        uint8_t data[512];
        for (int i = 0; i < 512; i++) //TODO: CRC?
            data[i] = ds.read();
    

        //Verify
        ds.reset();
        ds.select(DS2433_addr);
        ds.write(0xF0); // Read memory
        ds.write(0x00); // Offset
        ds.write(0x00); // Offset

        for (int i = 0; i < 512; i++) {
            if (data[i] == ds.read())
                appendByte(output, data[i]);
            else {
                Serial.print("Invalid data byte ");
                Serial.println(i);
                goto loop;
            }
        }
        

        setColor(COLOR_GREEN, true);

        Serial.println(output);

        HTTPClient http;
        String serverPath = SERVER_URL + "?token=" + SERVER_TOKEN + "&data=" + output;
        http.begin(serverPath.c_str(), le_root_ca_crt);

        int httpResponseCode = http.GET();

        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
        http.end();

        if (httpResponseCode == 200) {
            setColor(COLOR_GREEN, false);
            dsDeviceCount(); // Idk why, buy first time it is always returns 0

            while(true) {
                vTaskDelay(250 / portTICK_RATE_MS);

                if(dsDeviceCount() == 0)
                    break;
            }
        } else {
            setColor(COLOR_RED, true);
        }

        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

void taskLED(void *pvParameters) {
    while(true) {
        digitalWrite(LED_GREEN_PIN, (ledColor == COLOR_ORANGE || ledColor == COLOR_GREEN) ? HIGH : LOW);
        digitalWrite(LED_RED_PIN, (ledColor == COLOR_ORANGE || ledColor == COLOR_RED) ? HIGH : LOW);
        vTaskDelay(250 / portTICK_RATE_MS);

        if(ledColor != COLOR_NONE && ledBlink) {
            digitalWrite(LED_GREEN_PIN, LOW);
            digitalWrite(LED_RED_PIN, LOW);
            vTaskDelay(250 / portTICK_RATE_MS);
        }
    }
}

byte dsDeviceCount() {
    byte ret = 0;

    uint8_t addr[8];

    while(true) {
        if (!ds.search(addr)) {
            ds.reset_search();
            break;
        }

        ret++;
    }

    return ret;
}

void setColor(byte color, bool blink) {
    ledColor = color;
    ledBlink = blink;
}

void appendByte(String &str, uint8_t b) {
    str += "0123456789ABCDEF"[b / 16];
    str += "0123456789ABCDEF"[b % 16];
}