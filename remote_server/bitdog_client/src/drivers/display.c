#include "display.h"

ssd1306_t display;
uint8_t global_display_x = 0;
uint8_t global_display_y = 0;

/// Espaçamento entre linhas no display
const uint8_t line_spacing = 3;

/**
 * @brief Inicializa o barramento I2C para o display.
 *
 * Define os pinos de SDA e SCL, e configura os pull-ups necessários.
 */
void init_i2c()
{
    i2c_init(i2c1, DISPLAY_FREQUENCY);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

/**
 * @brief Inicializa o display SSD1306.
 *
 * Configura o barramento I2C, inicializa o driver do display e limpa a tela.
 * @return PICO_OK em caso de sucesso, ou erro genérico.
 */
int init_display()
{
    init_i2c();

    if (!ssd1306_init(&display, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_ADDRESS, i2c1))
    {
        printf("Falha ao inicializar o display SSD1306\n");
        return PICO_ERROR_GENERIC;
    }

    show("Iniciando...", true);
    clear_display(true);
    return PICO_OK;
}

/**
 * @brief Limpa o display e exibe uma linha divisória.
 *
 * Se o parâmetro `reset_screen` for verdadeiro, zera as coordenadas de posição de texto.
 *
 * @param reset_screen Define se as variáveis globais devem ser reiniciadas.
 */
void clear_display(bool reset_screen)
{
    ssd1306_clear(&display);
    show("==================", false);

    if (reset_screen)
    {
        global_display_x = 0;
        global_display_y = 0;
    }
}

/**
 * @brief Desenha uma string no display.
 *
 * A string será posicionada verticalmente conforme o valor atual de `global_display_y`.
 * Se `render_now` for verdadeiro, a tela será atualizada imediatamente.
 *
 * @param text Texto a ser exibido.
 * @param render_now Se verdadeiro, atualiza a tela após desenhar.
 */
void show(const char *text, bool render_now)
{
    ssd1306_draw_string(&display, 0, global_display_y, 1, text);
    global_display_y += line_spacing * 5;

    if (render_now)
    {
        ssd1306_show(&display);
    }
}
