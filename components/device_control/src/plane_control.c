/**
 * @file plane_control.c
 * @brief 飞机控制实现
 */

#include "plane_control.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include <string.h>

static const char *TAG = "PLANE_CTRL";

// 静态变量
static plane_servo_config_t servo_config = {0};
static plane_control_params_t current_params = {0};
static bool initialized = false;

// LEDC配置
#define LEDC_TIMER              LEDC_TIMER_1
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_THROTTLE_CHANNEL   LEDC_CHANNEL_2
#define LEDC_ELEVATOR_CHANNEL   LEDC_CHANNEL_3
#define LEDC_RUDDER_CHANNEL     LEDC_CHANNEL_4
#define LEDC_AILERON_CHANNEL    LEDC_CHANNEL_5
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT

/**
 * @brief 将控制值转换为PWM占空比
 */
static uint32_t control_to_duty(int16_t control_value, uint16_t min_us, uint16_t max_us, uint16_t center_us)
{
    uint32_t pulse_width_us;
    
    if (control_value == 0) {
        pulse_width_us = center_us;
    } else if (control_value > 0) {
        // 正值：从中心到最大
        pulse_width_us = center_us + (uint32_t)(control_value * (max_us - center_us) / 1000);
    } else {
        // 负值：从中心到最小
        pulse_width_us = center_us - (uint32_t)((-control_value) * (center_us - min_us) / 1000);
    }
    
    // 限制在有效范围内
    if (pulse_width_us < min_us) pulse_width_us = min_us;
    if (pulse_width_us > max_us) pulse_width_us = max_us;
    
    // 计算占空比 (13位分辨率)
    uint32_t duty = (pulse_width_us * 8191) / (1000000 / servo_config.pwm_frequency);
    
    return duty;
}

/**
 * @brief 油门特殊处理（只有正值）
 */
static uint32_t throttle_to_duty(int16_t throttle_value)
{
    // 油门范围：0-1000 对应 servo_min_us 到 servo_max_us
    if (throttle_value < 0) throttle_value = 0;
    if (throttle_value > 1000) throttle_value = 1000;
    
    uint32_t pulse_width_us = servo_config.servo_min_us + 
                             (uint32_t)(throttle_value * (servo_config.servo_max_us - servo_config.servo_min_us) / 1000);
    
    uint32_t duty = (pulse_width_us * 8191) / (1000000 / servo_config.pwm_frequency);
    
    return duty;
}

/**
 * @brief 初始化PWM
 */
static esp_err_t init_pwm(void)
{
    // 配置LEDC定时器
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = servo_config.pwm_frequency,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 配置油门通道
    ledc_channel_config_t throttle_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_THROTTLE_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = servo_config.throttle_pin,
        .duty           = throttle_to_duty(0), // 初始油门为0
        .hpoint         = 0
    };
    
    ret = ledc_channel_config(&throttle_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure throttle channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 配置升降舵通道
    ledc_channel_config_t elevator_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_ELEVATOR_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = servo_config.elevator_pin,
        .duty           = control_to_duty(0, servo_config.servo_min_us, 
                                        servo_config.servo_max_us, 
                                        servo_config.servo_center_us),
        .hpoint         = 0
    };
    
    ret = ledc_channel_config(&elevator_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure elevator channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 配置方向舵通道
    ledc_channel_config_t rudder_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_RUDDER_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = servo_config.rudder_pin,
        .duty           = control_to_duty(0, servo_config.servo_min_us, 
                                        servo_config.servo_max_us, 
                                        servo_config.servo_center_us),
        .hpoint         = 0
    };
    
    ret = ledc_channel_config(&rudder_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure rudder channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 配置副翼通道
    ledc_channel_config_t aileron_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_AILERON_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = servo_config.aileron_pin,
        .duty           = control_to_duty(0, servo_config.servo_min_us, 
                                        servo_config.servo_max_us, 
                                        servo_config.servo_center_us),
        .hpoint         = 0
    };
    
    ret = ledc_channel_config(&aileron_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure aileron channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Servo PWM initialized successfully");
    return ESP_OK;
}

esp_err_t plane_control_init(const plane_servo_config_t *config)
{
    ESP_LOGI(TAG, "Initializing plane control...");
    
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (initialized) {
        ESP_LOGW(TAG, "Plane control already initialized");
        return ESP_OK;
    }
    
    // 保存配置
    memcpy(&servo_config, config, sizeof(plane_servo_config_t));
    
    // 验证配置参数
    if (servo_config.servo_min_us >= servo_config.servo_max_us) {
        ESP_LOGE(TAG, "Invalid servo pulse width configuration");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (servo_config.servo_center_us < servo_config.servo_min_us || 
        servo_config.servo_center_us > servo_config.servo_max_us) {
        ESP_LOGE(TAG, "Invalid servo center pulse width");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 初始化PWM
    esp_err_t ret = init_pwm();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 初始化状态
    memset(&current_params, 0, sizeof(current_params));
    initialized = true;
    
    ESP_LOGI(TAG, "Plane control initialized successfully");
    ESP_LOGI(TAG, "Servo config: freq=%luHz, min=%dus, max=%dus, center=%dus", 
             servo_config.pwm_frequency, servo_config.servo_min_us, 
             servo_config.servo_max_us, servo_config.servo_center_us);
    ESP_LOGI(TAG, "Pins: throttle=%d, elevator=%d, rudder=%d, aileron=%d", 
             servo_config.throttle_pin, servo_config.elevator_pin, 
             servo_config.rudder_pin, servo_config.aileron_pin);
    
    return ESP_OK;
}

esp_err_t plane_control_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing plane control...");
    
    if (!initialized) {
        ESP_LOGW(TAG, "Plane control not initialized");
        return ESP_OK;
    }
    
    // 紧急停止
    plane_control_emergency_stop();
    
    // 停止PWM通道
    ledc_stop(LEDC_MODE, LEDC_THROTTLE_CHANNEL, 0);
    ledc_stop(LEDC_MODE, LEDC_ELEVATOR_CHANNEL, 0);
    ledc_stop(LEDC_MODE, LEDC_RUDDER_CHANNEL, 0);
    ledc_stop(LEDC_MODE, LEDC_AILERON_CHANNEL, 0);
    
    initialized = false;
    ESP_LOGI(TAG, "Plane control deinitialized");
    
    return ESP_OK;
}

esp_err_t plane_control_set_params(const plane_control_params_t *params)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Plane control not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!params) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 限制参数范围
    int16_t throttle = params->throttle;
    int16_t elevator = params->elevator;
    int16_t rudder = params->rudder;
    int16_t aileron = params->aileron;
    
    if (throttle < 0) throttle = 0;
    if (throttle > 1000) throttle = 1000;
    if (elevator > 1000) elevator = 1000;
    if (elevator < -1000) elevator = -1000;
    if (rudder > 1000) rudder = 1000;
    if (rudder < -1000) rudder = -1000;
    if (aileron > 1000) aileron = 1000;
    if (aileron < -1000) aileron = -1000;
    
    ESP_LOGD(TAG, "Setting params: throttle=%d, elevator=%d, rudder=%d, aileron=%d", 
             throttle, elevator, rudder, aileron);
    
    // 设置油门
    uint32_t throttle_duty = throttle_to_duty(throttle);
    esp_err_t ret = ledc_set_duty(LEDC_MODE, LEDC_THROTTLE_CHANNEL, throttle_duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set throttle duty: %s", esp_err_to_name(ret));
        return ret;
    }
    ledc_update_duty(LEDC_MODE, LEDC_THROTTLE_CHANNEL);
    
    // 设置升降舵
    uint32_t elevator_duty = control_to_duty(elevator, servo_config.servo_min_us, 
                                           servo_config.servo_max_us, 
                                           servo_config.servo_center_us);
    ret = ledc_set_duty(LEDC_MODE, LEDC_ELEVATOR_CHANNEL, elevator_duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set elevator duty: %s", esp_err_to_name(ret));
        return ret;
    }
    ledc_update_duty(LEDC_MODE, LEDC_ELEVATOR_CHANNEL);
    
    // 设置方向舵
    uint32_t rudder_duty = control_to_duty(rudder, servo_config.servo_min_us, 
                                         servo_config.servo_max_us, 
                                         servo_config.servo_center_us);
    ret = ledc_set_duty(LEDC_MODE, LEDC_RUDDER_CHANNEL, rudder_duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set rudder duty: %s", esp_err_to_name(ret));
        return ret;
    }
    ledc_update_duty(LEDC_MODE, LEDC_RUDDER_CHANNEL);
    
    // 设置副翼
    uint32_t aileron_duty = control_to_duty(aileron, servo_config.servo_min_us, 
                                          servo_config.servo_max_us, 
                                          servo_config.servo_center_us);
    ret = ledc_set_duty(LEDC_MODE, LEDC_AILERON_CHANNEL, aileron_duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set aileron duty: %s", esp_err_to_name(ret));
        return ret;
    }
    ledc_update_duty(LEDC_MODE, LEDC_AILERON_CHANNEL);
    
    // 更新当前状态
    current_params.throttle = throttle;
    current_params.elevator = elevator;
    current_params.rudder = rudder;
    current_params.aileron = aileron;
    
    return ESP_OK;
}

esp_err_t plane_control_set_neutral(void)
{
    ESP_LOGI(TAG, "Setting plane to neutral position");
    
    plane_control_params_t neutral_params = {
        .throttle = 0,
        .elevator = 0,
        .rudder = 0,
        .aileron = 0
    };
    
    return plane_control_set_params(&neutral_params);
}

esp_err_t plane_control_emergency_stop(void)
{
    ESP_LOGI(TAG, "Emergency stop - cutting throttle");
    
    if (!initialized) {
        ESP_LOGE(TAG, "Plane control not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 立即将油门设为0
    uint32_t throttle_duty = throttle_to_duty(0);
    esp_err_t ret = ledc_set_duty(LEDC_MODE, LEDC_THROTTLE_CHANNEL, throttle_duty);
    if (ret == ESP_OK) {
        ledc_update_duty(LEDC_MODE, LEDC_THROTTLE_CHANNEL);
        current_params.throttle = 0;
    }
    
    return ret;
}

esp_err_t plane_control_get_status(plane_control_params_t *params)
{
    if (!params) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!initialized) {
        ESP_LOGE(TAG, "Plane control not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    memcpy(params, &current_params, sizeof(plane_control_params_t));
    return ESP_OK;
}

esp_err_t plane_control_calibrate_servos(void)
{
    ESP_LOGI(TAG, "Calibrating servos...");
    
    if (!initialized) {
        ESP_LOGE(TAG, "Plane control not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 校准序列：中心 -> 最小 -> 最大 -> 中心
    ESP_LOGI(TAG, "Moving to center position");
    plane_control_set_neutral();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "Moving to minimum position");
    plane_control_params_t min_params = {.throttle = 0, .elevator = -1000, .rudder = -1000, .aileron = -1000};
    plane_control_set_params(&min_params);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "Moving to maximum position");
    plane_control_params_t max_params = {.throttle = 1000, .elevator = 1000, .rudder = 1000, .aileron = 1000};
    plane_control_set_params(&max_params);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "Returning to neutral position");
    plane_control_set_neutral();
    
    ESP_LOGI(TAG, "Servo calibration completed");
    return ESP_OK;
}
