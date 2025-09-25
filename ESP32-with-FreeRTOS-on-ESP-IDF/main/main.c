#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"

#include "esp_log.h"
#include "hal/gpio_types.h"
#include "portmacro.h"

static const char* TAG = "APP";

static TaskHandle_t acyclic_task_handler = NULL;

#define STUDENT_ID	"2213140"
#define CYCLIC_TASK_DELAY_MS	1000
#define CYCLIC_TASK_PRIORITY	1

#define BUTTON_GPIO	GPIO_NUM_25
#define ACYCLIC_TASK_PRIORITY	3
#define DEBOUNCE_DELAY_MS	10

static void cyclic_task(void* pvParameters){
	for(;;){
		ESP_LOGI(TAG, "STUDENT ID: %s", STUDENT_ID);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	
	vTaskDelete(NULL);
}

static void acyclic_task(void* pvParameters){
	for(;;){
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		
		int level0 = gpio_get_level(BUTTON_GPIO);
		vTaskDelay(DEBOUNCE_DELAY_MS/ portTICK_PERIOD_MS);
		int level1 = gpio_get_level(BUTTON_GPIO);
		
		if(level0 == 0 && level0 == level1){
			ESP_LOGI(TAG, "ESP32");
			
			while(gpio_get_level(BUTTON_GPIO) == 0){
				vTaskDelay(20 / portTICK_PERIOD_MS);
			}
		}
		
	}
	
	vTaskDelete(NULL);
}

static void gpio_isr_handler(void* arg){
	BaseType_t higher_priority_task_woken = pdFALSE;
	
	vTaskNotifyGiveFromISR(acyclic_task_handler, &higher_priority_task_woken);
	if (higher_priority_task_woken) {
		portYIELD_FROM_ISR();
	}
}

void app_main(void)
{
	gpio_config_t gpio_config_info = {
		.pin_bit_mask = (1ULL << BUTTON_GPIO),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_NEGEDGE,
	};
	gpio_config(&gpio_config_info);
	
	esp_err_t esp_err_return;
	esp_err_return = gpio_install_isr_service(0);
	if(esp_err_return != ESP_OK  && esp_err_return != ESP_ERR_INVALID_STATE	){
		ESP_LOGE(TAG, "gpio_install_isr_service failed: %d", esp_err_return);
		return;
	}
	
	BaseType_t basetype_return;
	basetype_return = xTaskCreate(acyclic_task, "acyclic task", 4096, NULL, ACYCLIC_TASK_PRIORITY, &acyclic_task_handler);
	if(basetype_return != pdPASS){
		ESP_LOGE(TAG, "acyclic task create failed: %d", basetype_return);
		return;
	}
	
	basetype_return = gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, NULL);
	if(basetype_return != ESP_OK){
		ESP_LOGE(TAG, "gpio_isr_handler_add failed: %d", basetype_return);
		return;
	}
	
	basetype_return = xTaskCreate(cyclic_task, "cyclic task", 2048, NULL, CYCLIC_TASK_PRIORITY, NULL);
	if(basetype_return != pdPASS){
		ESP_LOGE(TAG, "cyclic task create failed: %d", basetype_return);
		return;
	}
	
	ESP_LOGI(TAG, "Set up task successfully!");
	
	// Finish Lab02 Esp32
}
