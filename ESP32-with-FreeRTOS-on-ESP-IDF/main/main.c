#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "preempt_slice";

const TickType_t run_ms = 2000; // chạy trong 2000 ms (2s)

static TickType_t global_start_tick = 0;

static TaskHandle_t last = NULL;

static void vMonitorCurrentTask(void *pv)
{
    TaskHandle_t cur = xTaskGetCurrentTaskHandle();
    if (cur != last) {
		TickType_t tick_before = xTaskGetTickCount();

		const char *name = pcTaskGetName(cur);
        ESP_LOGI(TAG, "[Tick %u] Current running: %s",
                 (unsigned)tick_before, name ? name : "<?>");
        last = cur;
    }
}

static void do_work_us(uint32_t work_us)
{
    int64_t t0 = esp_timer_get_time(); // microseconds
    while ((esp_timer_get_time() - t0) < (int64_t)work_us) {
		vMonitorCurrentTask(NULL);
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
                 
        vMonitorCurrentTask(NULL);

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
                 
        vMonitorCurrentTask(NULL);

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
        
        vMonitorCurrentTask(NULL);

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

/* ---------------- CPU utilization measurement via idle hook ---------------- */
/* Số lõi ESP32 (thường là 2) */
#define NUM_CORES 2

/* counters: phải volatile để đọc/ghi trong nhiều context */
static volatile uint32_t idle_count[NUM_CORES] = {0, 0};
/* lưu giá trị calibrated "max" (số lần hook chạy trong 1s khi không có tải) */
static uint32_t idle_count_max_per_sec[NUM_CORES] = {1, 1}; // init != 0 để tránh chia 0

/* FreeRTOS gọi hàm này nếu configUSE_IDLE_HOOK = 1 */
void vApplicationIdleHook(void)
{
    /* lấy lõi hiện tại */
    BaseType_t core = xPortGetCoreID();
    if (core < 0 || core >= NUM_CORES) core = 0;
    /* tăng bộ đếm một cách nhẹ (relaxed atomic) */
    __atomic_add_fetch(&idle_count[core], 1, __ATOMIC_RELAXED);
}

/* Task giám sát CPU utilization */
static void vCPUUtilTask(void *pv)
{
    const TickType_t interval = pdMS_TO_TICKS(1000); // 1s
    while (1) {
        vTaskDelay(interval);

        for (int c = 0; c < NUM_CORES; ++c) {
            /* atomically lấy và reset counter */
            uint32_t cnt = __atomic_exchange_n(&idle_count[c], 0u, __ATOMIC_RELAXED);

            /* sử dụng giá trị calibrated làm thang 100% */
            uint32_t max = idle_count_max_per_sec[c];
            float util = 0.0f;
            if (max > 0) {
                float frac_idle = (float)cnt / (float)max;
                if (frac_idle > 1.0f) frac_idle = 1.0f;
                util = 100.0f * (1.0f - frac_idle);
                if (util < 0.0f) util = 0.0f;
            }
            ESP_LOGI("CPU", "Core %d: idle_count=%u, calibrated_max=%u -> CPU util ~ %.1f%%",
                     c, (unsigned)cnt, (unsigned)max, util);
        }
    }
}

void app_main(void)
{
    float tick_ms = 1000.0 / configTICK_RATE_HZ;
    ESP_LOGI("TickInfo", "configTICK_RATE_HZ = %d Hz -> 1 tick = %.2f ms",
             configTICK_RATE_HZ, tick_ms);
             
     /* --- Calibration: đo 1 giây idle_count khi chưa tạo task nặng nào --- */
    for (int c = 0; c < NUM_CORES; ++c) {
        __atomic_store_n(&idle_count[c], 0u, __ATOMIC_RELAXED);
    }

    ESP_LOGI(TAG, "Calibrating idle hook for 1000 ms (do not create other heavy tasks yet)...");
    /* delay 1000 ms để idle hook chạy — app_main hiện đang chạy trong một task
       nên vTaskDelay sẽ nhường CPU, cho phép idle task chạy trên cả hai lõi. */
    vTaskDelay(pdMS_TO_TICKS(1000));

    for (int c = 0; c < NUM_CORES; ++c) {
        uint32_t measured = __atomic_exchange_n(&idle_count[c], 0u, __ATOMIC_RELAXED);
        /* nếu measured == 0 , đặt mặc định nhỏ để tránh chia 0 */
        if (measured == 0) measured = 1;
        idle_count_max_per_sec[c] = measured;
        ESP_LOGI(TAG, "Calibration: core %d max idle count per 1s = %u", c, (unsigned)measured);
    }

    global_start_tick = xTaskGetTickCount();
    
    xTaskCreatePinnedToCore(vCPUUtilTask, "CPUUtil", 2048, NULL, 3, NULL, 0);
    
    xTaskCreatePinnedToCore(vTaskFunctionA,  "TaskA", 4096, "Task A", 0, NULL, 0);
    xTaskCreatePinnedToCore(vTaskFunctionB, "TaskB", 4096, "Task B", 1, NULL, 0);
    xTaskCreatePinnedToCore(vTaskHigh,     "High",  4096, NULL,     2, NULL, 0);
   
}
