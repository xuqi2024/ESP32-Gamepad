/**
 * @file system_monitor.h
 * @brief ESP32-Gamepad 系统监控模块
 * 
 * 提供系统状态监控、性能分析、内存管理、电源管理等功能
 * 
 * @author ESP32-Gamepad Team
 * @date 2024
 */

#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 系统状态枚举 */
typedef enum {
    SYSTEM_STATE_INIT = 0,      /**< 系统初始化 */
    SYSTEM_STATE_IDLE,          /**< 系统空闲 */
    SYSTEM_STATE_CONNECTING,    /**< 连接中 */
    SYSTEM_STATE_CONNECTED,     /**< 已连接 */
    SYSTEM_STATE_CONTROLLING,   /**< 控制中 */
    SYSTEM_STATE_ERROR,         /**< 错误状态 */
    SYSTEM_STATE_SHUTDOWN,      /**< 关机状态 */
    SYSTEM_STATE_MAX
} system_state_t;

/* 连接状态枚举 */
typedef enum {
    CONNECTION_STATE_DISCONNECTED = 0,
    CONNECTION_STATE_SCANNING,
    CONNECTION_STATE_PAIRING,
    CONNECTION_STATE_CONNECTED,
    CONNECTION_STATE_FAILED,
    CONNECTION_STATE_MAX
} connection_state_t;

/* 电源状态枚举 */
typedef enum {
    POWER_STATE_NORMAL = 0,
    POWER_STATE_LOW_BATTERY,
    POWER_STATE_CRITICAL,
    POWER_STATE_CHARGING,
    POWER_STATE_MAX
} power_state_t;

/* 系统资源信息 */
typedef struct {
    uint32_t free_heap;         /**< 空闲堆内存(字节) */
    uint32_t min_free_heap;     /**< 最小空闲堆内存(字节) */
    uint32_t total_heap;        /**< 总堆内存(字节) */
    uint8_t heap_usage;         /**< 堆内存使用率(%) */
    uint8_t cpu_usage;          /**< CPU使用率(%) */
    uint32_t uptime;            /**< 系统运行时间(秒) */
    uint16_t task_count;        /**< 任务数量 */
    uint32_t stack_high_water;  /**< 栈高水位标记 */
} system_resources_t;

/* 连接统计信息 */
typedef struct {
    uint32_t connection_attempts;   /**< 连接尝试次数 */
    uint32_t successful_connections; /**< 成功连接次数 */
    uint32_t connection_failures;   /**< 连接失败次数 */
    uint32_t disconnections;        /**< 断开连接次数 */
    uint32_t data_packets_sent;     /**< 发送数据包数 */
    uint32_t data_packets_received; /**< 接收数据包数 */
    uint32_t error_count;           /**< 错误计数 */
    float connection_success_rate;  /**< 连接成功率(%) */
    uint32_t avg_connection_time;   /**< 平均连接时间(ms) */
} connection_stats_t;

/* 性能统计信息 */
typedef struct {
    uint32_t input_processing_time; /**< 输入处理时间(us) */
    uint32_t output_processing_time; /**< 输出处理时间(us) */
    uint32_t bluetooth_latency;     /**< 蓝牙延迟(ms) */
    uint32_t control_latency;       /**< 控制延迟(ms) */
    uint16_t input_frequency;       /**< 输入频率(Hz) */
    uint16_t output_frequency;      /**< 输出频率(Hz) */
    uint8_t system_load;            /**< 系统负载(%) */
} performance_stats_t;

/* 错误信息结构 */
typedef struct {
    uint32_t timestamp;         /**< 错误时间戳 */
    uint16_t error_code;        /**< 错误代码 */
    char error_message[64];     /**< 错误消息 */
    uint8_t severity;           /**< 严重程度(0-255) */
} error_info_t;

/* 系统监控回调函数类型 */
typedef void (*system_state_callback_t)(system_state_t old_state, system_state_t new_state);
typedef void (*connection_state_callback_t)(connection_state_t state);
typedef void (*power_state_callback_t)(power_state_t state, float voltage);
typedef void (*error_callback_t)(const error_info_t *error);
typedef void (*resource_alert_callback_t)(const char *resource, uint8_t usage);

/**
 * @brief 初始化系统监控
 * 
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_init(void);

/**
 * @brief 反初始化系统监控
 * 
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_deinit(void);

/**
 * @brief 启动系统监控
 * 
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_start(void);

/**
 * @brief 停止系统监控
 * 
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_stop(void);

/**
 * @brief 获取当前系统状态
 * 
 * @return system_state_t 当前系统状态
 */
system_state_t system_monitor_get_state(void);

/**
 * @brief 设置系统状态
 * 
 * @param state 新的系统状态
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_set_state(system_state_t state);

/**
 * @brief 获取连接状态
 * 
 * @return connection_state_t 当前连接状态
 */
connection_state_t system_monitor_get_connection_state(void);

/**
 * @brief 设置连接状态
 * 
 * @param state 新的连接状态
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_set_connection_state(connection_state_t state);

/**
 * @brief 获取电源状态
 * 
 * @return power_state_t 当前电源状态
 */
power_state_t system_monitor_get_power_state(void);

/**
 * @brief 获取系统资源信息
 * 
 * @param resources 输出的资源信息
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_get_resources(system_resources_t *resources);

/**
 * @brief 获取连接统计信息
 * 
 * @param stats 输出的统计信息
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_get_connection_stats(connection_stats_t *stats);

/**
 * @brief 获取性能统计信息
 * 
 * @param stats 输出的性能统计信息
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_get_performance_stats(performance_stats_t *stats);

/**
 * @brief 记录连接事件
 * 
 * @param success 连接是否成功
 * @param duration 连接耗时(ms)
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_record_connection_event(bool success, uint32_t duration);

/**
 * @brief 记录数据传输事件
 * 
 * @param sent 发送的数据包数
 * @param received 接收的数据包数
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_record_data_event(uint32_t sent, uint32_t received);

/**
 * @brief 记录性能指标
 * 
 * @param input_time 输入处理时间(us)
 * @param output_time 输出处理时间(us)
 * @param latency 延迟(ms)
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_record_performance(uint32_t input_time, uint32_t output_time, uint32_t latency);

/**
 * @brief 记录错误事件
 * 
 * @param error_code 错误代码
 * @param message 错误消息
 * @param severity 严重程度
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_record_error(uint16_t error_code, const char *message, uint8_t severity);

/**
 * @brief 获取电池电压
 * 
 * @return float 电池电压(V)
 */
float system_monitor_get_battery_voltage(void);

/**
 * @brief 获取芯片温度
 * 
 * @return float 芯片温度(°C)
 */
float system_monitor_get_chip_temperature(void);

/**
 * @brief 触发看门狗喂狗
 * 
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_feed_watchdog(void);

/**
 * @brief 检查系统健康状态
 * 
 * @return bool true健康，false异常
 */
bool system_monitor_is_healthy(void);

/**
 * @brief 触发软件重启
 * 
 * @param delay_ms 延迟时间(ms)
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_request_restart(uint32_t delay_ms);

/**
 * @brief 进入深度睡眠模式
 * 
 * @param duration_ms 睡眠时间(ms)，0表示无限期睡眠
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_enter_deep_sleep(uint32_t duration_ms);

/**
 * @brief 注册系统状态回调
 * 
 * @param callback 回调函数
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_register_state_callback(system_state_callback_t callback);

/**
 * @brief 注册连接状态回调
 * 
 * @param callback 回调函数
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_register_connection_callback(connection_state_callback_t callback);

/**
 * @brief 注册电源状态回调
 * 
 * @param callback 回调函数
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_register_power_callback(power_state_callback_t callback);

/**
 * @brief 注册错误回调
 * 
 * @param callback 回调函数
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_register_error_callback(error_callback_t callback);

/**
 * @brief 注册资源警报回调
 * 
 * @param callback 回调函数
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_register_resource_alert_callback(resource_alert_callback_t callback);

/**
 * @brief 清除所有统计信息
 * 
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_clear_stats(void);

/**
 * @brief 获取系统监控版本信息
 * 
 * @return const char* 版本字符串
 */
const char* system_monitor_get_version(void);

/**
 * @brief 导出系统监控报告
 * 
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t system_monitor_export_report(char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_MONITOR_H
