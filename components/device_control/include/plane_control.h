/**
 * @file plane_control.h
 * @brief 飞机控制头文件
 */

#ifndef PLANE_CONTROL_H
#define PLANE_CONTROL_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 飞机控制参数结构体
 */
typedef struct {
    int16_t throttle;         ///< 油门 (0 to 1000)
    int16_t elevator;         ///< 升降舵 (-1000 to 1000)
    int16_t rudder;           ///< 方向舵 (-1000 to 1000)
    int16_t aileron;          ///< 副翼 (-1000 to 1000)
} plane_control_params_t;

/**
 * @brief 飞机舵机配置结构体
 */
typedef struct {
    int throttle_pin;         ///< 油门舵机引脚
    int elevator_pin;         ///< 升降舵机引脚
    int rudder_pin;           ///< 方向舵机引脚
    int aileron_pin;          ///< 副翼舵机引脚
    uint32_t pwm_frequency;   ///< PWM频率
    uint16_t servo_min_us;    ///< 舵机最小脉宽(微秒)
    uint16_t servo_max_us;    ///< 舵机最大脉宽(微秒)
    uint16_t servo_center_us; ///< 舵机中心脉宽(微秒)
} plane_servo_config_t;

/**
 * @brief 初始化飞机控制
 * @param config 舵机配置参数
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t plane_control_init(const plane_servo_config_t *config);

/**
 * @brief 反初始化飞机控制
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t plane_control_deinit(void);

/**
 * @brief 设置飞机控制参数
 * @param params 控制参数
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t plane_control_set_params(const plane_control_params_t *params);

/**
 * @brief 设置飞机到中性位置
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t plane_control_set_neutral(void);

/**
 * @brief 紧急停止（油门归零）
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t plane_control_emergency_stop(void);

/**
 * @brief 获取飞机当前状态
 * @param params 输出当前控制参数
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t plane_control_get_status(plane_control_params_t *params);

/**
 * @brief 校准舵机中心位置
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t plane_control_calibrate_servos(void);

#ifdef __cplusplus
}
#endif

#endif // PLANE_CONTROL_H
