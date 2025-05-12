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

int test_server_content(char *request, char *params, char *result, size_t result_size)
{
    printf("======================================================");
    printf(request);
    printf("======================================================");

    // A única rota que o servidor lida é "/"
    if (strncmp(request, "/sensors", sizeof("/sensors") - 1) == 0)
    {
        const char *html_content = "<html><body><h1>Servidor funcionando!</h1></body></html>";
        int len = strlen(html_content);
        if (len < result_size)
        {
            strncpy(result, html_content, result_size);
        }
        return len; // Retorna o tamanho do conteúdo gerado
    }
    else
    {
        // Resposta para URL não encontrada (fallback)
        const char *not_found_content = "<html><body><h1>404 Não encontrado</h1></body></html>";
        int len = strlen(not_found_content);
        if (len < result_size)
        {
            strncpy(result, not_found_content, result_size);
        }
        return len; // Retorna o tamanho do conteúdo gerado
    }
}

static err_t tcp_close_client_connection(void *arg, struct tcp_pcb *client_pcb, err_t close_err)
{
    if (client_pcb)
    {
        assert(con_state && con_state->pcb == client_pcb);
        tcp_arg(client_pcb, NULL);
        tcp_poll(client_pcb, NULL, 0);
        tcp_sent(client_pcb, NULL);
        tcp_recv(client_pcb, NULL);
        tcp_err(client_pcb, NULL);
        err_t err = tcp_close(client_pcb);
        if (err != ERR_OK)
        {
            printf("close failed %d, calling abort\n", err);
            tcp_abort(client_pcb);
            close_err = ERR_ABRT;
        }
    }
    return close_err;
}

err_t tcp_server_recv_fn(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    printf("TCP RECIEVE!");
    // 1. Se não houver dados, a conexão foi fechada pelo cliente
    if (!p)
    {
        // Realiza o encerramento da conexão TCP com o cliente
        err_t close_err = tcp_close(pcb); // Envia o pacote de fim (TCP FIN)
        if (close_err != ERR_OK)
        {
            printf("Failed to close TCP connection: %d\n", close_err);
            return close_err; // Caso falhe ao fechar, retorna o erro
        }

        // Libera a memória associada à conexão
        tcp_arg(pcb, NULL); // Remove os dados do argumento associado à conexão

        // Fecha o controle de protocolos de camada de conexão
        // Remover a conexão do PCB (Process Control Block)
        return ERR_OK; // Retorna OK se tudo foi realizado corretamente
    }

    // 2. Garante que o ponteiro de conexão está válido
    assert(pcb && arg);

    // 3. Só processa se houver dados recebidos
    if (p->tot_len > 0)
    {
        printf("tcp_server_recv %d bytes err %d\n", p->tot_len, err);

        // 4. Define buffers para armazenar os dados de headers e corpo
        char headers[512];
        char result[1024];
        int header_len = 0;
        int result_len = 0;
        int sent_len = 0;

        // 5. Copia os dados recebidos para o buffer de headers
        size_t max_copy = sizeof(headers) - 1;
        pbuf_copy_partial(p, headers, p->tot_len > max_copy ? max_copy : p->tot_len, 0);
        headers[max_copy] = '\0'; // Garantir terminação

        // 6. Verifica se é uma requisição GET
        if (strncmp("GET ", headers, 4) == 0)
        {
            char *request = headers + 4;
            char *params = strchr(request, '?');

            if (params && *params)
            {
                char *space = strchr(request, ' ');
                *params++ = 0; // Termina a string da URI
                if (space)
                    *space = 0; // Remove o resto após os parâmetros
            }
            else
            {
                params = NULL;
            }

            // 7. Gera o conteúdo de resposta no buffer result
            result_len = test_server_content(request, params, result, sizeof(result));
            printf("Request: %s?%s\n", request, params);
            printf("Result length: %d\n", result_len);

            if (result_len > sizeof(result) - 1)
            {
                printf("Result buffer overflow (%d bytes)\n", result_len);
                // return tcp_close_client_connection(pcb, ERR_CLSD);
            }

            // 8. Gera o cabeçalho HTTP adequado
            if (result_len > 0)
            {
                header_len = snprintf(
                    headers,
                    sizeof(headers),
                    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: %d\r\n\r\n",
                    result_len);
            }
            else
            {
                // Redirecionamento se sem conteúdo
                header_len = snprintf(
                    headers,
                    sizeof(headers),
                    "HTTP/1.1 302 Found\r\nLocation: http://%s\r\n\r\n/sensors",
                    ipaddr_ntoa(&pcb->local_ip));
                printf("Redirecting to %s\n", ipaddr_ntoa(&pcb->local_ip));
            }

            if (header_len > sizeof(headers) - 1)
            {
                printf("Header buffer overflow (%d bytes)\n", header_len);
                // Realiza o encerramento da conexão TCP com o cliente
                err_t close_err = tcp_close(pcb); // Envia o pacote de fim (TCP FIN)
                if (close_err != ERR_OK)
                {
                    printf("Failed to close TCP connection: %d\n", close_err);
                    return close_err; // Caso falhe ao fechar, retorna o erro
                }

                // Libera a memória associada à conexão
                tcp_arg(pcb, NULL); // Remove os dados do argumento associado à conexão

                // Fecha o controle de protocolos de camada de conexão
                return ERR_OK; // Retorna OK se tudo foi realizado corretamente
            }

            // 9. Envia cabeçalhos
            err_t e = tcp_write(pcb, headers, header_len, TCP_WRITE_FLAG_COPY);
            if (e != ERR_OK)
            {
                printf("Failed to write headers: %d\n", e);
                // Realiza o encerramento da conexão TCP com o cliente
                err_t close_err = tcp_close(pcb); // Envia o pacote de fim (TCP FIN)
                if (close_err != ERR_OK)
                {
                    printf("Failed to close TCP connection: %d\n", close_err);
                    return close_err; // Caso falhe ao fechar, retorna o erro
                }

                // Libera a memória associada à conexão
                tcp_arg(pcb, NULL); // Remove os dados do argumento associado à conexão

                return ERR_OK; // Retorna OK se tudo foi realizado corretamente
            }

            // 10. Envia corpo, se existir
            if (result_len > 0)
            {
                e = tcp_write(pcb, result, result_len, TCP_WRITE_FLAG_COPY);
                if (e != ERR_OK)
                {
                    printf("Failed to write body: %d\n", e);
                    // return tcp_close_client_connection(pcb, e);
                }
            }
        }

        // 11. Notifica o lwIP de que os dados foram processados
        tcp_recved(pcb, p->tot_len);
    }

    // 12. Libera o buffer da requisição
    pbuf_free(p);
    return ERR_OK;
}

static err_t tcp_server_poll(void *arg, struct tcp_pcb *pcb)
{
    printf("tcp_server_poll_fn\n");
    return tcp_close_client_connection(NULL, pcb, ERR_OK); // Just disconnect clent?
}

static err_t tcp_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    printf("tcp_server_sent %u\n", len);
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
    tcp_poll(new_pcb, tcp_server_poll, 10);
    tcp_sent(new_pcb, tcp_server_sent);
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

    err_t err = tcp_bind(pcb, IP_ADDR_ANY, TCP_PORT); // Bind to any IP on TCP_PORT
    if (err != ERR_OK)
    {                                                               // Check `err` not `pcb` for bind error
        printf("Não foi possível dar bind na porta 80: %d\n", err); // Original log
        tcp_close(pcb);                                             // Free pcb if bind fails
        return;
    }

    server_pcb = tcp_listen_with_backlog(pcb, 1); // Start listening (uses default backlog)
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

ip4_addr_t gw_ip;
ip4_addr_t mask;

/**
 * @brief Main setup function: initializes stdio, drivers, and hardware.
 */
void setup()
{
    stdio_init_all();
    init_display();
    init_wifi_ap();
    init_temp_sensor();
    adc_init();                        // General ADC init
    adc_set_temp_sensor_enabled(true); // Enable internal temperature sensor (ADC4)
    init_buttons();
    setup_joystick(); // Initializes ADC for joystick
    setup_pwm();

    sleep_ms(1000);
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

    // init_tcp_server(readings);

    while (true)
    {
        if (netif_default && netif_is_up(netif_default) && netif_is_link_up(netif_default))
        { // Check network status
            update_readings(readings);
            show_connection_status(); // Update display if available
            clear_display(true);      // Clear display if available
        }
        sleep_ms(1000);
    }

    free(readings);
    deinit_wifi();
    return 0;
}