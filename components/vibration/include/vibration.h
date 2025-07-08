/**
 * @file vibration.h
 * @brief 震动反馈控制头文件
 */

#ifndef VIBRATION_H
#define VIBRATION_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 震动模式枚举
 */
typedef enum {
    VIBRATION_MODE_PULSE = 0,    ///< 脉冲模式
    VIBRATION_MODE_CONTINUOUS,   ///< 连续模式
    VIBRATION_MODE_PATTERN,      ///< 模式模式
    VIBRATION_MODE_FEEDBACK      ///< 反馈模式
} vibration_mode_t;

/**
 * @brief 震动参数结构体
 */
typedef struct {
    uint8_t left_intensity;      ///< 左侧震动强度 (0-255)
    uint8_t right_intensity;     ///< 右侧震动强度 (0-255)
    uint32_t duration_ms;        ///< 持续时间(毫秒)
    vibration_mode_t mode;       ///< 震动模式
    uint16_t pulse_count;        ///< 脉冲次数(脉冲模式使用)
    uint16_t pulse_interval_ms;  ///< 脉冲间隔(毫秒)
} vibration_params_t;

/**
 * @brief 震动模式结构体
 */
typedef struct {
    uint8_t *pattern_data;       ///< 模式数据
    uint16_t pattern_length;     ///< 模式长度
    uint16_t step_duration_ms;   ///< 每步持续时间
    bool repeat;                 ///< 是否重复
} vibration_pattern_t;

/**
 * @brief 震动状态结构体
 */
typedef struct {
    bool active;                 ///< 是否激活
    vibration_params_t params;   ///< 当前参数
    uint32_t start_time;         ///< 开始时间
    uint32_t remaining_time;     ///< 剩余时间
} vibration_status_t;

/**
 * @brief 初始化震动控制
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t vibration_init(void);

/**
 * @brief 反初始化震动控制
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t vibration_deinit(void);

/**
 * @brief 开始震动
 * @param params 震动参数
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t vibration_start(const vibration_params_t *params);

/**
 * @brief 停止震动
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t vibration_stop(void);

/**
 * @brief 设置震动模式
 * @param pattern 震动模式参数
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t vibration_set_pattern(const vibration_pattern_t *pattern);

/**
 * @brief 快速震动（预设模式）
 * @param intensity 震动强度 (0-255)
 * @param duration_ms 持续时间(毫秒)
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t vibration_quick_pulse(uint8_t intensity, uint32_t duration_ms);

/**
 * @brief 双侧震动
 * @param left_intensity 左侧强度 (0-255)
 * @param right_intensity 右侧强度 (0-255)
 * @param duration_ms 持续时间(毫秒)
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t vibration_dual_motor(uint8_t left_intensity, uint8_t right_intensity, uint32_t duration_ms);

/**
 * @brief 获取震动状态
 * @param status 输出状态信息
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t vibration_get_status(vibration_status_t *status);

/**
 * @brief 检查震动是否激活
 * @return true 激活，false 未激活
 */
bool vibration_is_active(void);

/**
 * @brief 设置全局震动使能
 * @param enable 是否使能震动
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t vibration_set_enable(bool enable);

/**
 * @brief 获取震动使能状态
 * @return true 使能，false 禁用
 */
bool vibration_is_enabled(void);

// 预定义震动模式
#define VIBRATION_PATTERN_CLICK      {.left_intensity = 100, .right_intensity = 100, .duration_ms = 50, .mode = VIBRATION_MODE_PULSE}
#define VIBRATION_PATTERN_ERROR      {.left_intensity = 255, .right_intensity = 255, .duration_ms = 200, .mode = VIBRATION_MODE_PULSE}
#define VIBRATION_PATTERN_SUCCESS    {.left_intensity = 150, .right_intensity = 150, .duration_ms = 100, .mode = VIBRATION_MODE_PULSE}
#define VIBRATION_PATTERN_WARNING    {.left_intensity = 200, .right_intensity = 0, .duration_ms = 150, .mode = VIBRATION_MODE_PULSE}

#ifdef __cplusplus
}
#endif

#endif // VIBRATION_H
