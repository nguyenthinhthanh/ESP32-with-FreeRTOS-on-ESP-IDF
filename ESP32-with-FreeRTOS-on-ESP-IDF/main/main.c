#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "preempt_slice";

const TickType_t run_ms = 2000; // chạy trong 2000 ms (2s)

static TickType_t global_start_tick = 0;

static void do_work_us(uint32_t work_us)
{
    int64_t t0 = esp_timer_get_time(); // microseconds
    while ((esp_timer_get_time() - t0) < (int64_t)work_us) {
        asm volatile("nop");
    }
}

/* Task A (khoảng work mong muốn 20 ms) */
void vTaskFunctionA(void *pvParameters)
{
    const char *pcTaskName = (const char*)pvParameters;
    const uint32_t intended_us = 20000; // 20 ms

    TickType_t now;
    while ((now = xTaskGetTickCount()) - global_start_tick < pdMS_TO_TICKS(run_ms)) {
        /* before */
        TickType_t tick_before = xTaskGetTickCount();
        int64_t us_before = esp_timer_get_time();

        ESP_LOGI(TAG, "[Tick %u] %s begin work (intended %u us) (global elapsed %u ms)",
                 (unsigned)tick_before,
                 pcTaskName,
                 (unsigned)intended_us,
                 (unsigned)((tick_before - global_start_tick) * portTICK_PERIOD_MS));

        do_work_us(intended_us);

        /* after */
        TickType_t tick_after = xTaskGetTickCount();
        int64_t us_after = esp_timer_get_time();
        uint32_t actual_us = (uint32_t)(us_after - us_before);
        uint32_t tick_diff_ms = (uint32_t)((tick_after - tick_before) * portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "[Tick %u -> %u | Elapsed %u ms -> %u ms] %s end work: intended=%u us, actual=%u us, tick_diff=%u ms",
                 (unsigned)tick_before,
                 (unsigned)tick_after,
                 (unsigned)((tick_before - global_start_tick) * portTICK_PERIOD_MS),
                 (unsigned)((tick_after  - global_start_tick) * portTICK_PERIOD_MS),
                 pcTaskName,
                 (unsigned)intended_us,
                 (unsigned)actual_us,
                 (unsigned)tick_diff_ms);

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "%s stopping after %u ms", pcTaskName, (unsigned)run_ms);
    vTaskDelete(NULL);
}

/* Task B (khoảng work mong muốn 30 ms) */
void vTaskFunctionB(void *pvParameters)
{
    const char *pcTaskName = (const char*)pvParameters;
    const uint32_t intended_us = 30000; // 30 ms

    TickType_t now;
    while ((now = xTaskGetTickCount()) - global_start_tick < pdMS_TO_TICKS(run_ms)) {
        TickType_t tick_before = xTaskGetTickCount();
        int64_t us_before = esp_timer_get_time();

        ESP_LOGI(TAG, "[Tick %u] %s begin work (intended %u us)",
                 (unsigned)tick_before, pcTaskName, (unsigned)intended_us);

        do_work_us(intended_us);

        TickType_t tick_after = xTaskGetTickCount();
        int64_t us_after = esp_timer_get_time();
        uint32_t actual_us = (uint32_t)(us_after - us_before);
        uint32_t tick_diff_ms = (uint32_t)((tick_after - tick_before) * portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "[Tick %u -> %u] %s end work: intended=%u us, actual=%u us, tick_diff=%u ms",
                 (unsigned)tick_before, (unsigned)tick_after, pcTaskName,
                 (unsigned)intended_us, (unsigned)actual_us, (unsigned)tick_diff_ms);

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "%s stopping after %u ms", pcTaskName, (unsigned)run_ms);
    vTaskDelete(NULL);
}

/* High task (khoảng work mong muốn 5 ms), priority cao */
void vTaskHigh(void *pv)
{
    const uint32_t intended_us = 5000; // 5 ms

    TickType_t now;
    while ((now = xTaskGetTickCount()) - global_start_tick < pdMS_TO_TICKS(run_ms)) {
        TickType_t tick_before = xTaskGetTickCount();
        int64_t us_before = esp_timer_get_time();

        ESP_LOGI(TAG, "[Tick %u] High begin work (intended %u us)", (unsigned)tick_before, (unsigned)intended_us);

        do_work_us(intended_us);

        TickType_t tick_after = xTaskGetTickCount();
        int64_t us_after = esp_timer_get_time();
        uint32_t actual_us = (uint32_t)(us_after - us_before);
        uint32_t tick_diff_ms = (uint32_t)((tick_after - tick_before) * portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "[Tick %u -> %u] High end work: intended=%u us, actual=%u us, tick_diff=%u ms",
                 (unsigned)tick_before, (unsigned)tick_after,
                 (unsigned)intended_us, (unsigned)actual_us, (unsigned)tick_diff_ms);

        vTaskDelay(pdMS_TO_TICKS(250));
    }

    ESP_LOGI(TAG, "High task stopping after %u ms", (unsigned)run_ms);
    vTaskDelete(NULL);
}

void app_main(void)
{
    float tick_ms = 1000.0 / configTICK_RATE_HZ;
    ESP_LOGI("TickInfo", "configTICK_RATE_HZ = %d Hz -> 1 tick = %.2f ms",
             configTICK_RATE_HZ, tick_ms);

    global_start_tick = xTaskGetTickCount();

    xTaskCreatePinnedToCore(vTaskFunctionA,  "TaskA", 4096, "Task A", 1, NULL, 0);
    xTaskCreatePinnedToCore(vTaskFunctionB, "TaskB", 4096, "Task B", 1, NULL, 0);
    xTaskCreatePinnedToCore(vTaskHigh,     "High",  4096, NULL,     2, NULL, 0);
   
}
