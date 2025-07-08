/**
 * @file car_control.h
 * @brief 小车控制头文件
 */

#ifndef CAR_CONTROL_H
#define CAR_CONTROL_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 小车控制参数结构体
 */
typedef struct {
    int16_t forward_speed;    ///< 前进速度 (-1000 to 1000)
    int16_t turn_speed;       ///< 转向速度 (-1000 to 1000)
    bool brake_enable;        ///< 刹车使能
} car_control_params_t;

/**
 * @brief 小车电机配置结构体
 */
typedef struct {
    int left_motor_pwm_pin;   ///< 左电机PWM引脚
    int left_motor_dir1_pin;  ///< 左电机方向引脚1
    int left_motor_dir2_pin;  ///< 左电机方向引脚2
    int right_motor_pwm_pin;  ///< 右电机PWM引脚
    int right_motor_dir1_pin; ///< 右电机方向引脚1
    int right_motor_dir2_pin; ///< 右电机方向引脚2
    uint32_t pwm_frequency;   ///< PWM频率
} car_motor_config_t;

/**
 * @brief 初始化小车控制
 * @param config 电机配置参数
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t car_control_init(const car_motor_config_t *config);

/**
 * @brief 反初始化小车控制
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t car_control_deinit(void);

/**
 * @brief 设置小车运动参数
 * @param params 控制参数
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t car_control_set_motion(const car_control_params_t *params);

/**
 * @brief 停止小车运动
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t car_control_stop(void);

/**
 * @brief 设置小车制动
 * @param enable 是否启用制动
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t car_control_brake(bool enable);

/**
 * @brief 获取小车当前状态
 * @param params 输出当前控制参数
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t car_control_get_status(car_control_params_t *params);

#ifdef __cplusplus
}
#endif

#endif // CAR_CONTROL_H
