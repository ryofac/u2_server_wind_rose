#ifndef WIFI_H
#define WIFI_H

#define WIFI_SSID "u_ead"
#define WIFI_PASSWORD "batatinha"
#define WIFI_TIMEOUT_MS 10000

#include "pico/cyw43_arch.h"

void init_wifi_sta();

void init_wifi_ap();

void connect_to_wifi();

#endif
