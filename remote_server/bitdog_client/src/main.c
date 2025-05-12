#include <stdio.h>

#include "hardware/adc.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

// Incluindo bibliotecas de hardware
#include "hardware/pwm.h" // PWM
#include "hardware/adc.h" // ADC - Joystick

#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"

#include "drivers/display.h"
#include "drivers/wifi.h"
#include "drivers/temp.h"

/** @file main.c
 *  @brief Pico W HTTP client for sending sensor data (joystick, buttons, temperature).
 */

/** @brief GPIO pin for Joystick (ADC0, typically Y-axis). */
#define JOY_DIR_PIN 26

/** @brief GPIO pin for Red LED (PWM). */
#define RED_LED_PIN 13
/** @brief GPIO pin for Blue LED (PWM). */
#define BLUE_LED_PIN 12

/** @brief GPIO pin for Button A. */
#define BTA 5
/** @brief GPIO pin for Button B. */
#define BTB 6

/** @brief PWM period (wrap value). */
const uint16_t PERIOD_PWM = 255;
/** @brief PWM clock divider. */
const float DIVIDER_PWM = 16;

/** @brief TCP client Protocol Control Block. */
struct tcp_pcb *client_pcb = {0};

/** @brief Server witch the data will be sent. */
#define HTTP_SERVER "192.168.181.161"
#define HTTP_SERVER_PORT 5000
#define DATA_ENDPOINT "/update_readings"

/**
 * @brief Stores sensor readings.
 */
typedef struct
{
    float analog_x;    ///< Joystick X-axis value (-1.0 to 1.0).
    float analog_y;    ///< Joystick Y-axis value (-1.0 to 1.0).
    float temperature; ///< Internal temperature (°C).
    uint8_t button_a;  ///< Button A state (1 if pressed).
    uint8_t button_b;  ///< Button B state (1 if pressed).

} SENSOR_DATA_T;

/**
 * @brief Configures PWM for Red and Blue LEDs.
 */
void setup_pwm()
{
    gpio_init(RED_LED_PIN);
    gpio_set_dir(RED_LED_PIN, GPIO_OUT);
    gpio_set_function(RED_LED_PIN, GPIO_FUNC_PWM);

    uint16_t slice_red = pwm_gpio_to_slice_num(RED_LED_PIN);
    pwm_set_clkdiv(slice_red, DIVIDER_PWM);
    pwm_set_wrap(slice_red, PERIOD_PWM);
    pwm_set_gpio_level(RED_LED_PIN, 0);
    pwm_set_enabled(slice_red, true);

    gpio_init(BLUE_LED_PIN);
    gpio_set_dir(BLUE_LED_PIN, GPIO_OUT);
    gpio_set_function(BLUE_LED_PIN, GPIO_FUNC_PWM);

    uint16_t slice_blue = pwm_gpio_to_slice_num(BLUE_LED_PIN);
    pwm_set_clkdiv(slice_blue, DIVIDER_PWM);
    pwm_set_wrap(slice_blue, PERIOD_PWM);
    pwm_set_gpio_level(BLUE_LED_PIN, 0);
    pwm_set_enabled(slice_blue, true);
}

/**
 * @brief Initializes ADC and joystick GPIO pin (ADC0).
 */
void setup_joystick()
{
    adc_init();
    adc_gpio_init(JOY_DIR_PIN); // Configures GPIO26 (ADC0)
}

/**
 * @brief Reads and normalizes joystick X-axis value (ADC1).
 * @return float Normalized X-axis value (-1.0 to 1.0).
 */
float read_analog_x()
{
    adc_select_input(1); // ADC1 for X-axis
    uint16_t raw_value = adc_read();
    uint16_t dead_zone = 400;
    float joy_x;

    if (raw_value > (2048 + dead_zone))
        joy_x = (float)(raw_value - 2048 - dead_zone) / (2047.0 - dead_zone);
    else if (raw_value < (2048 - dead_zone))
        joy_x = (float)(raw_value - 2048 + dead_zone) / (2047.0 - dead_zone);
    else
        joy_x = 0;
    return joy_x;
}

/**
 * @brief Reads and normalizes joystick Y-axis value (ADC0).
 * @return float Normalized Y-axis value (-1.0 to 1.0).
 */
float read_analog_y()
{
    adc_select_input(0); // ADC0 for Y-axis
    uint16_t raw_value = adc_read();
    uint16_t dead_zone = 400;
    float joy_y;

    if (raw_value > (2048 + dead_zone))
        joy_y = (float)(raw_value - 2048 - dead_zone) / (2047.0 - dead_zone);
    else if (raw_value < (2048 - dead_zone))
        joy_y = (float)(raw_value - 2048 + dead_zone) / (2047.0 - dead_zone);
    else
        joy_y = 0;
    return joy_y;
}

/**
 * @brief Deinitializes Wi-Fi architecture.
 */
void deinit_wifi()
{
    cyw43_arch_deinit();
}

/**
 * @brief Initializes button GPIOs as inputs with pull-ups.
 */
void init_buttons()
{
    gpio_init(BTA);
    gpio_set_dir(BTA, GPIO_IN);
    gpio_pull_up(BTA);

    gpio_init(BTB);
    gpio_set_dir(BTB, GPIO_IN);
    gpio_pull_up(BTB);
}

/**
 * @brief Main setup function: initializes stdio, drivers, and hardware.
 */
void setup()
{
    stdio_init_all();
    init_display();
    init_wifi_sta();
    connect_to_wifi();
    init_temp_sensor();

    adc_init();                        // General ADC init
    adc_set_temp_sensor_enabled(true); // Enable internal temperature sensor (ADC4)

    init_buttons();
    setup_joystick();
    setup_pwm();
}

/**
 * @brief Displays Wi-Fi connection status (intended for external display).
 * @note Relies on `show()` and `clear_display()` from `drivers/display.h`.
 *       Also assumes `WIFI_SSID`, `WIFI_PASSWORD` are defined.
 */
void show_connection_status()
{
    show("-=-REDE-=-=", false);
    show(WIFI_SSID, false);
    show(WIFI_PASSWORD, false);
    char ip_msg[50];
    if (netif_default)
    { // Check if netif_default is valid
        snprintf(ip_msg, sizeof(ip_msg), "IP: %s", ipaddr_ntoa(&netif_default->ip_addr));
        show(ip_msg, true);
    }
    clear_display(true);
}

/**
 * @brief Updates all sensor readings into the SENSOR_DATA_T struct.
 * @param readings Pointer to SENSOR_DATA_T to update.
 */
void update_readings(SENSOR_DATA_T *readings)
{
    readings->analog_x = read_analog_x();
    readings->analog_y = read_analog_y();

    // Read internal temperature sensor (ADC4)
    adc_select_input(4);
    uint16_t temp_raw = adc_read();
    float conversion_factor = 3.3f / (1 << 12); // ADC is 12-bit
    float voltage = temp_raw * conversion_factor;
    readings->temperature = 27.0f - (voltage - 0.706f) / 0.001721f; // Formula from datasheet

    readings->button_a = !gpio_get(BTA); // Inverted due to pull-up
    readings->button_b = !gpio_get(BTB); // Inverted due to pull-up

    printf("UPDATE: X=%.2f Y=%.2f A=%d B=%d T=%.2f\n",
           readings->analog_x, readings->analog_y,
           readings->button_a, readings->button_b, readings->temperature);
}

static void http_client_send_post(struct tcp_pcb *tpcb, SENSOR_DATA_T *data)
{
    char body[512];
    snprintf(body, sizeof(body),
             "{\"temp\":%.2f,\"joy_x\":%.2f,\"joy_y\":%.2f,\"btn_a\":%d,\"btn_b\":%d}",
             data->temperature, data->analog_x, data->analog_y,
             data->button_a, data->button_b);

    int body_len = strlen(body);

    char request[1024];
    snprintf(request, sizeof(request),
             "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             DATA_ENDPOINT, HTTP_SERVER, body_len, body);

    tcp_write(tpcb, request, strlen(request), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
}

static err_t http_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        return ERR_OK;
    }

    // (Opcional) Lê resposta do servidor
    char *resp = malloc(p->tot_len + 1);
    pbuf_copy_partial(p, resp, p->tot_len, 0);
    resp[p->tot_len] = '\0';
    printf("Resposta do servidor: %s\n", resp);
    free(resp);

    pbuf_free(p);
    tcp_close(tpcb);
    return ERR_OK;
}

static err_t http_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    if (err != ERR_OK)
    {
        printf("Erro na conexão: %d\n", err);
        tcp_close(tpcb);
        return err;
    }

    SENSOR_DATA_T *data = (SENSOR_DATA_T *)arg;
    tcp_recv(tpcb, http_client_recv); // opcional: tratar resposta
    http_client_send_post(tpcb, data);
    return ERR_OK;
}

void send_sensor_data(SENSOR_DATA_T *data)
{
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb)
    {
        printf("Erro ao criar PCB TCP\n");
        return;
    }

    ip_addr_t server_ip;
    ipaddr_aton(HTTP_SERVER, &server_ip);

    err_t err = tcp_connect(pcb, &server_ip, HTTP_SERVER_PORT, http_client_connected);
    if (err != ERR_OK)
    {
        printf("Erro ao conectar: %d\n", err);
        tcp_abort(pcb);
        return;
    }

    tcp_arg(pcb, data); // Passa o ponteiro dos dados como contexto
}

/**
 * @brief Main application entry point and loop.
 * @return int Exit code (should not return).
 */
int main()
{
    setup();

    SENSOR_DATA_T *readings = (SENSOR_DATA_T *)malloc(sizeof(SENSOR_DATA_T));

    while (true)
    {
        cyw43_arch_poll();

        if (netif_default && netif_is_up(netif_default) && netif_is_link_up(netif_default))
        { // Check network status
            update_readings(readings);
            send_sensor_data(readings);
            show_connection_status();
            clear_display(true);
        }
        sleep_ms(1000);
    }

    free(readings);
    deinit_wifi();
    return 0;
}