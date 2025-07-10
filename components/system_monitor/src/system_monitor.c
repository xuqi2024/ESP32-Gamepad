/**
 * @file system_monitor.c
 * @brief ESP32-Gamepad 系统监控模块实现
 * 
 * 提供系统状态监控、性能分析、内存管理、电源管理等功能
 * 
 * @author ESP32-Gamepad Team
 * @date 2024
 */

#include "system_monitor.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "esp_pm.h"
#include <string.h>
#include <math.h>

static const char *TAG = "SYS_MONITOR";

/* 系统监控状态 */
static bool is_initialized = false;
static system_state_t current_system_state = SYSTEM_STATE_INIT;
static connection_state_t current_connection_state = CONNECTION_STATE_DISCONNECTED;
static power_state_t current_power_state = POWER_STATE_NORMAL;

/* 统计数据 */
static system_resources_t resources = {0};
static connection_stats_t conn_stats = {0};
static performance_stats_t perf_stats = {0};

/* 回调函数 */
static system_state_callback_t system_state_cb = NULL;
static connection_state_callback_t connection_state_cb = NULL;
static power_state_callback_t power_state_cb = NULL;
static error_callback_t error_cb = NULL;

/* 监控任务句柄 */
static TaskHandle_t monitor_task_handle = NULL;
static QueueHandle_t error_queue = NULL;

/* 定时器 */
static esp_timer_handle_t monitor_timer = NULL;

/* 内部函数声明 */
static void monitor_task(void *pvParameters);
static void monitor_timer_callback(void *arg);
static void update_system_resources(void);
static void update_performance_stats(void);
static void check_power_state(void);

/**
 * @brief 初始化系统监控模块
 */
esp_err_t system_monitor_init(void)
{
    if (is_initialized) {
        ESP_LOGW(TAG, "System monitor already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing system monitor...");

    // 创建错误队列
    error_queue = xQueueCreate(10, sizeof(error_info_t));
    if (error_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create error queue");
        return ESP_ERR_NO_MEM;
    }

    // 创建监控任务
    BaseType_t ret = xTaskCreate(
        monitor_task,
        "sys_monitor",
        4096,
        NULL,
        5,
        &monitor_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create monitor task");
        vQueueDelete(error_queue);
        return ESP_ERR_NO_MEM;
    }

    // 创建定时器（每秒更新一次）
    esp_timer_create_args_t timer_args = {
        .callback = monitor_timer_callback,
        .arg = NULL,
        .name = "monitor_timer"
    };

    esp_err_t err = esp_timer_create(&timer_args, &monitor_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create monitor timer: %s", esp_err_to_name(err));
        vTaskDelete(monitor_task_handle);
        vQueueDelete(error_queue);
        return err;
    }

    // 启动定时器
    err = esp_timer_start_periodic(monitor_timer, 1000000); // 1秒
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start monitor timer: %s", esp_err_to_name(err));
        esp_timer_delete(monitor_timer);
        vTaskDelete(monitor_task_handle);
        vQueueDelete(error_queue);
        return err;
    }

    // 初始化统计数据
    memset(&resources, 0, sizeof(resources));
    memset(&conn_stats, 0, sizeof(conn_stats));
    memset(&perf_stats, 0, sizeof(perf_stats));

    is_initialized = true;
    
    // 设置初始状态
    system_monitor_set_state(SYSTEM_STATE_IDLE);
    
    ESP_LOGI(TAG, "System monitor initialized successfully");
    return ESP_OK;
}

/**
 * @brief 反初始化系统监控模块
 */
esp_err_t system_monitor_deinit(void)
{
    if (!is_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing system monitor...");

    // 停止并删除定时器
    if (monitor_timer) {
        esp_timer_stop(monitor_timer);
        esp_timer_delete(monitor_timer);
        monitor_timer = NULL;
    }

    // 删除任务
    if (monitor_task_handle) {
        vTaskDelete(monitor_task_handle);
        monitor_task_handle = NULL;
    }

    // 删除队列
    if (error_queue) {
        vQueueDelete(error_queue);
        error_queue = NULL;
    }

    // 清除回调函数
    system_state_cb = NULL;
    connection_state_cb = NULL;
    power_state_cb = NULL;
    error_cb = NULL;

    is_initialized = false;
    ESP_LOGI(TAG, "System monitor deinitialized");
    return ESP_OK;
}

/**
 * @brief 设置系统状态
 */
esp_err_t system_monitor_set_state(system_state_t state)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (state >= SYSTEM_STATE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    system_state_t old_state = current_system_state;
    current_system_state = state;

    ESP_LOGI(TAG, "System state changed: %d -> %d", old_state, state);

    // 调用回调函数
    if (system_state_cb && old_state != state) {
        system_state_cb(old_state, state);
    }

    return ESP_OK;
}

/**
 * @brief 获取当前系统状态
 */
system_state_t system_monitor_get_system_state(void)
{
    return current_system_state;
}

/**
 * @brief 设置连接状态
 */
esp_err_t system_monitor_set_connection_state(connection_state_t state)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (state >= CONNECTION_STATE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    connection_state_t old_state = current_connection_state;
    current_connection_state = state;

    ESP_LOGD(TAG, "Connection state changed: %d -> %d", old_state, state);

    // 更新连接统计
    switch (state) {
        case CONNECTION_STATE_SCANNING:
            conn_stats.connection_attempts++;
            break;
        case CONNECTION_STATE_CONNECTED:
            conn_stats.successful_connections++;
            break;
        case CONNECTION_STATE_FAILED:
            conn_stats.connection_failures++;
            break;
        case CONNECTION_STATE_DISCONNECTED:
            if (old_state == CONNECTION_STATE_CONNECTED) {
                conn_stats.disconnections++;
            }
            break;
        default:
            break;
    }

    // 计算连接成功率
    if (conn_stats.connection_attempts > 0) {
        conn_stats.connection_success_rate = 
            (float)conn_stats.successful_connections / conn_stats.connection_attempts * 100.0f;
    }

    // 调用回调函数
    if (connection_state_cb && old_state != state) {
        connection_state_cb(state);
    }

    return ESP_OK;
}

/**
 * @brief 获取当前连接状态
 */
connection_state_t system_monitor_get_connection_state(void)
{
    return current_connection_state;
}

/**
 * @brief 获取系统资源信息
 */
esp_err_t system_monitor_get_resources(system_resources_t *res)
{
    if (!is_initialized || res == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(res, &resources, sizeof(system_resources_t));
    return ESP_OK;
}

/**
 * @brief 获取连接统计信息
 */
esp_err_t system_monitor_get_connection_stats(connection_stats_t *stats)
{
    if (!is_initialized || stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(stats, &conn_stats, sizeof(connection_stats_t));
    return ESP_OK;
}

/**
 * @brief 获取性能统计信息
 */
esp_err_t system_monitor_get_performance_stats(performance_stats_t *stats)
{
    if (!is_initialized || stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(stats, &perf_stats, sizeof(performance_stats_t));
    return ESP_OK;
}

/**
 * @brief 记录错误信息
 */
esp_err_t system_monitor_log_error(uint16_t error_code, const char *message, uint8_t severity)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    error_info_t error = {
        .timestamp = esp_timer_get_time() / 1000000, // 转换为秒
        .error_code = error_code,
        .severity = severity
    };

    // 复制错误消息
    if (message) {
        strncpy(error.error_message, message, sizeof(error.error_message) - 1);
        error.error_message[sizeof(error.error_message) - 1] = '\0';
    } else {
        error.error_message[0] = '\0';
    }

    // 发送到错误队列
    if (xQueueSend(error_queue, &error, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Error queue full, dropping error");
    }

    ESP_LOGE(TAG, "Error logged: code=%d, msg=%s, severity=%d", 
             error_code, error.error_message, severity);

    return ESP_OK;
}

/**
 * @brief 注册系统状态回调函数
 */
esp_err_t system_monitor_register_state_callback(system_state_callback_t callback)
{
    system_state_cb = callback;
    return ESP_OK;
}

/**
 * @brief 注册连接状态回调函数
 */
esp_err_t system_monitor_register_connection_callback(connection_state_callback_t callback)
{
    connection_state_cb = callback;
    return ESP_OK;
}

/**
 * @brief 注册电源状态回调函数
 */
esp_err_t system_monitor_register_power_callback(power_state_callback_t callback)
{
    power_state_cb = callback;
    return ESP_OK;
}

/**
 * @brief 注册错误回调函数
 */
esp_err_t system_monitor_register_error_callback(error_callback_t callback)
{
    error_cb = callback;
    return ESP_OK;
}

/**
 * @brief 监控任务
 */
static void monitor_task(void *pvParameters)
{
    error_info_t error;
    
    ESP_LOGI(TAG, "Monitor task started");

    while (1) {
        // 处理错误队列
        while (xQueueReceive(error_queue, &error, 0) == pdTRUE) {
            if (error_cb) {
                error_cb(&error);
            }
        }

        // 检查电源状态
        check_power_state();

        // 等待一段时间
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief 定时器回调函数
 */
static void monitor_timer_callback(void *arg)
{
    update_system_resources();
    update_performance_stats();
}

/**
 * @brief 更新系统资源信息
 */
static void update_system_resources(void)
{
    // 更新堆内存信息
    resources.free_heap = esp_get_free_heap_size();
    resources.min_free_heap = esp_get_minimum_free_heap_size();
    resources.total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    
    if (resources.total_heap > 0) {
        resources.heap_usage = ((resources.total_heap - resources.free_heap) * 100) / resources.total_heap;
    }

    // 更新运行时间
    resources.uptime = esp_timer_get_time() / 1000000; // 转换为秒

    // 更新任务数量
    resources.task_count = uxTaskGetNumberOfTasks();

    // 更新栈高水位（主任务的栈）
    if (monitor_task_handle) {
        resources.stack_high_water = uxTaskGetStackHighWaterMark(monitor_task_handle);
    }

    // 简单的CPU使用率估算（基于空闲任务的运行时间）
    static uint32_t last_total_time = 0;
    
    uint32_t current_time = esp_timer_get_time() / 1000; // 毫秒
    uint32_t time_diff = current_time - last_total_time;
    
    if (time_diff > 1000) { // 每秒更新一次
        // 这里应该获取空闲任务的运行时间，但由于API限制，使用简化计算
        resources.cpu_usage = (resources.heap_usage > 80) ? 80 : 20; // 简化的CPU使用率
        last_total_time = current_time;
    }
}

/**
 * @brief 更新性能统计信息
 */
static void update_performance_stats(void)
{
    // 这里应该从其他模块获取实际的性能数据
    // 目前使用模拟数据
    perf_stats.input_processing_time = 50 + (esp_random() % 100); // 50-150us
    perf_stats.output_processing_time = 30 + (esp_random() % 50); // 30-80us
    perf_stats.bluetooth_latency = 10 + (esp_random() % 20); // 10-30ms
    perf_stats.control_latency = 5 + (esp_random() % 10); // 5-15ms
    perf_stats.input_frequency = 100; // 100Hz
    perf_stats.output_frequency = 50; // 50Hz
    perf_stats.system_load = resources.cpu_usage;
}

/**
 * @brief 检查电源状态
 */
static void check_power_state(void)
{
    // 这里应该读取实际的电池电压
    // 目前使用模拟数据
    static float battery_voltage = 3.7f; // 模拟电池电压
    
    power_state_t new_state = POWER_STATE_NORMAL;
    
    if (battery_voltage < 3.0f) {
        new_state = POWER_STATE_CRITICAL;
    } else if (battery_voltage < 3.3f) {
        new_state = POWER_STATE_LOW_BATTERY;
    } else if (battery_voltage > 4.0f) {
        new_state = POWER_STATE_CHARGING;
    }
    
    if (new_state != current_power_state) {
        current_power_state = new_state;
        if (power_state_cb) {
            power_state_cb(new_state, battery_voltage);
        }
    }
}

/**
 * @brief 更新数据包统计
 */
esp_err_t system_monitor_update_data_stats(bool sent, uint32_t count)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (sent) {
        conn_stats.data_packets_sent += count;
    } else {
        conn_stats.data_packets_received += count;
    }

    return ESP_OK;
}

/**
 * @brief 更新错误计数
 */
esp_err_t system_monitor_increment_error_count(void)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    conn_stats.error_count++;
    return ESP_OK;
}

/**
 * @brief 重置连接统计
 */
esp_err_t system_monitor_reset_connection_stats(void)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&conn_stats, 0, sizeof(conn_stats));
    ESP_LOGI(TAG, "Connection stats reset");
    return ESP_OK;
}

/**
 * @brief 重置性能统计
 */
esp_err_t system_monitor_reset_performance_stats(void)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&perf_stats, 0, sizeof(perf_stats));
    ESP_LOGI(TAG, "Performance stats reset");
    return ESP_OK;
}
