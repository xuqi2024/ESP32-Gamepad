/**
 * @file vibration.c
 * @brief 震动反馈控制实现
 */

#include "vibration.h"
#include "bluetooth_hid.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <string.h>

static const char *TAG = "VIBRATION";

// 静态变量
static vibration_status_t current_status = {0};
static bool vibration_enabled = true;
static bool initialized = false;
static TimerHandle_t vibration_timer = NULL;

/**
 * @brief 发送震动命令到手柄
 */
static esp_err_t send_vibration_command(uint8_t left_intensity, uint8_t right_intensity)
{
    if (!bluetooth_hid_is_connected()) {
        ESP_LOGW(TAG, "Bluetooth HID not connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 构造震动报告数据
    // 这里的数据格式取决于具体的手柄类型
    // 通用格式：报告ID + 左马达强度 + 右马达强度
    uint8_t vibration_data[4] = {
        0x01,              // 报告ID（震动报告）
        0x00,              // 保留字节
        left_intensity,    // 左马达强度
        right_intensity    // 右马达强度
    };
    
    hid_output_report_t report = {
        .report_id = 0x01,
        .data = vibration_data,
        .len = sizeof(vibration_data)
    };
    
    hid_device_info_t device_info;
    esp_err_t ret = bluetooth_hid_get_connected_device(&device_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get connected device: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = bluetooth_hid_send_output_report(device_info.dev_handle, &report);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send vibration command: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "Vibration command sent: left=%d, right=%d", left_intensity, right_intensity);
    return ESP_OK;
}

/**
 * @brief 震动定时器回调函数
 */
static void vibration_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGD(TAG, "Vibration timer expired");
    
    // 停止震动
    send_vibration_command(0, 0);
    
    // 更新状态
    current_status.active = false;
    current_status.remaining_time = 0;
    
    // 停止定时器
    if (vibration_timer != NULL) {
        xTimerStop(vibration_timer, 0);
    }
}

/**
 * @brief 处理脉冲模式震动
 */
static esp_err_t handle_pulse_mode(const vibration_params_t *params)
{
    ESP_LOGD(TAG, "Starting pulse mode vibration");
    
    // TODO: 实现脉冲模式
    // 这里需要创建一个任务来处理脉冲序列
    // 暂时使用简单的连续模式
    
    esp_err_t ret = send_vibration_command(params->left_intensity, params->right_intensity);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 启动定时器
    if (vibration_timer != NULL) {
        xTimerChangePeriod(vibration_timer, pdMS_TO_TICKS(params->duration_ms), 0);
        xTimerStart(vibration_timer, 0);
    }
    
    return ESP_OK;
}

/**
 * @brief 处理连续模式震动
 */
static esp_err_t handle_continuous_mode(const vibration_params_t *params)
{
    ESP_LOGD(TAG, "Starting continuous mode vibration");
    
    esp_err_t ret = send_vibration_command(params->left_intensity, params->right_intensity);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 启动定时器
    if (vibration_timer != NULL) {
        xTimerChangePeriod(vibration_timer, pdMS_TO_TICKS(params->duration_ms), 0);
        xTimerStart(vibration_timer, 0);
    }
    
    return ESP_OK;
}

esp_err_t vibration_init(void)
{
    ESP_LOGI(TAG, "Initializing vibration control...");
    
    if (initialized) {
        ESP_LOGW(TAG, "Vibration already initialized");
        return ESP_OK;
    }
    
    // 初始化状态
    memset(&current_status, 0, sizeof(current_status));
    vibration_enabled = true;
    
    // 创建定时器
    vibration_timer = xTimerCreate("VibrationTimer", 
                                   pdMS_TO_TICKS(100),  // 默认100ms
                                   pdFALSE,             // 单次触发
                                   (void *)0,           // 定时器ID
                                   vibration_timer_callback);
    
    if (vibration_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create vibration timer");
        return ESP_ERR_NO_MEM;
    }
    
    initialized = true;
    ESP_LOGI(TAG, "Vibration control initialized successfully");
    
    return ESP_OK;
}

esp_err_t vibration_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing vibration control...");
    
    if (!initialized) {
        ESP_LOGW(TAG, "Vibration not initialized");
        return ESP_OK;
    }
    
    // 停止当前震动
    vibration_stop();
    
    // 删除定时器
    if (vibration_timer != NULL) {
        xTimerDelete(vibration_timer, portMAX_DELAY);
        vibration_timer = NULL;
    }
    
    initialized = false;
    ESP_LOGI(TAG, "Vibration control deinitialized");
    
    return ESP_OK;
}

esp_err_t vibration_start(const vibration_params_t *params)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Vibration not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!params) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!vibration_enabled) {
        ESP_LOGD(TAG, "Vibration disabled");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting vibration: left=%d, right=%d, duration=%lums, mode=%d", 
             params->left_intensity, params->right_intensity, params->duration_ms, params->mode);
    
    // 停止当前震动
    vibration_stop();
    
    // 保存参数
    memcpy(&current_status.params, params, sizeof(vibration_params_t));
    current_status.active = true;
    current_status.start_time = esp_timer_get_time() / 1000; // 转换为毫秒
    current_status.remaining_time = params->duration_ms;
    
    // 根据模式处理
    esp_err_t ret = ESP_OK;
    switch (params->mode) {
        case VIBRATION_MODE_PULSE:
            ret = handle_pulse_mode(params);
            break;
            
        case VIBRATION_MODE_CONTINUOUS:
            ret = handle_continuous_mode(params);
            break;
            
        case VIBRATION_MODE_PATTERN:
            // TODO: 实现模式模式
            ESP_LOGW(TAG, "Pattern mode not implemented yet");
            ret = handle_continuous_mode(params);
            break;
            
        case VIBRATION_MODE_FEEDBACK:
            ret = handle_continuous_mode(params);
            break;
            
        default:
            ESP_LOGE(TAG, "Invalid vibration mode: %d", params->mode);
            ret = ESP_ERR_INVALID_ARG;
            break;
    }
    
    if (ret != ESP_OK) {
        current_status.active = false;
        current_status.remaining_time = 0;
    }
    
    return ret;
}

esp_err_t vibration_stop(void)
{
    ESP_LOGD(TAG, "Stopping vibration");
    
    if (!initialized) {
        ESP_LOGE(TAG, "Vibration not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 停止定时器
    if (vibration_timer != NULL) {
        xTimerStop(vibration_timer, 0);
    }
    
    // 发送停止命令
    esp_err_t ret = send_vibration_command(0, 0);
    
    // 更新状态
    current_status.active = false;
    current_status.remaining_time = 0;
    
    return ret;
}

esp_err_t vibration_set_pattern(const vibration_pattern_t *pattern)
{
    ESP_LOGW(TAG, "Pattern mode not implemented yet");
    // TODO: 实现模式震动
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t vibration_quick_pulse(uint8_t intensity, uint32_t duration_ms)
{
    vibration_params_t params = {
        .left_intensity = intensity,
        .right_intensity = intensity,
        .duration_ms = duration_ms,
        .mode = VIBRATION_MODE_PULSE,
        .pulse_count = 1,
        .pulse_interval_ms = 0
    };
    
    return vibration_start(&params);
}

esp_err_t vibration_dual_motor(uint8_t left_intensity, uint8_t right_intensity, uint32_t duration_ms)
{
    vibration_params_t params = {
        .left_intensity = left_intensity,
        .right_intensity = right_intensity,
        .duration_ms = duration_ms,
        .mode = VIBRATION_MODE_CONTINUOUS,
        .pulse_count = 0,
        .pulse_interval_ms = 0
    };
    
    return vibration_start(&params);
}

esp_err_t vibration_get_status(vibration_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!initialized) {
        ESP_LOGE(TAG, "Vibration not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 更新剩余时间
    if (current_status.active) {
        uint32_t elapsed = (esp_timer_get_time() / 1000) - current_status.start_time;
        if (elapsed >= current_status.params.duration_ms) {
            current_status.remaining_time = 0;
        } else {
            current_status.remaining_time = current_status.params.duration_ms - elapsed;
        }
    }
    
    memcpy(status, &current_status, sizeof(vibration_status_t));
    return ESP_OK;
}

bool vibration_is_active(void)
{
    return current_status.active;
}

esp_err_t vibration_set_enable(bool enable)
{
    ESP_LOGI(TAG, "Setting vibration enable: %s", enable ? "ON" : "OFF");
    
    if (!enable && vibration_enabled) {
        // 禁用震动时，停止当前震动
        vibration_stop();
    }
    
    vibration_enabled = enable;
    return ESP_OK;
}

bool vibration_is_enabled(void)
{
    return vibration_enabled;
}
