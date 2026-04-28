#ifndef SETTINGS_H
#define SETTINGS_H

#include <WiFiMulti.h>
#include <defines.h>

// WiFi
void addWifi(WiFiMulti &wifiMulti) {
    wifiMulti.addAP("YourSSID", "YourPassword");
}

#define HOSTNAME     "BatteryReaderMulti"
// Server
const String SERVER_URL   = "https://yoursite.com/battery_data";
const String SERVER_TOKEN = "your_token_here";

// SSL certificate
const char *le_root_ca_crt =
    "-----BEGIN CERTIFICATE-----\n"
    "...\n"
    "-----END CERTIFICATE-----\n";


// Number of channels
#define CHANNELS_COUNT  4

// Channel pin mappings: { OneWire GPIO, LED GPIO }
const ChannelPins CHANNEL_PINS[CHANNELS_COUNT] = {
    { 26, 27 },  // Channel 0
    { 18, 33 },  // Channel 1
    { 19, 2 },  // Channel 2
    { 23, 5 },  // Channel 3
};

#endif
