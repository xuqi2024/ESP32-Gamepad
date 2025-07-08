/**
 * @file bluetooth_hid.h
 * @brief 蓝牙HID协议处理头文件
 */

#ifndef BLUETOOTH_HID_H
#define BLUETOOTH_HID_H

#include "esp_err.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief HID设备信息结构体
 */
typedef struct {
    esp_bd_addr_t bda;           ///< 设备蓝牙地址
    char name[64];               ///< 设备名称
    bool connected;              ///< 连接状态
    void *dev_handle;            ///< 设备句柄（通用指针）
} hid_device_info_t;

/**
 * @brief HID输入报告结构体
 */
typedef struct {
    uint8_t *data;               ///< 报告数据
    uint16_t len;                ///< 数据长度
    uint8_t map_index;           ///< 报告映射索引
    uint8_t protocol_mode;       ///< 协议模式
} hid_input_report_t;

/**
 * @brief HID输出报告结构体（用于震动等）
 */
typedef struct {
    uint8_t report_id;           ///< 报告ID
    uint8_t *data;               ///< 报告数据
    uint16_t len;                ///< 数据长度
} hid_output_report_t;

/**
 * @brief HID事件类型
 */
typedef enum {
    HID_EVENT_INIT = 0,
    HID_EVENT_DEINIT,
    HID_EVENT_OPEN,
    HID_EVENT_CLOSE,
    HID_EVENT_DATA,
    HID_EVENT_SET_REPORT,
    HID_EVENT_GET_REPORT
} hid_event_type_t;

/**
 * @brief HID事件参数
 */
typedef struct {
    hid_event_type_t event;
    union {
        struct {
            esp_err_t status;
        } init;
        struct {
            esp_bd_addr_t bd_addr;
            esp_err_t status;
        } open;
        struct {
            esp_err_t status;
        } close;
        struct {
            uint8_t *data;
            uint16_t len;
        } data;
    } param;
} hid_event_param_t;

/**
 * @brief HID事件回调函数类型
 */
typedef void (*hid_event_callback_t)(hid_event_param_t *param);

/**
 * @brief HID输入数据回调函数类型
 */
typedef void (*hid_input_callback_t)(hid_device_info_t *device, hid_input_report_t *report);

/**
 * @brief 初始化蓝牙HID主机
 * @param event_cb HID事件回调函数
 * @param input_cb HID输入数据回调函数
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t bluetooth_hid_init(hid_event_callback_t event_cb, hid_input_callback_t input_cb);

/**
 * @brief 反初始化蓝牙HID主机
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t bluetooth_hid_deinit(void);

/**
 * @brief 开始扫描HID设备
 * @param duration_sec 扫描持续时间（秒）
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t bluetooth_hid_start_scan(uint32_t duration_sec);

/**
 * @brief 停止扫描HID设备
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t bluetooth_hid_stop_scan(void);

/**
 * @brief 连接HID设备
 * @param bda 设备蓝牙地址
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t bluetooth_hid_connect(esp_bd_addr_t bda);

/**
 * @brief 断开HID设备连接
 * @param dev_handle 设备句柄
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t bluetooth_hid_disconnect(void *dev_handle);

/**
 * @brief 发送HID输出报告（震动等）
 * @param dev_handle 设备句柄
 * @param report 输出报告
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t bluetooth_hid_send_output_report(void *dev_handle, hid_output_report_t *report);

/**
 * @brief 获取已连接的HID设备信息
 * @param device_info 输出设备信息
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t bluetooth_hid_get_connected_device(hid_device_info_t *device_info);

/**
 * @brief 检查HID设备是否已连接
 * @return true 已连接，false 未连接
 */
bool bluetooth_hid_is_connected(void);

/**
 * @brief 设置设备可发现模式
 * @param discoverable 是否可发现
 * @param connectable 是否可连接
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t bluetooth_hid_set_discoverable(bool discoverable, bool connectable);

#ifdef __cplusplus
}
#endif

#endif // BLUETOOTH_HID_H
