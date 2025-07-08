/**
 * @file config_manager.h
 * @brief ESP32-Gamepad 配置管理器
 * 
 * 提供配置文件读取、写入、缓存和参数验证功能
 * 
 * @author ESP32-Gamepad Team
 * @date 2024
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 配置类型定义 */
typedef enum {
    CONFIG_TYPE_GAMEPAD = 0,
    CONFIG_TYPE_CONTROL,
    CONFIG_TYPE_VIBRATION,
    CONFIG_TYPE_BLUETOOTH,
    CONFIG_TYPE_GPIO,
    CONFIG_TYPE_PWM,
    CONFIG_TYPE_SAFETY,
    CONFIG_TYPE_DEBUG,
    CONFIG_TYPE_MAX
} config_type_t;

/* 手柄类型枚举 */
typedef enum {
    CONTROLLER_TYPE_PS4 = 0,
    CONTROLLER_TYPE_XBOX,
    CONTROLLER_TYPE_GENERIC,
    CONTROLLER_TYPE_BEITONG,
    CONTROLLER_TYPE_MAX
} controller_type_t;

/* 控制模式枚举 */
typedef enum {
    CONTROL_MODE_CAR = 0,
    CONTROL_MODE_PLANE,
    CONTROL_MODE_CUSTOM,
    CONTROL_MODE_MAX
} control_mode_t;

/* 加速度曲线类型 */
typedef enum {
    ACCEL_CURVE_LINEAR = 0,
    ACCEL_CURVE_EXPONENTIAL,
    ACCEL_CURVE_LOGARITHMIC,
    ACCEL_CURVE_MAX
} accel_curve_t;

/* 震动模式类型 */
typedef enum {
    VIBRATION_PATTERN_SINGLE = 0,
    VIBRATION_PATTERN_DOUBLE,
    VIBRATION_PATTERN_PATTERN,
    VIBRATION_PATTERN_MAX
} vibration_pattern_t;

/* 日志级别 */
typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_VERBOSE,
    LOG_LEVEL_MAX
} log_level_t;

/* 手柄配置结构 */
typedef struct {
    controller_type_t supported_controllers[4];
    controller_type_t default_controller;
    uint32_t connection_timeout;
    uint8_t max_reconnect_attempts;
} gamepad_config_t;

/* 控制配置结构 */
typedef struct {
    control_mode_t default_mode;
    uint8_t stick_deadzone;
    uint8_t max_speed;
    accel_curve_t acceleration_curve;
} control_config_t;

/* 震动配置结构 */
typedef struct {
    bool enable_vibration;
    uint8_t default_intensity;
    uint16_t default_duration;
    vibration_pattern_t default_pattern;
} vibration_config_t;

/* 蓝牙配置结构 */
typedef struct {
    char device_name[32];
    uint16_t scan_window;
    uint16_t scan_interval;
    uint16_t connection_interval;
} bluetooth_config_t;

/* GPIO配置结构 */
typedef struct {
    // 小车控制引脚
    uint8_t car_motor_left_forward;
    uint8_t car_motor_left_backward;
    uint8_t car_motor_right_forward;
    uint8_t car_motor_right_backward;
    uint8_t car_motor_enable;
    
    // 飞机控制引脚
    uint8_t plane_throttle;
    uint8_t plane_aileron;
    uint8_t plane_elevator;
    uint8_t plane_rudder;
    
    // 震动马达引脚
    uint8_t vibration_motor_left;
    uint8_t vibration_motor_right;
    
    // 状态指示LED
    uint8_t status_led;
    uint8_t bluetooth_led;
} gpio_config_t;

/* PWM配置结构 */
typedef struct {
    uint16_t motor_frequency;
    uint16_t servo_frequency;
    uint8_t resolution;
    uint16_t servo_min_pulse;
    uint16_t servo_max_pulse;
} pwm_config_t;

/* 安全配置结构 */
typedef struct {
    bool enable_watchdog;
    uint32_t connection_lost_timeout;
    uint32_t emergency_stop_keys;
    bool battery_monitor;
    float low_battery_threshold;
} safety_config_t;

/* 调试配置结构 */
typedef struct {
    bool enable_debug;
    log_level_t log_level;
    uint32_t uart_baudrate;
    bool enable_performance_monitor;
} debug_config_t;

/* 全局配置结构 */
typedef struct {
    gamepad_config_t gamepad;
    control_config_t control;
    vibration_config_t vibration;
    bluetooth_config_t bluetooth;
    gpio_config_t gpio;
    pwm_config_t pwm;
    safety_config_t safety;
    debug_config_t debug;
} global_config_t;

/**
 * @brief 初始化配置管理器
 * 
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t config_manager_init(void);

/**
 * @brief 反初始化配置管理器
 * 
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t config_manager_deinit(void);

/**
 * @brief 加载配置文件
 * 
 * @param config_file 配置文件路径
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t config_manager_load(const char *config_file);

/**
 * @brief 保存配置到文件
 * 
 * @param config_file 配置文件路径
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t config_manager_save(const char *config_file);

/**
 * @brief 获取全局配置
 * 
 * @return const global_config_t* 配置指针
 */
const global_config_t* config_manager_get_global_config(void);

/**
 * @brief 获取手柄配置
 * 
 * @return const gamepad_config_t* 配置指针
 */
const gamepad_config_t* config_manager_get_gamepad_config(void);

/**
 * @brief 获取控制配置
 * 
 * @return const control_config_t* 配置指针
 */
const control_config_t* config_manager_get_control_config(void);

/**
 * @brief 获取震动配置
 * 
 * @return const vibration_config_t* 配置指针
 */
const vibration_config_t* config_manager_get_vibration_config(void);

/**
 * @brief 获取蓝牙配置
 * 
 * @return const bluetooth_config_t* 配置指针
 */
const bluetooth_config_t* config_manager_get_bluetooth_config(void);

/**
 * @brief 获取GPIO配置
 * 
 * @return const gpio_config_t* 配置指针
 */
const gpio_config_t* config_manager_get_gpio_config(void);

/**
 * @brief 获取PWM配置
 * 
 * @return const pwm_config_t* 配置指针
 */
const pwm_config_t* config_manager_get_pwm_config(void);

/**
 * @brief 获取安全配置
 * 
 * @return const safety_config_t* 配置指针
 */
const safety_config_t* config_manager_get_safety_config(void);

/**
 * @brief 获取调试配置
 * 
 * @return const debug_config_t* 配置指针
 */
const debug_config_t* config_manager_get_debug_config(void);

/**
 * @brief 设置手柄配置
 * 
 * @param config 新的配置
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t config_manager_set_gamepad_config(const gamepad_config_t *config);

/**
 * @brief 设置控制配置
 * 
 * @param config 新的配置
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t config_manager_set_control_config(const control_config_t *config);

/**
 * @brief 重置所有配置为默认值
 * 
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t config_manager_reset_to_defaults(void);

/**
 * @brief 验证配置参数有效性
 * 
 * @return esp_err_t ESP_OK有效，其他值无效
 */
esp_err_t config_manager_validate_config(void);

/**
 * @brief 配置参数更新回调函数类型
 * 
 * @param type 配置类型
 * @param config 新配置数据
 */
typedef void (*config_update_callback_t)(config_type_t type, const void *config);

/**
 * @brief 注册配置更新回调
 * 
 * @param callback 回调函数
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t config_manager_register_callback(config_update_callback_t callback);

/**
 * @brief 获取配置版本信息
 * 
 * @return const char* 版本字符串
 */
const char* config_manager_get_version(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_H
