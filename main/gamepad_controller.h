/**
 * @file gamepad_controller.h
 * @brief 游戏手柄控制器头文件
 */

#ifndef GAMEPAD_CONTROLLER_H
#define GAMEPAD_CONTROLLER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 控制模式枚举
 */
typedef enum {
    CONTROL_MODE_CAR = 0,    ///< 小车控制模式
    CONTROL_MODE_PLANE,      ///< 飞机控制模式
    CONTROL_MODE_DISABLED    ///< 禁用模式
} control_mode_t;

/**
 * @brief 手柄按键状态结构体
 */
typedef struct {
    bool button_a;           ///< A键状态
    bool button_b;           ///< B键状态
    bool button_x;           ///< X键状态
    bool button_y;           ///< Y键状态
    bool button_l1;          ///< L1键状态
    bool button_r1;          ///< R1键状态
    bool button_l2;          ///< L2键状态
    bool button_r2;          ///< R2键状态
    bool button_select;      ///< 选择键状态
    bool button_start;       ///< 开始键状态
    bool button_home;        ///< Home键状态
    bool dpad_up;           ///< 方向键上
    bool dpad_down;         ///< 方向键下
    bool dpad_left;         ///< 方向键左
    bool dpad_right;        ///< 方向键右
} gamepad_buttons_t;

/**
 * @brief 手柄摇杆状态结构体
 */
typedef struct {
    int16_t left_x;          ///< 左摇杆X轴 (-32768 to 32767)
    int16_t left_y;          ///< 左摇杆Y轴 (-32768 to 32767)
    int16_t right_x;         ///< 右摇杆X轴 (-32768 to 32767)
    int16_t right_y;         ///< 右摇杆Y轴 (-32768 to 32767)
    uint8_t left_trigger;    ///< 左扳机 (0 to 255)
    uint8_t right_trigger;   ///< 右扳机 (0 to 255)
} gamepad_sticks_t;

/**
 * @brief 完整的手柄状态结构体
 */
typedef struct {
    gamepad_buttons_t buttons;
    gamepad_sticks_t sticks;
    bool connected;          ///< 连接状态
    uint32_t last_update;    ///< 最后更新时间戳
} gamepad_state_t;

/**
 * @brief 震动参数结构体
 */
typedef struct {
    uint8_t left_motor;      ///< 左侧震动马达强度 (0-255)
    uint8_t right_motor;     ///< 右侧震动马达强度 (0-255)
    uint32_t duration_ms;    ///< 震动持续时间(毫秒)
} vibration_params_t;

/**
 * @brief 初始化游戏手柄控制器
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t gamepad_controller_init(void);

/**
 * @brief 设置控制模式
 * @param mode 控制模式
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t gamepad_controller_set_mode(control_mode_t mode);

/**
 * @brief 获取当前控制模式
 * @return 当前控制模式
 */
control_mode_t gamepad_controller_get_mode(void);

/**
 * @brief 获取当前手柄状态
 * @param state 输出的手柄状态
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t gamepad_controller_get_state(gamepad_state_t *state);

/**
 * @brief 发送震动反馈
 * @param params 震动参数
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t gamepad_controller_vibrate(const vibration_params_t *params);

/**
 * @brief 停止震动
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t gamepad_controller_stop_vibration(void);

/**
 * @brief 检查手柄连接状态
 * @return true 已连接，false 未连接
 */
bool gamepad_controller_is_connected(void);

/**
 * @brief 获取手柄电池电量（如果支持）
 * @return 电池电量百分比 (0-100)，-1表示不支持
 */
int8_t gamepad_controller_get_battery_level(void);

#ifdef __cplusplus
}
#endif

#endif // GAMEPAD_CONTROLLER_H
