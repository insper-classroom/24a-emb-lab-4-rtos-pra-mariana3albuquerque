

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "ssd1306.h"
#include "gfx.h"

// Pin configuration
const int TRIGGER_PIN = 16;
const int ECHO_PIN = 17;

//OLED
// const uint BTN_1_OLED = 28;
// const uint BTN_2_OLED = 26;
// const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

// FreeRTOS Handles
SemaphoreHandle_t xSemaphoreTrigger;
QueueHandle_t xQueueDistance;

// OLED Display object
ssd1306_t disp;

void oled_init(void) {
    gpio_init(LED_1_OLED);
    gpio_set_dir(LED_1_OLED, GPIO_OUT);

    gpio_init(LED_2_OLED);
    gpio_set_dir(LED_2_OLED, GPIO_OUT);

    gpio_init(LED_3_OLED);
    gpio_set_dir(LED_3_OLED, GPIO_OUT);


}

void ECHO_PIN_callback(uint gpio, uint32_t events) {
    static BaseType_t xHigherPriorityTaskWoken;
    static absolute_time_t rise_time;

    if (events & GPIO_IRQ_EDGE_RISE) {
        rise_time = get_absolute_time();
    } else if (events & GPIO_IRQ_EDGE_FALL) {
        absolute_time_t fall_time = get_absolute_time();
        uint32_t difference_us = absolute_time_diff_us(rise_time, fall_time);
        float distance_m = (difference_us / 1000000.0) * (340.29) / 2.0;
        float distance_cm = distance_m * 100;
        xQueueSendFromISR(xQueueDistance, &distance_cm, &xHigherPriorityTaskWoken);
    }
}

void trigger_task(void *params) {
    while (1) {
        xSemaphoreGive(xSemaphoreTrigger);
        vTaskDelay(pdMS_TO_TICKS(60)); // Delay to avoid continuous trigger
    }
}

void echo_task(void *params) {
    while (1) {
        if (xSemaphoreTake(xSemaphoreTrigger, portMAX_DELAY) == pdTRUE) {
            gpio_put(TRIGGER_PIN, 1);
            busy_wait_us_32(10); // Wait for 10 microseconds
            gpio_put(TRIGGER_PIN, 0);
        }
    }
}

void oled_task(void *params) {
    float distance;
    oled_init();
    while (1) {
        if(xSemaphoreTake(xSemaphoreTrigger,portMAX_DELAY) == pdTRUE){
            
        if (xQueueReceive(xQueueDistance, &distance, 0) == pdTRUE) {
            gfx_clear_buffer(&disp);
            if (distance < 0 || distance >= 1199.99) {
                gfx_draw_string(&disp, 0, 0, 1, "Erro: fora do alcance");
            } else {
                gfx_draw_string(&disp, 0, 0, 1, "Distancia:");
                char distance_str[16];
                snprintf(distance_str, sizeof(distance_str), "%.2f cm", distance);
                gfx_draw_string(&disp, 0, 10, 1, distance_str);
                int bar_length = (int)(distance * 0.86);
                if (bar_length > 128) bar_length = 128; // Limita o comprimento da barra ao máximo da tela
                gfx_draw_line(&disp, 0, 20, bar_length, 20);
            }

            // Mostra as atualizações no display
            gfx_show(&disp);
        }
        else{
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "Erro: sensor");
            gfx_show(&disp);
            // vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    }

}

int main() {
    stdio_init_all();

    // Initialize GPIO for trigger and echo
    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
    gpio_put(TRIGGER_PIN, 0);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &ECHO_PIN_callback);

    ssd1306_init();
    gfx_init(&disp, 128, 32);

    xSemaphoreTrigger = xSemaphoreCreateBinary();
    xQueueDistance = xQueueCreate(5, sizeof(float));

    xTaskCreate(trigger_task, "Trigger Task", 256, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo Task", 256, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED Task", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) {}
}
