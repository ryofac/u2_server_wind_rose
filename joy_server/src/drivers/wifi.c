#include "wifi.h"

void init_wifi_ap()
{
    // Initialise the Wi-Fi chip
    if (cyw43_arch_init())
    {
        printf("Wi-Fi init failed\n");
        return;
    }

    // Enable wifi station
    cyw43_arch_enable_ap_mode(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
}

void init_wifi_sta()
{
    // Initialise the Wi-Fi chip
    if (cyw43_arch_init())
    {
        printf("Wi-Fi init failed\n");
        return;
    }

    // Enable wifi station
    cyw43_arch_enable_sta_mode();
}

void connect_to_wifi()
{
    printf("Connecting to Wi-Fi...\n");
    int err = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, WIFI_TIMEOUT_MS);
    if (err)
    {
        printf("failed to connect. %d \n", err);
        return;
    }
    else
    {
        printf("Connected.\n");
        // Read the ip address in a human readable way
        uint8_t *ip_address = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
        printf("IP address %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    }
}