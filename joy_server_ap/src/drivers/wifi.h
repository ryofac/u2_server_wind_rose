#ifndef WIFI_H
#define WIFI_H

#define WIFI_SSID "dogaolab"
#define WIFI_PASSWORD "batatinha123"
#define WIFI_TIMEOUT_MS 10000

#include "pico/cyw43_arch.h"
#include "dhcpserver.h"
#include "dnsserver.h"

extern ip_addr_t gateway_ip;
extern ip_addr_t df_mask;

void init_wifi_sta();

void init_wifi_ap();

void connect_to_wifi();

#endif
