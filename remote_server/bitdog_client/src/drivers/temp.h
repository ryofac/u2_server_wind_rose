/**
 * @file temp.h
 * @brief Interface para leitura da temperatura interna do RP2040.
 *
 * Este módulo fornece funções para inicializar o sensor de temperatura interno,
 * realizar a leitura da temperatura e converter a tensão lida para graus Celsius.
 */

#ifndef TEMP_H
#define TEMP_H

#include "hardware/adc.h"
#include "pico/stdlib.h"

/// Canal ADC utilizado para leitura da temperatura interna
#define ADC_TEMP_CHANNEL 4

/// Tensão de referência do ADC
#define ADC_VREF 3.3f

/// Resolução do ADC (12 bits)
#define ADC_RESOLUTION 4096.0f

/**
 * @brief Inicializa o sensor de temperatura interno do RP2040.
 *
 * Ativa o ADC e habilita o sensor de temperatura embutido no chip.
 */
void init_temp_sensor();

/**
 * @brief Lê a temperatura interna do microcontrolador.
 *
 * Realiza a leitura do canal ADC correspondente ao sensor de temperatura,
 * converte a leitura para tensão e depois para graus Celsius.
 *
 * @return Temperatura em graus Celsius.
 */
float read_internal_temperature();

/**
 * @brief Converte a tensão obtida do ADC em temperatura (graus Celsius).
 *
 * @param voltage Valor de tensão lido do ADC.
 * @return Temperatura correspondente em graus Celsius.
 */
float voltage_to_temperature_c(float voltage);

#endif
