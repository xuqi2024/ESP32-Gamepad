/**
 * @file car_control.c
 * @brief 小车控制实现
 */

#include "car_control.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include <string.h>

static const char *TAG = "CAR_CTRL";

// 静态变量
static car_motor_config_t motor_config = {0};
static car_control_params_t current_params = {0};
static bool initialized = false;

// LEDC配置
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_LEFT_CHANNEL       LEDC_CHANNEL_0
#define LEDC_RIGHT_CHANNEL      LEDC_CHANNEL_1
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT
#define LEDC_DUTY_MAX           (8191) // 13位分辨率最大值

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
        .freq_hz          = motor_config.pwm_frequency,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 配置左电机PWM通道
    ledc_channel_config_t left_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_LEFT_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = motor_config.left_motor_pwm_pin,
        .duty           = 0,
        .hpoint         = 0
    };
    
    ret = ledc_channel_config(&left_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure left PWM channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 配置右电机PWM通道
    ledc_channel_config_t right_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_RIGHT_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = motor_config.right_motor_pwm_pin,
        .duty           = 0,
        .hpoint         = 0
    };
    
    ret = ledc_channel_config(&right_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure right PWM channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "PWM initialized successfully");
    return ESP_OK;
}

/**
 * @brief 初始化GPIO
 */
static esp_err_t init_gpio(void)
{
    // 配置方向控制引脚
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = ((1ULL << motor_config.left_motor_dir1_pin) |
                        (1ULL << motor_config.left_motor_dir2_pin) |
                        (1ULL << motor_config.right_motor_dir1_pin) |
                        (1ULL << motor_config.right_motor_dir2_pin)),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 初始化为停止状态
    gpio_set_level(motor_config.left_motor_dir1_pin, 0);
    gpio_set_level(motor_config.left_motor_dir2_pin, 0);
    gpio_set_level(motor_config.right_motor_dir1_pin, 0);
    gpio_set_level(motor_config.right_motor_dir2_pin, 0);
    
    ESP_LOGI(TAG, "GPIO initialized successfully");
    return ESP_OK;
}

/**
 * @brief 设置电机方向和速度
 */
static esp_err_t set_motor_speed(ledc_channel_t channel, int gpio_dir1, int gpio_dir2, int16_t speed)
{
    uint32_t duty = 0;
    
    // 计算PWM占空比
    if (speed > 0) {
        // 正转
        gpio_set_level(gpio_dir1, 1);
        gpio_set_level(gpio_dir2, 0);
        duty = (uint32_t)(speed * LEDC_DUTY_MAX / 1000);
    } else if (speed < 0) {
        // 反转
        gpio_set_level(gpio_dir1, 0);
        gpio_set_level(gpio_dir2, 1);
        duty = (uint32_t)((-speed) * LEDC_DUTY_MAX / 1000);
    } else {
        // 停止
        gpio_set_level(gpio_dir1, 0);
        gpio_set_level(gpio_dir2, 0);
        duty = 0;
    }
    
    // 设置PWM占空比
    esp_err_t ret = ledc_set_duty(LEDC_MODE, channel, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set PWM duty: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = ledc_update_duty(LEDC_MODE, channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update PWM duty: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t car_control_init(const car_motor_config_t *config)
{
    ESP_LOGI(TAG, "Initializing car control...");
    
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (initialized) {
        ESP_LOGW(TAG, "Car control already initialized");
        return ESP_OK;
    }
    
    // 保存配置
    memcpy(&motor_config, config, sizeof(car_motor_config_t));
    
    // 初始化PWM
    esp_err_t ret = init_pwm();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 初始化GPIO
    ret = init_gpio();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 初始化状态
    memset(&current_params, 0, sizeof(current_params));
    initialized = true;
    
    ESP_LOGI(TAG, "Car control initialized successfully");
    ESP_LOGI(TAG, "Left motor: PWM=%d, DIR1=%d, DIR2=%d", 
             motor_config.left_motor_pwm_pin, 
             motor_config.left_motor_dir1_pin, 
             motor_config.left_motor_dir2_pin);
    ESP_LOGI(TAG, "Right motor: PWM=%d, DIR1=%d, DIR2=%d", 
             motor_config.right_motor_pwm_pin, 
             motor_config.right_motor_dir1_pin, 
             motor_config.right_motor_dir2_pin);
    
    return ESP_OK;
}

esp_err_t car_control_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing car control...");
    
    if (!initialized) {
        ESP_LOGW(TAG, "Car control not initialized");
        return ESP_OK;
    }
    
    // 停止所有电机
    car_control_stop();
    
    // 重置PWM通道
    ledc_stop(LEDC_MODE, LEDC_LEFT_CHANNEL, 0);
    ledc_stop(LEDC_MODE, LEDC_RIGHT_CHANNEL, 0);
    
    initialized = false;
    ESP_LOGI(TAG, "Car control deinitialized");
    
    return ESP_OK;
}

esp_err_t car_control_set_motion(const car_control_params_t *params)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Car control not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!params) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 限制速度范围
    int16_t forward_speed = params->forward_speed;
    int16_t turn_speed = params->turn_speed;
    
    if (forward_speed > 1000) forward_speed = 1000;
    if (forward_speed < -1000) forward_speed = -1000;
    if (turn_speed > 1000) turn_speed = 1000;
    if (turn_speed < -1000) turn_speed = -1000;
    
    // 计算左右电机速度
    int16_t left_speed = forward_speed - turn_speed;
    int16_t right_speed = forward_speed + turn_speed;
    
    // 限制电机速度
    if (left_speed > 1000) left_speed = 1000;
    if (left_speed < -1000) left_speed = -1000;
    if (right_speed > 1000) right_speed = 1000;
    if (right_speed < -1000) right_speed = -1000;
    
    ESP_LOGD(TAG, "Setting motion: forward=%d, turn=%d, left=%d, right=%d", 
             forward_speed, turn_speed, left_speed, right_speed);
    
    // 设置电机速度
    esp_err_t ret = set_motor_speed(LEDC_LEFT_CHANNEL, 
                                   motor_config.left_motor_dir1_pin, 
                                   motor_config.left_motor_dir2_pin, 
                                   left_speed);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = set_motor_speed(LEDC_RIGHT_CHANNEL, 
                         motor_config.right_motor_dir1_pin, 
                         motor_config.right_motor_dir2_pin, 
                         right_speed);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 更新当前状态
    current_params.forward_speed = forward_speed;
    current_params.turn_speed = turn_speed;
    current_params.brake_enable = params->brake_enable;
    
    return ESP_OK;
}

esp_err_t car_control_stop(void)
{
    ESP_LOGI(TAG, "Stopping car");
    
    car_control_params_t stop_params = {0};
    return car_control_set_motion(&stop_params);
}

esp_err_t car_control_brake(bool enable)
{
    ESP_LOGI(TAG, "Setting brake: %s", enable ? "ON" : "OFF");
    
    if (!initialized) {
        ESP_LOGE(TAG, "Car control not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (enable) {
        // 刹车：所有方向引脚设置为高，产生反向电动势
        gpio_set_level(motor_config.left_motor_dir1_pin, 1);
        gpio_set_level(motor_config.left_motor_dir2_pin, 1);
        gpio_set_level(motor_config.right_motor_dir1_pin, 1);
        gpio_set_level(motor_config.right_motor_dir2_pin, 1);
        
        // 设置PWM为最大占空比
        ledc_set_duty(LEDC_MODE, LEDC_LEFT_CHANNEL, LEDC_DUTY_MAX);
        ledc_set_duty(LEDC_MODE, LEDC_RIGHT_CHANNEL, LEDC_DUTY_MAX);
        ledc_update_duty(LEDC_MODE, LEDC_LEFT_CHANNEL);
        ledc_update_duty(LEDC_MODE, LEDC_RIGHT_CHANNEL);
    } else {
        // 取消刹车
        car_control_stop();
    }
    
    current_params.brake_enable = enable;
    return ESP_OK;
}

esp_err_t car_control_get_status(car_control_params_t *params)
{
    if (!params) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!initialized) {
        ESP_LOGE(TAG, "Car control not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    memcpy(params, &current_params, sizeof(car_control_params_t));
    return ESP_OK;
}
