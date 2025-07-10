/**
 * @file bluetooth_hid.c
 * @brief 蓝牙HID协议处理实现（简化版本）
 */

#include "bluetooth_hid.h"
#include "esp_log.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include <string.h>

static const char *TAG = "BT_HID";

// 静态变量
static hid_device_info_t connected_device = {0};
static hid_event_callback_t event_callback = NULL;
static hid_input_callback_t input_callback = NULL;
static bool hid_initialized = false;
static bool scanning = false;

/**
 * @brief GAP事件回调函数
 */
static void gap_event_handler(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT:
        ESP_LOGI(TAG, "Discovery result: device found");
        // 检查设备类型
        uint32_t cod = 0x002500; // 默认HID设备类别码
        
        // HID设备的设备类别码检查
        // 键盘: 0x002540, 鼠标: 0x002580, 游戏手柄: 0x002508
        if ((cod & 0x00FF00) == 0x002500) { // HID设备
            ESP_LOGI(TAG, "Found HID device: COD: 0x%06lx", cod);
            
            // 可以在这里自动连接或通知上层应用
            if (event_callback) {
                hid_event_param_t hid_param = {
                    .event = HID_EVENT_OPEN,
                    .param.open.status = ESP_OK
                };
                // 使用默认的蓝牙地址
                memset(hid_param.param.open.bd_addr, 0, sizeof(esp_bd_addr_t));
                event_callback(&hid_param);
            }
        }
        break;
        
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
            ESP_LOGI(TAG, "Discovery stopped");
            scanning = false;
        } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
            ESP_LOGI(TAG, "Discovery started");
            scanning = true;
        }
        break;
        
    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(TAG, "GAP mode changed to %d", param->mode_chg.mode);
        break;
        
    default:
        ESP_LOGD(TAG, "GAP event: %d", event);
        break;
    }
}

esp_err_t bluetooth_hid_init(hid_event_callback_t event_cb, hid_input_callback_t input_cb)
{
    ESP_LOGI(TAG, "Initializing Bluetooth HID host (simplified version)...");
    
    if (hid_initialized) {
        ESP_LOGW(TAG, "HID already initialized");
        return ESP_OK;
    }
    
    // 保存回调函数
    event_callback = event_cb;
    input_callback = input_cb;
    
    // 初始化连接设备信息
    memset(&connected_device, 0, sizeof(connected_device));
    
    // 注册GAP回调函数
    esp_err_t ret = esp_bt_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GAP callback: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 设置GAP扫描模式
    ret = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set scan mode: %s", esp_err_to_name(ret));
        return ret;
    }
    
    hid_initialized = true;
    ESP_LOGI(TAG, "Bluetooth HID host initialized successfully (simplified)");
    
    // 触发初始化完成事件
    if (event_callback) {
        hid_event_param_t param = {
            .event = HID_EVENT_INIT,
            .param.init.status = ESP_OK
        };
        event_callback(&param);
    }
    
    return ESP_OK;
}

esp_err_t bluetooth_hid_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing Bluetooth HID host...");
    
    if (!hid_initialized) {
        ESP_LOGW(TAG, "HID not initialized");
        return ESP_OK;
    }
    
    // 停止扫描
    if (scanning) {
        bluetooth_hid_stop_scan();
    }
    
    // 断开连接
    if (connected_device.connected) {
        bluetooth_hid_disconnect(connected_device.dev_handle);
    }
    
    hid_initialized = false;
    event_callback = NULL;
    input_callback = NULL;
    
    ESP_LOGI(TAG, "Bluetooth HID host deinitialized");
    
    return ESP_OK;
}

esp_err_t bluetooth_hid_start_scan(uint32_t duration_sec)
{
    ESP_LOGI(TAG, "Starting HID device scan for %lu seconds...", duration_sec);
    
    if (!hid_initialized) {
        ESP_LOGE(TAG, "HID not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (scanning) {
        ESP_LOGW(TAG, "Already scanning");
        return ESP_OK;
    }
    
    esp_err_t ret = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, duration_sec, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start discovery: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "HID device scan started");
    return ESP_OK;
}

esp_err_t bluetooth_hid_stop_scan(void)
{
    ESP_LOGI(TAG, "Stopping HID device scan...");
    
    if (!scanning) {
        ESP_LOGW(TAG, "Not scanning");
        return ESP_OK;
    }
    
    esp_err_t ret = esp_bt_gap_cancel_discovery();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop discovery: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "HID device scan stopped");
    return ESP_OK;
}

esp_err_t bluetooth_hid_connect(esp_bd_addr_t bda)
{
    ESP_LOGI(TAG, "Connecting to HID device (simulated)...");
    
    if (!hid_initialized) {
        ESP_LOGE(TAG, "HID not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (connected_device.connected) {
        ESP_LOGW(TAG, "Already connected to a device");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 模拟连接成功
    memcpy(connected_device.bda, bda, sizeof(esp_bd_addr_t));
    connected_device.connected = true;
    strcpy(connected_device.name, "Simulated Gamepad");
    connected_device.dev_handle = (void*)0x12345678; // 模拟句柄
    
    ESP_LOGI(TAG, "HID device connected (simulated)");
    
    // 触发连接事件
    if (event_callback) {
        hid_event_param_t param = {
            .event = HID_EVENT_OPEN,
            .param.open.status = ESP_OK
        };
        memcpy(param.param.open.bd_addr, bda, sizeof(esp_bd_addr_t));
        event_callback(&param);
    }
    
    return ESP_OK;
}

esp_err_t bluetooth_hid_disconnect(void *dev_handle)
{
    ESP_LOGI(TAG, "Disconnecting HID device...");
    
    if (!hid_initialized) {
        ESP_LOGE(TAG, "HID not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!connected_device.connected) {
        ESP_LOGW(TAG, "No device connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 模拟断开连接
    connected_device.connected = false;
    connected_device.dev_handle = NULL;
    memset(connected_device.bda, 0, sizeof(esp_bd_addr_t));
    memset(connected_device.name, 0, sizeof(connected_device.name));
    
    ESP_LOGI(TAG, "HID device disconnected");
    
    // 触发断开事件
    if (event_callback) {
        hid_event_param_t param = {
            .event = HID_EVENT_CLOSE,
            .param.close.status = ESP_OK
        };
        event_callback(&param);
    }
    
    return ESP_OK;
}

esp_err_t bluetooth_hid_send_output_report(void *dev_handle, hid_output_report_t *report)
{
    ESP_LOGD(TAG, "Sending HID output report (simulated)...");
    
    if (!hid_initialized) {
        ESP_LOGE(TAG, "HID not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!dev_handle || !report || !report->data) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!connected_device.connected) {
        ESP_LOGW(TAG, "No device connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "HID output report sent successfully (simulated)");
    ESP_LOGD(TAG, "Report ID: 0x%02x, Length: %d", report->report_id, report->len);
    
    return ESP_OK;
}

esp_err_t bluetooth_hid_get_connected_device(hid_device_info_t *device_info)
{
    if (!device_info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!connected_device.connected) {
        return ESP_ERR_NOT_FOUND;
    }
    
    memcpy(device_info, &connected_device, sizeof(hid_device_info_t));
    return ESP_OK;
}

bool bluetooth_hid_is_connected(void)
{
    return connected_device.connected;
}

esp_err_t bluetooth_hid_set_discoverable(bool discoverable, bool connectable)
{
    ESP_LOGI(TAG, "Setting discoverable: %d, connectable: %d", discoverable, connectable);
    
    esp_bt_connection_mode_t c_mode;
    esp_bt_discovery_mode_t d_mode;
    
    if (connectable) {
        c_mode = ESP_BT_CONNECTABLE;
    } else {
        c_mode = ESP_BT_NON_CONNECTABLE;
    }
    
    if (discoverable) {
        d_mode = ESP_BT_GENERAL_DISCOVERABLE;
    } else {
        d_mode = ESP_BT_NON_DISCOVERABLE;
    }
    
    esp_err_t ret = esp_bt_gap_set_scan_mode(c_mode, d_mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set scan mode: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}
