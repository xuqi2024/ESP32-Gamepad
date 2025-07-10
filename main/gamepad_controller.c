/**
 * @file gamepad_controller.c
 * @brief 游戏手柄控制器实现
 */

#include "gamepad_controller.h"
#include "bluetooth_hid.h"
#include "car_control.h"
#include "plane_control.h"
#include "vibration.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <inttypes.h>
#include <string.h>

static const char *TAG = "GAMEPAD_CTRL";

// 函数声明
static void parse_gamepad_input(const uint8_t *data, uint16_t len);

// 任务参数
#define GAMEPAD_INPUT_TASK_STACK_SIZE   4096
#define GAMEPAD_INPUT_TASK_PRIORITY     10
#define CONTROL_OUTPUT_TASK_STACK_SIZE  4096
#define CONTROL_OUTPUT_TASK_PRIORITY    9

// 更新频率
#define GAMEPAD_UPDATE_INTERVAL_MS      10   // 100Hz
#define CONTROL_UPDATE_INTERVAL_MS      20   // 50Hz

// 静态变量
static control_mode_t current_mode = CONTROL_MODE_DISABLED;
static gamepad_state_t current_state = {0};
static SemaphoreHandle_t state_mutex = NULL;
static TaskHandle_t input_task_handle = NULL;
static TaskHandle_t output_task_handle = NULL;

// 默认配置
static car_motor_config_t default_car_config = {
    .left_motor_pwm_pin = 18,
    .left_motor_dir1_pin = 19,
    .left_motor_dir2_pin = 21,
    .right_motor_pwm_pin = 22,
    .right_motor_dir1_pin = 23,
    .right_motor_dir2_pin = 25,
    .pwm_frequency = 1000
};

static plane_servo_config_t default_plane_config = {
    .throttle_pin = 26,
    .elevator_pin = 27,
    .rudder_pin = 14,
    .aileron_pin = 12,
    .pwm_frequency = 50,
    .servo_min_us = 1000,
    .servo_max_us = 2000,
    .servo_center_us = 1500
};

/**
 * @brief HID事件回调函数
 */
static void hid_event_callback(hid_event_param_t *param)
{
    switch (param->event) {
    case HID_EVENT_INIT:
        if (param->param.init.status == ESP_OK) {
            ESP_LOGI(TAG, "HID Host initialized successfully");
        } else {
            ESP_LOGE(TAG, "HID Host initialization failed");
        }
        break;
        
    case HID_EVENT_OPEN:
        if (param->param.open.status == ESP_OK) {
            ESP_LOGI(TAG, "HID device connected successfully");
            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                current_state.connected = true;
                xSemaphoreGive(state_mutex);
            }
            
            // 连接成功震动反馈
            vibration_quick_pulse(150, 200);
        } else {
            ESP_LOGE(TAG, "HID device connection failed");
        }
        break;
        
    case HID_EVENT_CLOSE:
        ESP_LOGI(TAG, "HID device disconnected");
        if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            current_state.connected = false;
            memset(&current_state.buttons, 0, sizeof(current_state.buttons));
            memset(&current_state.sticks, 0, sizeof(current_state.sticks));
            xSemaphoreGive(state_mutex);
        }
        break;
        
    case HID_EVENT_DATA:
        ESP_LOGD(TAG, "HID data received: len=%d", param->param.data.len);
        if (param->param.data.data && param->param.data.len > 0) {
            parse_gamepad_input(param->param.data.data, param->param.data.len);
        }
        break;
        
    default:
        ESP_LOGD(TAG, "HID event: %d", param->event);
        break;
    }
}

/**
 * @brief 解析通用手柄输入数据
 */
static void parse_gamepad_input(const uint8_t *data, uint16_t len)
{
    if (len < 8) {
        ESP_LOGW(TAG, "Input data too short: %d bytes", len);
        return;
    }
    
    // 这是一个通用的解析示例，实际格式取决于具体手柄
    // 字节0-1: 按键状态
    // 字节2-3: 左摇杆X,Y
    // 字节4-5: 右摇杆X,Y
    // 字节6-7: 扳机值
    
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        // 解析按键
        uint16_t buttons = (data[1] << 8) | data[0];
        current_state.buttons.button_a = (buttons & 0x0001) != 0;
        current_state.buttons.button_b = (buttons & 0x0002) != 0;
        current_state.buttons.button_x = (buttons & 0x0004) != 0;
        current_state.buttons.button_y = (buttons & 0x0008) != 0;
        current_state.buttons.button_l1 = (buttons & 0x0010) != 0;
        current_state.buttons.button_r1 = (buttons & 0x0020) != 0;
        current_state.buttons.button_select = (buttons & 0x0040) != 0;
        current_state.buttons.button_start = (buttons & 0x0080) != 0;
        
        // 解析摇杆 (转换为-32768到32767范围)
        current_state.sticks.left_x = (int16_t)((data[2] - 128) * 256);
        current_state.sticks.left_y = (int16_t)((data[3] - 128) * 256);
        current_state.sticks.right_x = (int16_t)((data[4] - 128) * 256);
        current_state.sticks.right_y = (int16_t)((data[5] - 128) * 256);
        
        // 解析扳机
        current_state.sticks.left_trigger = data[6];
        current_state.sticks.right_trigger = data[7];
        
        // 更新时间戳
        current_state.last_update = esp_timer_get_time() / 1000;
        
        xSemaphoreGive(state_mutex);
        
        ESP_LOGD(TAG, "Gamepad input: LX=%d, LY=%d, RX=%d, RY=%d, Buttons=0x%04x", 
                 current_state.sticks.left_x, current_state.sticks.left_y,
                 current_state.sticks.right_x, current_state.sticks.right_y, buttons);
    }
}

/**
 * @brief HID输入数据回调函数
 */
static void hid_input_callback(hid_device_info_t *device, hid_input_report_t *report)
{
    ESP_LOGD(TAG, "HID input received: len=%d", report->len);
    
    if (report->data && report->len > 0) {
        parse_gamepad_input(report->data, report->len);
    }
}
/**
 * @brief 手柄输入处理任务
 */
static void gamepad_input_task(void *parameter)
{
    ESP_LOGI(TAG, "Gamepad input task started");
    
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        // 检查蓝牙连接状态
        if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            // 更新连接状态
            current_state.connected = bluetooth_hid_is_connected();
            
            // 更新时间戳
            if (current_state.connected) {
                current_state.last_update = esp_timer_get_time() / 1000;
            }
            
            xSemaphoreGive(state_mutex);
        }
        
        // 精确的任务调度
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(GAMEPAD_UPDATE_INTERVAL_MS));
    }
}

/**
 * @brief 控制输出处理任务
 */
/**
 * @brief 控制输出处理任务
 */
static void control_output_task(void *parameter)
{
    ESP_LOGI(TAG, "Control output task started");
    
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        gamepad_state_t state;
        if (gamepad_controller_get_state(&state) == ESP_OK && state.connected) {
            
            switch (current_mode) {
                case CONTROL_MODE_CAR:
                    {
                        // 小车控制逻辑
                        car_control_params_t car_params;
                        
                        // 左摇杆控制前进/后退和转向
                        car_params.forward_speed = -state.sticks.left_y / 32;  // 转换到-1000到1000
                        car_params.turn_speed = state.sticks.left_x / 32;      // 转换到-1000到1000
                        car_params.brake_enable = state.buttons.button_b;      // B键刹车
                        
                        car_control_set_motion(&car_params);
                        
                        // 转向时的震动反馈
                        if (abs(car_params.turn_speed) > 500) {
                            vibration_dual_motor(50, 50, 50);
                        }
                        
                        ESP_LOGD(TAG, "Car control: forward=%d, turn=%d, brake=%d", 
                                 car_params.forward_speed, car_params.turn_speed, car_params.brake_enable);
                    }
                    break;
                    
                case CONTROL_MODE_PLANE:
                    {
                        // 飞机控制逻辑
                        plane_control_params_t plane_params;
                        
                        // 右扳机控制油门 (0-1000)
                        plane_params.throttle = state.sticks.right_trigger * 4; // 转换到0-1000
                        
                        // 左摇杆控制升降舵和副翼
                        plane_params.elevator = -state.sticks.left_y / 32;  // 转换到-1000到1000
                        plane_params.aileron = state.sticks.left_x / 32;    // 转换到-1000到1000
                        
                        // 右摇杆X轴控制方向舵
                        plane_params.rudder = state.sticks.right_x / 32;    // 转换到-1000到1000
                        
                        plane_control_set_params(&plane_params);
                        
                        // 紧急停止
                        if (state.buttons.button_y) {
                            plane_control_emergency_stop();
                            vibration_quick_pulse(255, 500); // 紧急停止震动提示
                        }
                        
                        ESP_LOGD(TAG, "Plane control: throttle=%d, elevator=%d, rudder=%d, aileron=%d", 
                                 plane_params.throttle, plane_params.elevator, 
                                 plane_params.rudder, plane_params.aileron);
                    }
                    break;
                    
                case CONTROL_MODE_DISABLED:
                default:
                    // 停止所有输出
                    ESP_LOGD(TAG, "Control disabled");
                    break;
            }
            
            // 模式切换检测
            if (state.buttons.button_select) {
                // Select键切换模式
                control_mode_t new_mode = (current_mode + 1) % 3;
                gamepad_controller_set_mode(new_mode);
                vibration_quick_pulse(100, 100); // 模式切换提示
            }
            
        } else {
            // 手柄未连接或获取状态失败
            ESP_LOGD(TAG, "Gamepad not connected");
            
            // 确保所有输出都停止
            if (current_mode == CONTROL_MODE_CAR) {
                car_control_stop();
            } else if (current_mode == CONTROL_MODE_PLANE) {
                plane_control_set_neutral();
            }
        }
        
        // 精确的任务调度
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(CONTROL_UPDATE_INTERVAL_MS));
    }
}

/**
 * @brief 初始化状态结构体
 */
static void init_gamepad_state(void)
{
    memset(&current_state, 0, sizeof(current_state));
    current_state.connected = false;
    current_state.last_update = 0;
}

esp_err_t gamepad_controller_init(void)
{
    ESP_LOGI(TAG, "Initializing gamepad controller...");
    
    // 检查是否已经初始化
    if (state_mutex != NULL) {
        ESP_LOGW(TAG, "Gamepad controller already initialized");
        return ESP_OK;
    }
    
    // 创建互斥锁
    state_mutex = xSemaphoreCreateMutex();
    if (state_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create state mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // 初始化状态
    init_gamepad_state();
    current_mode = CONTROL_MODE_DISABLED;
    
    // 初始化蓝牙HID
    esp_err_t ret = bluetooth_hid_init(hid_event_callback, hid_input_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Bluetooth HID: %s", esp_err_to_name(ret));
        vSemaphoreDelete(state_mutex);
        state_mutex = NULL;
        return ret;
    }
    
    // 初始化震动控制
    ret = vibration_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize vibration: %s", esp_err_to_name(ret));
        bluetooth_hid_deinit();
        vSemaphoreDelete(state_mutex);
        state_mutex = NULL;
        return ret;
    }
    
    // 初始化小车控制
    ret = car_control_init(&default_car_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize car control: %s", esp_err_to_name(ret));
        vibration_deinit();
        bluetooth_hid_deinit();
        vSemaphoreDelete(state_mutex);
        state_mutex = NULL;
        return ret;
    }
    
    // 初始化飞机控制
    ret = plane_control_init(&default_plane_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize plane control: %s", esp_err_to_name(ret));
        car_control_deinit();
        vibration_deinit();
        bluetooth_hid_deinit();
        vSemaphoreDelete(state_mutex);
        state_mutex = NULL;
        return ret;
    }
    
    // 开始扫描手柄
    ret = bluetooth_hid_start_scan(30); // 扫描30秒
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start HID scan: %s", esp_err_to_name(ret));
    }
    
    // 创建输入处理任务
    BaseType_t task_ret = xTaskCreate(
        gamepad_input_task,
        "gamepad_input",
        GAMEPAD_INPUT_TASK_STACK_SIZE,
        NULL,
        GAMEPAD_INPUT_TASK_PRIORITY,
        &input_task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create gamepad input task");
        plane_control_deinit();
        car_control_deinit();
        vibration_deinit();
        bluetooth_hid_deinit();
        vSemaphoreDelete(state_mutex);
        state_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }
    
    // 创建控制输出任务
    task_ret = xTaskCreate(
        control_output_task,
        "control_output",
        CONTROL_OUTPUT_TASK_STACK_SIZE,
        NULL,
        CONTROL_OUTPUT_TASK_PRIORITY,
        &output_task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create control output task");
        vTaskDelete(input_task_handle);
        plane_control_deinit();
        car_control_deinit();
        vibration_deinit();
        bluetooth_hid_deinit();
        vSemaphoreDelete(state_mutex);
        state_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Gamepad controller initialized successfully");
    ESP_LOGI(TAG, "Input task priority: %d, Output task priority: %d", 
             GAMEPAD_INPUT_TASK_PRIORITY, CONTROL_OUTPUT_TASK_PRIORITY);
    ESP_LOGI(TAG, "Car control: PWM pins [%d,%d], Direction pins [%d,%d,%d,%d]",
             default_car_config.left_motor_pwm_pin, default_car_config.right_motor_pwm_pin,
             default_car_config.left_motor_dir1_pin, default_car_config.left_motor_dir2_pin,
             default_car_config.right_motor_dir1_pin, default_car_config.right_motor_dir2_pin);
    ESP_LOGI(TAG, "Plane control: Servo pins [%d,%d,%d,%d]",
             default_plane_config.throttle_pin, default_plane_config.elevator_pin,
             default_plane_config.rudder_pin, default_plane_config.aileron_pin);
    
    return ESP_OK;
}

esp_err_t gamepad_controller_set_mode(control_mode_t mode)
{
    if (mode >= CONTROL_MODE_DISABLED) {
        // 允许设置为禁用模式
    } else if (mode > CONTROL_MODE_PLANE) {
        ESP_LOGE(TAG, "Invalid control mode: %d", mode);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Setting control mode from %d to %d", current_mode, mode);
    current_mode = mode;
    
    // 模式切换时的特殊处理
    switch (mode) {
        case CONTROL_MODE_CAR:
            ESP_LOGI(TAG, "Switched to Car Control Mode");
            // TODO: 初始化小车控制参数
            break;
            
        case CONTROL_MODE_PLANE:
            ESP_LOGI(TAG, "Switched to Plane Control Mode");
            // TODO: 初始化飞机控制参数
            break;
            
        case CONTROL_MODE_DISABLED:
            ESP_LOGI(TAG, "Control Disabled");
            // TODO: 停止所有输出
            break;
    }
    
    return ESP_OK;
}

control_mode_t gamepad_controller_get_mode(void)
{
    return current_mode;
}

esp_err_t gamepad_controller_get_state(gamepad_state_t *state)
{
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        memcpy(state, &current_state, sizeof(gamepad_state_t));
        xSemaphoreGive(state_mutex);
        return ESP_OK;
    }
    
    ESP_LOGW(TAG, "Failed to acquire state mutex");
    return ESP_ERR_TIMEOUT;
}

esp_err_t gamepad_controller_vibrate(const vibration_params_t *params)
{
    if (params == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
     ESP_LOGI(TAG, "Controller vibration request: left=%d, right=%d, duration=%"PRIu32"ms",
             params->left_intensity, params->right_intensity, params->duration_ms);
    
    return vibration_start(params);
}

esp_err_t gamepad_controller_stop_vibration(void)
{
    ESP_LOGI(TAG, "Stopping controller vibration");
    
    return vibration_stop();
}

bool gamepad_controller_is_connected(void)
{
    gamepad_state_t state;
    if (gamepad_controller_get_state(&state) == ESP_OK) {
        return state.connected;
    }
    return false;
}

int8_t gamepad_controller_get_battery_level(void)
{
    // TODO: 实现电池电量检测
    // 大多数手柄通过HID协议可以获取电池信息
    return -1; // 暂时返回不支持
}
