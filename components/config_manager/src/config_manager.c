/**
 * @file config_manager.c
 * @brief ESP32-Gamepad 配置管理器实现
 * 
 * 负责配置文件的读取、解析、验证和管理
 * 
 * @author ESP32-Gamepad Team
 * @date 2024
 */

#include "config_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_spiffs.h"

static const char *TAG = "CONFIG_MANAGER";

/* 配置管理器版本 */
#define CONFIG_MANAGER_VERSION "1.0.0"

/* NVS存储命名空间 */
#define NVS_NAMESPACE "gamepad_cfg"

/* 配置文件默认路径 */
#define DEFAULT_CONFIG_FILE "/spiffs/gamepad_config.ini"

/* 全局配置实例 */
static global_config_t g_config;
static bool g_config_initialized = false;
static config_update_callback_t g_update_callback = NULL;

/* 默认配置值 */
static const global_config_t g_default_config = {
    .gamepad = {
        .supported_controllers = {
            CONTROLLER_TYPE_BEITONG,
            CONTROLLER_TYPE_PS4,
            CONTROLLER_TYPE_XBOX,
            CONTROLLER_TYPE_GENERIC
        },
        .default_controller = CONTROLLER_TYPE_BEITONG,
        .connection_timeout = 30,
        .max_reconnect_attempts = 5
    },
    .control = {
        .default_mode = CONTROL_MODE_CAR,
        .stick_deadzone = 10,
        .max_speed = 255,
        .acceleration_curve = ACCEL_CURVE_EXPONENTIAL
    },
    .vibration = {
        .enable_vibration = true,
        .default_intensity = 128,
        .default_duration = 200,
        .default_pattern = VIBRATION_PATTERN_SINGLE
    },
    .bluetooth = {
        .device_name = "ESP32-Gamepad",
        .scan_window = 100,
        .scan_interval = 100,
        .connection_interval = 20
    },
    .gpio = {
        .car_motor_left_forward = 18,
        .car_motor_left_backward = 19,
        .car_motor_right_forward = 21,
        .car_motor_right_backward = 22,
        .car_motor_enable = 23,
        .plane_throttle = 25,
        .plane_aileron = 26,
        .plane_elevator = 27,
        .plane_rudder = 14,
        .vibration_motor_left = 32,
        .vibration_motor_right = 33,
        .status_led = 2,
        .bluetooth_led = 4
    },
    .pwm = {
        .motor_frequency = 1000,
        .servo_frequency = 50,
        .resolution = 8,
        .servo_min_pulse = 1000,
        .servo_max_pulse = 2000
    },
    .safety = {
        .enable_watchdog = true,
        .connection_lost_timeout = 5000,
        .emergency_stop_keys = 0x0F, // L1+R1+SELECT+START
        .battery_monitor = true,
        .low_battery_threshold = 3.3
    },
    .debug = {
        .enable_debug = true,
        .log_level = LOG_LEVEL_INFO,
        .uart_baudrate = 115200,
        .enable_performance_monitor = false
    }
};

/* 私有函数声明 */
static esp_err_t config_init_spiffs(void);
static esp_err_t config_init_nvs(void);
static esp_err_t config_parse_line(const char *line);
static esp_err_t config_parse_gamepad_section(const char *key, const char *value);
static esp_err_t config_parse_control_section(const char *key, const char *value);
static esp_err_t config_parse_vibration_section(const char *key, const char *value);
static esp_err_t config_parse_bluetooth_section(const char *key, const char *value);
static esp_err_t config_parse_gpio_section(const char *key, const char *value);
static esp_err_t config_parse_pwm_section(const char *key, const char *value);
static esp_err_t config_parse_safety_section(const char *key, const char *value);
static esp_err_t config_parse_debug_section(const char *key, const char *value);
static char* config_trim_whitespace(char *str);
static controller_type_t config_parse_controller_type(const char *str);
static control_mode_t config_parse_control_mode(const char *str);
static accel_curve_t config_parse_accel_curve(const char *str);
static vibration_pattern_t config_parse_vibration_pattern(const char *str);
static log_level_t config_parse_log_level(const char *str);
static void config_notify_update(config_type_t type, const void *config);

/**
 * @brief 初始化配置管理器
 */
esp_err_t config_manager_init(void)
{
    esp_err_t ret;

    if (g_config_initialized) {
        ESP_LOGW(TAG, "Config manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing config manager v%s", CONFIG_MANAGER_VERSION);

    // 初始化NVS
    ret = config_init_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // 初始化SPIFFS
    ret = config_init_spiffs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS: %s", esp_err_to_name(ret));
        return ret;
    }

    // 加载默认配置
    memcpy(&g_config, &g_default_config, sizeof(global_config_t));

    // 尝试从文件加载配置
    ret = config_manager_load(DEFAULT_CONFIG_FILE);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load config file, using defaults");
    }

    // 验证配置
    ret = config_manager_validate_config();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Invalid config detected, resetting to defaults");
        config_manager_reset_to_defaults();
    }

    g_config_initialized = true;
    ESP_LOGI(TAG, "Config manager initialized successfully");

    return ESP_OK;
}

/**
 * @brief 反初始化配置管理器
 */
esp_err_t config_manager_deinit(void)
{
    if (!g_config_initialized) {
        return ESP_OK;
    }

    // 保存当前配置
    config_manager_save(DEFAULT_CONFIG_FILE);

    // 卸载SPIFFS
    esp_vfs_spiffs_unregister(NULL);

    g_config_initialized = false;
    g_update_callback = NULL;

    ESP_LOGI(TAG, "Config manager deinitialized");
    return ESP_OK;
}

/**
 * @brief 加载配置文件
 */
esp_err_t config_manager_load(const char *config_file)
{
    if (!g_config_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!config_file) {
        config_file = DEFAULT_CONFIG_FILE;
    }

    FILE *file = fopen(config_file, "r");
    if (!file) {
        ESP_LOGW(TAG, "Config file not found: %s", config_file);
        return ESP_ERR_NOT_FOUND;
    }

    char line[256];
    esp_err_t ret = ESP_OK;
    int line_num = 0;

    while (fgets(line, sizeof(line), file)) {
        line_num++;
        if (config_parse_line(line) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to parse line %d: %s", line_num, line);
        }
    }

    fclose(file);
    ESP_LOGI(TAG, "Config loaded from: %s", config_file);
    return ret;
}

/**
 * @brief 保存配置到文件
 */
esp_err_t config_manager_save(const char *config_file)
{
    if (!g_config_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!config_file) {
        config_file = DEFAULT_CONFIG_FILE;
    }

    FILE *file = fopen(config_file, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to create config file: %s", config_file);
        return ESP_ERR_NO_MEM;
    }

    // 写入配置文件头
    fprintf(file, "# ESP32-Gamepad Configuration File\n");
    fprintf(file, "# Auto-generated on startup\n\n");

    // 写入各个配置节
    fprintf(file, "[gamepad]\n");
    fprintf(file, "default_controller = %d\n", g_config.gamepad.default_controller);
    fprintf(file, "connection_timeout = %lu\n", g_config.gamepad.connection_timeout);
    fprintf(file, "max_reconnect_attempts = %d\n\n", g_config.gamepad.max_reconnect_attempts);

    fprintf(file, "[control]\n");
    fprintf(file, "default_mode = %d\n", g_config.control.default_mode);
    fprintf(file, "stick_deadzone = %d\n", g_config.control.stick_deadzone);
    fprintf(file, "max_speed = %d\n", g_config.control.max_speed);
    fprintf(file, "acceleration_curve = %d\n\n", g_config.control.acceleration_curve);

    fprintf(file, "[vibration]\n");
    fprintf(file, "enable_vibration = %s\n", g_config.vibration.enable_vibration ? "true" : "false");
    fprintf(file, "default_intensity = %d\n", g_config.vibration.default_intensity);
    fprintf(file, "default_duration = %d\n", g_config.vibration.default_duration);
    fprintf(file, "default_pattern = %d\n\n", g_config.vibration.default_pattern);

    fprintf(file, "[bluetooth]\n");
    fprintf(file, "device_name = %s\n", g_config.bluetooth.device_name);
    fprintf(file, "scan_window = %d\n", g_config.bluetooth.scan_window);
    fprintf(file, "scan_interval = %d\n", g_config.bluetooth.scan_interval);
    fprintf(file, "connection_interval = %d\n\n", g_config.bluetooth.connection_interval);

    fclose(file);
    ESP_LOGI(TAG, "Config saved to: %s", config_file);
    return ESP_OK;
}

/**
 * @brief 获取全局配置
 */
const global_config_t* config_manager_get_global_config(void)
{
    return g_config_initialized ? &g_config : NULL;
}

/**
 * @brief 获取手柄配置
 */
const gamepad_config_t* config_manager_get_gamepad_config(void)
{
    return g_config_initialized ? &g_config.gamepad : NULL;
}

/**
 * @brief 获取控制配置
 */
const control_config_t* config_manager_get_control_config(void)
{
    return g_config_initialized ? &g_config.control : NULL;
}

/**
 * @brief 获取震动配置
 */
const vibration_config_t* config_manager_get_vibration_config(void)
{
    return g_config_initialized ? &g_config.vibration : NULL;
}

/**
 * @brief 获取蓝牙配置
 */
const bluetooth_config_t* config_manager_get_bluetooth_config(void)
{
    return g_config_initialized ? &g_config.bluetooth : NULL;
}

/**
 * @brief 获取GPIO配置
 */
const gpio_config_t* config_manager_get_gpio_config(void)
{
    return g_config_initialized ? &g_config.gpio : NULL;
}

/**
 * @brief 获取PWM配置
 */
const pwm_config_t* config_manager_get_pwm_config(void)
{
    return g_config_initialized ? &g_config.pwm : NULL;
}

/**
 * @brief 获取安全配置
 */
const safety_config_t* config_manager_get_safety_config(void)
{
    return g_config_initialized ? &g_config.safety : NULL;
}

/**
 * @brief 获取调试配置
 */
const debug_config_t* config_manager_get_debug_config(void)
{
    return g_config_initialized ? &g_config.debug : NULL;
}

/**
 * @brief 重置所有配置为默认值
 */
esp_err_t config_manager_reset_to_defaults(void)
{
    if (!g_config_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(&g_config, &g_default_config, sizeof(global_config_t));
    ESP_LOGI(TAG, "Configuration reset to defaults");
    return ESP_OK;
}

/**
 * @brief 验证配置参数有效性
 */
esp_err_t config_manager_validate_config(void)
{
    if (!g_config_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // 验证手柄配置
    if (g_config.gamepad.default_controller >= CONTROLLER_TYPE_MAX) {
        ESP_LOGE(TAG, "Invalid controller type");
        return ESP_ERR_INVALID_ARG;
    }

    // 验证控制配置
    if (g_config.control.default_mode >= CONTROL_MODE_MAX) {
        ESP_LOGE(TAG, "Invalid control mode");
        return ESP_ERR_INVALID_ARG;
    }

    if (g_config.control.stick_deadzone > 50) {
        ESP_LOGE(TAG, "Invalid stick deadzone");
        return ESP_ERR_INVALID_ARG;
    }

    // 验证PWM配置
    if (g_config.pwm.motor_frequency == 0 || g_config.pwm.servo_frequency == 0) {
        ESP_LOGE(TAG, "Invalid PWM frequency");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Configuration validation passed");
    return ESP_OK;
}

/**
 * @brief 注册配置更新回调
 */
esp_err_t config_manager_register_callback(config_update_callback_t callback)
{
    g_update_callback = callback;
    return ESP_OK;
}

/**
 * @brief 获取配置版本信息
 */
const char* config_manager_get_version(void)
{
    return CONFIG_MANAGER_VERSION;
}

/* 私有函数实现 */

/**
 * @brief 初始化SPIFFS文件系统
 */
static esp_err_t config_init_spiffs(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS: %d KB total, %d KB used", total / 1024, used / 1024);
    }

    return ESP_OK;
}

/**
 * @brief 初始化NVS存储
 */
static esp_err_t config_init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

/**
 * @brief 解析配置文件行
 */
static esp_err_t config_parse_line(const char *line)
{
    static char current_section[32] = {0};
    char *line_copy = strdup(line);
    char *trimmed = config_trim_whitespace(line_copy);

    // 跳过空行和注释
    if (strlen(trimmed) == 0 || trimmed[0] == '#') {
        free(line_copy);
        return ESP_OK;
    }

    // 检查是否是节头
    if (trimmed[0] == '[' && trimmed[strlen(trimmed) - 1] == ']') {
        strncpy(current_section, trimmed + 1, strlen(trimmed) - 2);
        current_section[strlen(trimmed) - 2] = '\0';
        free(line_copy);
        return ESP_OK;
    }

    // 解析键值对
    char *eq_pos = strchr(trimmed, '=');
    if (!eq_pos) {
        free(line_copy);
        return ESP_ERR_INVALID_ARG;
    }

    *eq_pos = '\0';
    char *key = config_trim_whitespace(trimmed);
    char *value = config_trim_whitespace(eq_pos + 1);

    esp_err_t ret = ESP_OK;

    // 根据当前节处理配置
    if (strcmp(current_section, "gamepad") == 0) {
        ret = config_parse_gamepad_section(key, value);
    } else if (strcmp(current_section, "control") == 0) {
        ret = config_parse_control_section(key, value);
    } else if (strcmp(current_section, "vibration") == 0) {
        ret = config_parse_vibration_section(key, value);
    } else if (strcmp(current_section, "bluetooth") == 0) {
        ret = config_parse_bluetooth_section(key, value);
    } else if (strcmp(current_section, "gpio") == 0) {
        ret = config_parse_gpio_section(key, value);
    } else if (strcmp(current_section, "pwm") == 0) {
        ret = config_parse_pwm_section(key, value);
    } else if (strcmp(current_section, "safety") == 0) {
        ret = config_parse_safety_section(key, value);
    } else if (strcmp(current_section, "debug") == 0) {
        ret = config_parse_debug_section(key, value);
    }

    free(line_copy);
    return ret;
}

/**
 * @brief 解析手柄配置节
 */
static esp_err_t config_parse_gamepad_section(const char *key, const char *value)
{
    if (strcmp(key, "default_controller") == 0) {
        g_config.gamepad.default_controller = config_parse_controller_type(value);
    } else if (strcmp(key, "connection_timeout") == 0) {
        g_config.gamepad.connection_timeout = atoi(value);
    } else if (strcmp(key, "max_reconnect_attempts") == 0) {
        g_config.gamepad.max_reconnect_attempts = atoi(value);
    }
    return ESP_OK;
}

/**
 * @brief 解析控制配置节
 */
static esp_err_t config_parse_control_section(const char *key, const char *value)
{
    if (strcmp(key, "default_mode") == 0) {
        g_config.control.default_mode = config_parse_control_mode(value);
    } else if (strcmp(key, "stick_deadzone") == 0) {
        g_config.control.stick_deadzone = atoi(value);
    } else if (strcmp(key, "max_speed") == 0) {
        g_config.control.max_speed = atoi(value);
    } else if (strcmp(key, "acceleration_curve") == 0) {
        g_config.control.acceleration_curve = config_parse_accel_curve(value);
    }
    return ESP_OK;
}

/**
 * @brief 解析震动配置节
 */
static esp_err_t config_parse_vibration_section(const char *key, const char *value)
{
    if (strcmp(key, "enable_vibration") == 0) {
        g_config.vibration.enable_vibration = (strcmp(value, "true") == 0);
    } else if (strcmp(key, "default_intensity") == 0) {
        g_config.vibration.default_intensity = atoi(value);
    } else if (strcmp(key, "default_duration") == 0) {
        g_config.vibration.default_duration = atoi(value);
    } else if (strcmp(key, "default_pattern") == 0) {
        g_config.vibration.default_pattern = config_parse_vibration_pattern(value);
    }
    return ESP_OK;
}

/**
 * @brief 解析蓝牙配置节
 */
static esp_err_t config_parse_bluetooth_section(const char *key, const char *value)
{
    if (strcmp(key, "device_name") == 0) {
        strncpy(g_config.bluetooth.device_name, value, sizeof(g_config.bluetooth.device_name) - 1);
    } else if (strcmp(key, "scan_window") == 0) {
        g_config.bluetooth.scan_window = atoi(value);
    } else if (strcmp(key, "scan_interval") == 0) {
        g_config.bluetooth.scan_interval = atoi(value);
    } else if (strcmp(key, "connection_interval") == 0) {
        g_config.bluetooth.connection_interval = atoi(value);
    }
    return ESP_OK;
}

/**
 * @brief 解析GPIO配置节
 */
static esp_err_t config_parse_gpio_section(const char *key, const char *value)
{
    int pin = atoi(value);
    
    if (strcmp(key, "car_motor_left_forward") == 0) {
        g_config.gpio.car_motor_left_forward = pin;
    } else if (strcmp(key, "car_motor_left_backward") == 0) {
        g_config.gpio.car_motor_left_backward = pin;
    } else if (strcmp(key, "car_motor_right_forward") == 0) {
        g_config.gpio.car_motor_right_forward = pin;
    } else if (strcmp(key, "car_motor_right_backward") == 0) {
        g_config.gpio.car_motor_right_backward = pin;
    } else if (strcmp(key, "car_motor_enable") == 0) {
        g_config.gpio.car_motor_enable = pin;
    } else if (strcmp(key, "plane_throttle") == 0) {
        g_config.gpio.plane_throttle = pin;
    } else if (strcmp(key, "plane_aileron") == 0) {
        g_config.gpio.plane_aileron = pin;
    } else if (strcmp(key, "plane_elevator") == 0) {
        g_config.gpio.plane_elevator = pin;
    } else if (strcmp(key, "plane_rudder") == 0) {
        g_config.gpio.plane_rudder = pin;
    } else if (strcmp(key, "vibration_motor_left") == 0) {
        g_config.gpio.vibration_motor_left = pin;
    } else if (strcmp(key, "vibration_motor_right") == 0) {
        g_config.gpio.vibration_motor_right = pin;
    } else if (strcmp(key, "status_led") == 0) {
        g_config.gpio.status_led = pin;
    } else if (strcmp(key, "bluetooth_led") == 0) {
        g_config.gpio.bluetooth_led = pin;
    }
    return ESP_OK;
}

/**
 * @brief 解析PWM配置节
 */
static esp_err_t config_parse_pwm_section(const char *key, const char *value)
{
    if (strcmp(key, "motor_frequency") == 0) {
        g_config.pwm.motor_frequency = atoi(value);
    } else if (strcmp(key, "servo_frequency") == 0) {
        g_config.pwm.servo_frequency = atoi(value);
    } else if (strcmp(key, "resolution") == 0) {
        g_config.pwm.resolution = atoi(value);
    } else if (strcmp(key, "servo_min_pulse") == 0) {
        g_config.pwm.servo_min_pulse = atoi(value);
    } else if (strcmp(key, "servo_max_pulse") == 0) {
        g_config.pwm.servo_max_pulse = atoi(value);
    }
    return ESP_OK;
}

/**
 * @brief 解析安全配置节
 */
static esp_err_t config_parse_safety_section(const char *key, const char *value)
{
    if (strcmp(key, "enable_watchdog") == 0) {
        g_config.safety.enable_watchdog = (strcmp(value, "true") == 0);
    } else if (strcmp(key, "connection_lost_timeout") == 0) {
        g_config.safety.connection_lost_timeout = atoi(value);
    } else if (strcmp(key, "battery_monitor") == 0) {
        g_config.safety.battery_monitor = (strcmp(value, "true") == 0);
    } else if (strcmp(key, "low_battery_threshold") == 0) {
        g_config.safety.low_battery_threshold = atof(value);
    }
    return ESP_OK;
}

/**
 * @brief 解析调试配置节
 */
static esp_err_t config_parse_debug_section(const char *key, const char *value)
{
    if (strcmp(key, "enable_debug") == 0) {
        g_config.debug.enable_debug = (strcmp(value, "true") == 0);
    } else if (strcmp(key, "log_level") == 0) {
        g_config.debug.log_level = config_parse_log_level(value);
    } else if (strcmp(key, "uart_baudrate") == 0) {
        g_config.debug.uart_baudrate = atoi(value);
    } else if (strcmp(key, "enable_performance_monitor") == 0) {
        g_config.debug.enable_performance_monitor = (strcmp(value, "true") == 0);
    }
    return ESP_OK;
}

/**
 * @brief 去除字符串首尾空白字符
 */
static char* config_trim_whitespace(char *str)
{
    char *end;

    // 去除前导空白字符
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    // 去除尾随空白字符
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

/**
 * @brief 解析控制器类型字符串
 */
static controller_type_t config_parse_controller_type(const char *str)
{
    if (strcmp(str, "ps4") == 0) return CONTROLLER_TYPE_PS4;
    if (strcmp(str, "xbox") == 0) return CONTROLLER_TYPE_XBOX;
    if (strcmp(str, "generic") == 0) return CONTROLLER_TYPE_GENERIC;
    if (strcmp(str, "beitong") == 0) return CONTROLLER_TYPE_BEITONG;
    return (controller_type_t)atoi(str);
}

/**
 * @brief 解析控制模式字符串
 */
static control_mode_t config_parse_control_mode(const char *str)
{
    if (strcmp(str, "car") == 0) return CONTROL_MODE_CAR;
    if (strcmp(str, "plane") == 0) return CONTROL_MODE_PLANE;
    if (strcmp(str, "custom") == 0) return CONTROL_MODE_CUSTOM;
    return (control_mode_t)atoi(str);
}

/**
 * @brief 解析加速度曲线字符串
 */
static accel_curve_t config_parse_accel_curve(const char *str)
{
    if (strcmp(str, "linear") == 0) return ACCEL_CURVE_LINEAR;
    if (strcmp(str, "exponential") == 0) return ACCEL_CURVE_EXPONENTIAL;
    if (strcmp(str, "logarithmic") == 0) return ACCEL_CURVE_LOGARITHMIC;
    return (accel_curve_t)atoi(str);
}

/**
 * @brief 解析震动模式字符串
 */
static vibration_pattern_t config_parse_vibration_pattern(const char *str)
{
    if (strcmp(str, "single") == 0) return VIBRATION_PATTERN_SINGLE;
    if (strcmp(str, "double") == 0) return VIBRATION_PATTERN_DOUBLE;
    if (strcmp(str, "pattern") == 0) return VIBRATION_PATTERN_PATTERN;
    return (vibration_pattern_t)atoi(str);
}

/**
 * @brief 解析日志级别字符串
 */
static log_level_t config_parse_log_level(const char *str)
{
    if (strcmp(str, "ERROR") == 0) return LOG_LEVEL_ERROR;
    if (strcmp(str, "WARN") == 0) return LOG_LEVEL_WARN;
    if (strcmp(str, "INFO") == 0) return LOG_LEVEL_INFO;
    if (strcmp(str, "DEBUG") == 0) return LOG_LEVEL_DEBUG;
    if (strcmp(str, "VERBOSE") == 0) return LOG_LEVEL_VERBOSE;
    return (log_level_t)atoi(str);
}

/**
 * @brief 通知配置更新
 */
static void __attribute__((unused)) config_notify_update(config_type_t type, const void *config)
{
    if (g_update_callback) {
        g_update_callback(type, config);
    }
}
