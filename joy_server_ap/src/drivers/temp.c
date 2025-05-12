#include "temp.h"

/**
 * @brief Inicializa o ADC e ativa o sensor de temperatura.
 */
void init_temp_sensor()
{
    adc_init();
    adc_set_temp_sensor_enabled(true);
}

/**
 * @brief Realiza a leitura da temperatura interna.
 *
 * Faz a leitura bruta do ADC, converte para tensão e depois em temperatura.
 *
 * @return Temperatura em graus Celsius.
 */
float read_internal_temperature()
{
    adc_select_input(ADC_TEMP_CHANNEL);
    float conversion_factor = ADC_VREF / ADC_RESOLUTION;
    uint16_t raw = adc_read();

    float voltage = raw * conversion_factor;
    float temperature = voltage_to_temperature_c(voltage);

    return temperature;
}

/**
 * @brief Converte tensão em temperatura com base na fórmula do datasheet.
 *
 * A fórmula utilizada é: T(°C) = 27 - (V - 0.706) / 0.001721
 *
 * @param voltage Tensão lida no ADC.
 * @return Temperatura em graus Celsius.
 */
float voltage_to_temperature_c(float voltage)
{
    float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;
    return temperature;
}
