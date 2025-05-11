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
 *  @brief Pico W TCP server for sensor data (joystick, buttons, temperature).
 */

/** @brief TCP server port. */
#define TCP_PORT 80
/** @brief TCP pending connections limit (used with `tcp_listen_with_backlog`, `tcp_listen` uses default). */
#define TCP_PENDING_CONNECTIONS_LIMIT 1

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

/** @brief TCP server Protocol Control Block. */
struct tcp_pcb *server_pcb = {0};

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
 * @brief TCP callback: Closes connection after data is successfully sent.
 * @param arg User argument (unused).
 * @param tpcb TCP Protocol Control Block.
 * @param len Number of bytes acknowledged as sent.
 * @return err_t ERR_OK on success, or closes connection.
 */
static err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    (void)arg; // Unused
    (void)len; // Unused
    return tcp_close(tpcb);
}

/**
 * @brief Determines wind rose direction from joystick X, Y.
 * @param x Joystick X value.
 * @param y Joystick Y value.
 * @return const char* Direction string (e.g., "NORTE", "CENTRO").
 */
const char *get_wind_rose_direction(float x, float y)
{
    const float threshold = 0.5;
    if (x > threshold)
    {
        if (y > threshold)
            return "NORDESTE";
        else if (y < -threshold)
            return "SUDESTE";
        else
            return "LESTE";
    }
    else if (x < -threshold)
    {
        if (y > threshold)
            return "NOROESTE";
        else if (y < -threshold)
            return "SUDOESTE";
        else
            return "OESTE";
    }
    else
    {
        if (y > threshold)
            return "NORTE";
        else if (y < -threshold)
            return "SUL";
    }
    return "CENTRO";
}

/**
 * @brief TCP callback: Handles received client data and sends HTML response.
 * @param arg Pointer to SENSOR_DATA_T.
 * @param tpcb TCP Protocol Control Block.
 * @param p Received pbuf chain (NULL if connection closed).
 * @param err Error code.
 * @return err_t ERR_OK on success.
 */
static err_t tcp_server_recv_fn(void *arg, struct tcp_pcb *tpcb,
                                struct pbuf *p, err_t err)
{
    SENSOR_DATA_T *readings = (SENSOR_DATA_T *)arg;

    if (!p)
    { // Client closed connection
        tcp_recv(tpcb, NULL);
        tcp_close(tpcb);
        printf("P está vazio (conexão fechada pelo cliente ou erro)\n"); // Original log
        return ERR_OK;
    }

    // Assumes any received data is an HTTP GET request.
    // The request content itself is not parsed here.
    // `request` buffer is allocated but its content not used for response logic.
    char *request = (char *)malloc(p->tot_len + 1);
    pbuf_copy_partial(p, request, p->tot_len, 0);
    request[p->tot_len] = '\0';
    // Original code does not use 'request' content.
    // printf("Request: %s\n", request); // Optional: log request

    // Temperature reading for HTML is from `readings->temperature`
    // `read_internal_temperature()` call here is redundant if `update_readings` is current.
    // float internal_temp = read_internal_temperature(); // This value is not used in snprintf.

    char headers[256];
    char body[4092]; // Buffer for HTML body

    snprintf(body, sizeof(body),
             "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta http-equiv=\"refresh\" content=\"1\"><title>Pico W - Status</title>"
             "<style>body{background:#1a1a1a;color:#00ff00;font-family:monospace;display:flex;justify-content:center;align-items:center;height:100vh;margin:0;}"
             ".box{border:2px solid #00ff00;padding:20px;text-align:center;min-width:300px;min-height:200px;}"
             "h1{font-size:24px;margin-bottom:10px;}p{margin:5px 0;font-size:16px;}</style></head>"
             "<body><div class=\"box\"><h1>PicoW Status</h1>"
             "<p>Joystick X: %.2f</p><p>Joystick Y: %.2f</p>"
             "<p>Botão A: %d</p><p>Botão B: %d</p>"
             "<p>Temperatura: %.2f °C</p>"
             "<p>Direção: <strong>%s</strong></p>"
             "</div></body></html>",
             readings->analog_x, readings->analog_y,
             readings->button_a, readings->button_b,
             readings->temperature, // Uses the value from SENSOR_DATA_T
             get_wind_rose_direction(readings->analog_x, readings->analog_y));

    int content_length = strlen(body);
    snprintf(headers, sizeof(headers),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
             content_length);

    char html[sizeof(headers) + sizeof(body)]; // Ensure buffer is large enough
    snprintf(html, sizeof(html), "%s%s", headers, body);

    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    err_t error = tcp_output(tpcb);
    if (error != ERR_OK)
    {
        printf("Erro no TCP OUTPUT: %d\n", error); // Original log
    }

    free(request);
    pbuf_free(p);

    // Set sent callback to close connection after data is acknowledged
    tcp_sent(tpcb, tcp_sent_callback);

    return ERR_OK;
}

/**
 * @brief TCP callback: Accepts new client connections.
 * @param arg User argument (pointer to SENSOR_DATA_T).
 * @param new_pcb PCB for the new connection.
 * @param err Error code.
 * @return err_t ERR_OK on success.
 */
static err_t tcp_server_accept_callback(void *arg, struct tcp_pcb *new_pcb, err_t err)
{
    if (err != ERR_OK || new_pcb == NULL)
    {
        printf("Erro ao aceitar nova conexão: %d\n", err); // Original log
        return ERR_VAL;                                    // Or appropriate error
    }
    tcp_arg(new_pcb, arg);
    tcp_recv(new_pcb, tcp_server_recv_fn);
    return ERR_OK;
}

/**
 * @brief Initializes the TCP server.
 * @param tcp_var User argument for TCP callbacks (pointer to SENSOR_DATA_T).
 */
void init_tcp_server(void *tcp_var)
{
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb)
    {
        printf("Não foi possível iniciar o PCB\n"); // Original log
        return;
    }

    err_t err = tcp_bind(pcb, IP_ANY_TYPE, TCP_PORT); // Bind to any IP on TCP_PORT
    if (err != ERR_OK)
    {                                                               // Check `err` not `pcb` for bind error
        printf("Não foi possível dar bind na porta 80: %d\n", err); // Original log
        tcp_close(pcb);                                             // Free pcb if bind fails
        return;
    }

    server_pcb = tcp_listen(pcb); // Start listening (uses default backlog)
    if (!server_pcb)
    {
        printf("Não foi possível escutar na porta 80 (tcp_listen falhou)\n");
        if (pcb != NULL)
        {
            tcp_close(pcb); // pcb is the original, server_pcb is the listening one
        }
        return;
    }

    tcp_arg(server_pcb, tcp_var);
    tcp_accept(server_pcb, tcp_server_accept_callback);
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
    // init_display();       // Assumes drivers/display.h
    // init_wifi_sta();      // Assumes drivers/wifi.h
    // connect_to_wifi();    // Assumes drivers/wifi.h
    // init_temp_sensor();   // Assumes drivers/temp.h (for separate temp sensor)

    // Basic Wi-Fi and internal temperature sensor init
    if (cyw43_arch_init())
    {
        printf("Falha ao inicializar cyw43_arch\n");
        return;
    }
    cyw43_arch_enable_sta_mode();
    // Assumes WIFI_SSID and WIFI_PASSWORD are defined elsewhere
    // printf("Conectando a %s...\n", WIFI_SSID);
    // if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
    //     printf("Falha ao conectar.\n");
    // } else {
    //     printf("Conectado. IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
    // }

    adc_init();                        // General ADC init
    adc_set_temp_sensor_enabled(true); // Enable internal temperature sensor (ADC4)

    init_buttons();
    setup_joystick(); // Initializes ADC for joystick
    // setup_pwm(); // Call if PWM LEDs are actively used
}

/**
 * @brief Displays Wi-Fi connection status (intended for external display).
 * @note Relies on `show()` and `clear_display()` from `drivers/display.h`.
 *       Also assumes `WIFI_SSID`, `WIFI_PASSWORD` are defined.
 */
void show_connection_status()
{
    // show("-=-REDE-=-=", false);
    // show(WIFI_SSID, false);
    // show(WIFI_PASSWORD, false); // Caution: Displaying password
    // char ip_msg[50];
    // if (netif_default) { // Check if netif_default is valid
    //    snprintf(ip_msg, sizeof(ip_msg), "IP: %s", ipaddr_ntoa(&netif_default->ip_addr));
    //    show(ip_msg, true);
    // }
    // clear_display(true);
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

/**
 * @brief Main application entry point and loop.
 * @return int Exit code (should not return).
 */
int main()
{
    setup();

    SENSOR_DATA_T *readings = (SENSOR_DATA_T *)malloc(sizeof(SENSOR_DATA_T));
    if (!readings)
    {
        // Handle allocation failure
        printf("Falha ao alocar SENSOR_DATA_T\n");
        return 1;
    }
    init_tcp_server(readings);

    while (true)
    {
        cyw43_arch_poll(); // Essential for lwIP and Wi-Fi event processing

        if (netif_default && netif_is_up(netif_default) && netif_is_link_up(netif_default))
        { // Check network status
            update_readings(readings);
            // show_connection_status(); // Update display if available
            // clear_display(true);      // Clear display if available
        }
        sleep_ms(1000);
    }

    free(readings);
    deinit_wifi(); // Cleanup (though loop is infinite)
    return 0;
}