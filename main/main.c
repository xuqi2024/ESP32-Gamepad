/**
 * @file main.c
 * @brief ESP32 Gamepad Controller 主程序
 * @version 1.0.0
 * @date 2025-07-07
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "gamepad_controller.h"

static const char *TAG = "MAIN";

/**
 * @brief 系统初始化
 */
static void system_init(void)
{
    // 初始化NVS存储
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "NVS Flash initialized");
}

/**
 * @brief 蓝牙初始化
 */
static void bluetooth_init(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Starting Bluetooth initialization...");
    
    // ESP-IDF 5.4+ 蓝牙初始化：先尝试不释放内存
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth controller init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Bluetooth controller initialized");
    
    // 尝试启用BTDM模式（双模式）
    ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "BTDM mode failed: %s, trying Classic BT only", esp_err_to_name(ret));
        
        // 如果BTDM失败，尝试释放BLE内存后使用经典蓝牙
        esp_bt_controller_deinit();
        
        ret = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "Failed to release BLE memory: %s", esp_err_to_name(ret));
        }
        
        ret = esp_bt_controller_init(&bt_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Bluetooth controller re-init failed: %s", esp_err_to_name(ret));
            return;
        }
        
        ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Classic BT enable failed: %s", esp_err_to_name(ret));
            esp_bt_controller_deinit();
            return;
        }
    }
    ESP_LOGI(TAG, "Bluetooth controller enabled");
    
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return;
    }
    ESP_LOGI(TAG, "Bluedroid initialized");
    
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return;
    }
    
    ESP_LOGI(TAG, "Bluetooth initialized successfully");
}

/**
 * @brief 应用程序主函数
 */
void app_main(void)
{
    ESP_LOGI(TAG, "===============================================");
    ESP_LOGI(TAG, "ESP32 Gamepad Controller Starting...");
    ESP_LOGI(TAG, "Version: %s", "1.0.0");
    ESP_LOGI(TAG, "Build Date: %s %s", __DATE__, __TIME__);
    ESP_LOGI(TAG, "===============================================");
    
    // 系统初始化
    system_init();
    
    // 蓝牙初始化
    bluetooth_init();
    
    // 游戏手柄控制器初始化
    gamepad_controller_init();
    
    ESP_LOGI(TAG, "System initialization completed");
    ESP_LOGI(TAG, "System is ready for gamepad connection...");
    
    // 主循环
    while (1) {
        // 系统心跳
        ESP_LOGI(TAG, "System running... Free heap: %"PRIu32" bytes", esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10秒心跳
    }
}
