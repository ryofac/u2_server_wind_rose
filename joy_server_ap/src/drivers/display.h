/**
 * @file display.h
 * @brief Interface para controle de display OLED SSD1306 via I2C.
 *
 * Fornece funções para inicialização, limpeza e exibição de texto no display OLED,
 * utilizando a biblioteca ssd1306. Os pinos e parâmetros de comunicação I2C são definidos aqui.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include "ssd1306.h"
#include "hardware/i2c.h"
#include <stdio.h>
#include "pico/stdlib.h"

/// Largura do display em pixels
#define SCREEN_WIDTH 128

/// Altura do display em pixels
#define SCREEN_HEIGHT 64

/// Endereço I2C do display SSD1306
#define SCREEN_ADDRESS 0x3C

/// Frequência de comunicação I2C (400 kHz)
#define DISPLAY_FREQUENCY 400000

/// Pino SDA do barramento I2C
#define I2C_SDA 14

/// Pino SCL do barramento I2C
#define I2C_SCL 15

/// Objeto que representa o display SSD1306
extern ssd1306_t display;

/// Coordenada X global usada para posicionamento de texto
extern uint8_t global_display_x;

/// Coordenada Y global usada para posicionamento de texto
extern uint8_t global_display_y;

/**
 * @brief Inicializa o display SSD1306.
 *
 * Configura o barramento I2C e o display OLED.
 * @return PICO_OK em caso de sucesso, PICO_ERROR_GENERIC em caso de erro.
 */
int init_display();

/**
 * @brief Exibe uma string no display.
 *
 * Desenha o texto a partir da posição atual de Y no display. Pode atualizar imediatamente a tela.
 *
 * @param text Texto a ser exibido.
 * @param render_now Se verdadeiro, o conteúdo será renderizado imediatamente.
 */
void show(const char *text, bool render_now);

/**
 * @brief Limpa o display.
 *
 * Apaga o conteúdo da tela e exibe uma linha divisória. Opcionalmente, reinicia as coordenadas globais.
 *
 * @param reset_screen Se verdadeiro, zera as variáveis globais de posição.
 */
void clear_display(bool reset_screen);

#endif
