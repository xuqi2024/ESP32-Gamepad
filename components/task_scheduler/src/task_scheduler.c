/**
 * @file task_scheduler.c
 * @brief ESP32-Gamepad 任务调度器实现
 * 
 * 提供高级任务调度、优先级管理、资源分配和性能监控功能
 * 
 * @author ESP32-Gamepad Team
 * @date 2024
 */

#include "task_scheduler.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "TASK_SCHEDULER";

/* 最大任务数量 */
#define MAX_TASKS 32

/* 任务调度器内部任务结构 */
typedef struct {
    task_id_t id;                       /**< 任务ID */
    task_config_t config;               /**< 任务配置 */
    task_stats_t stats;                 /**< 任务统计 */
    TaskHandle_t handle;                /**< FreeRTOS任务句柄 */
    bool is_active;                     /**< 是否活跃 */
    uint32_t create_time;               /**< 创建时间 */
    uint32_t last_wakeup_time;          /**< 上次唤醒时间 */
    esp_timer_handle_t timer;           /**< 定时器句柄 */
} scheduler_task_t;

/* 调度器状态 */
static bool is_initialized = false;
static scheduler_task_t tasks[MAX_TASKS];
static uint32_t task_count = 0;
static task_id_t next_task_id = 1;
static SemaphoreHandle_t scheduler_mutex = NULL;

/* 调度器统计 */
static scheduler_stats_t scheduler_stats = {0};

/* 内部函数声明 */
static scheduler_task_t* find_task_by_id(task_id_t id);
static scheduler_task_t* find_free_task_slot(void);
static void task_wrapper(void *param);
static void periodic_timer_callback(void *arg);
static void cleanup_completed_tasks(void);
static void update_task_stats(scheduler_task_t *task, uint32_t execution_time);

/**
 * @brief 初始化任务调度器
 */
esp_err_t task_scheduler_init(const scheduler_config_t *config)
{
    if (is_initialized) {
        ESP_LOGW(TAG, "Task scheduler already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing task scheduler...");

    // 创建互斥锁
    scheduler_mutex = xSemaphoreCreateMutex();
    if (scheduler_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create scheduler mutex");
        return ESP_ERR_NO_MEM;
    }

    // 初始化任务数组
    memset(tasks, 0, sizeof(tasks));
    task_count = 0;
    next_task_id = 1;

    // 初始化统计信息
    memset(&scheduler_stats, 0, sizeof(scheduler_stats));
    scheduler_stats.start_time = esp_timer_get_time() / 1000;

    // 如果提供了配置，使用配置参数（此处暂时忽略config参数）
    (void)config;

    is_initialized = true;
    ESP_LOGI(TAG, "Task scheduler initialized successfully");
    return ESP_OK;
}

/**
 * @brief 反初始化任务调度器
 */
esp_err_t task_scheduler_deinit(void)
{
    if (!is_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing task scheduler...");

    // 停止所有任务
    task_scheduler_stop_all_tasks();

    // 删除互斥锁
    if (scheduler_mutex) {
        vSemaphoreDelete(scheduler_mutex);
        scheduler_mutex = NULL;
    }

    // 清理任务数组
    memset(tasks, 0, sizeof(tasks));
    task_count = 0;

    is_initialized = false;
    ESP_LOGI(TAG, "Task scheduler deinitialized");
    return ESP_OK;
}

/**
 * @brief 创建任务
 */
task_id_t task_scheduler_create_task(const task_config_t *config)
{
    if (!is_initialized || config == NULL || config->function == NULL) {
        ESP_LOGE(TAG, "Invalid parameters for task creation");
        return INVALID_TASK_ID;
    }

    // 检查任务类型
    if (config->type >= TASK_TYPE_MAX) {
        ESP_LOGE(TAG, "Invalid task type: %d", config->type);
        return INVALID_TASK_ID;
    }

    // 获取互斥锁
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take scheduler mutex");
        return INVALID_TASK_ID;
    }

    // 查找空闲任务槽
    scheduler_task_t *task = find_free_task_slot();
    if (task == NULL) {
        ESP_LOGE(TAG, "No free task slots available");
        xSemaphoreGive(scheduler_mutex);
        return INVALID_TASK_ID;
    }

    // 分配任务ID
    task_id_t id = next_task_id++;
    if (next_task_id == INVALID_TASK_ID) {
        next_task_id = 1; // 避免使用无效ID
    }

    // 初始化任务
    task->id = id;
    memcpy(&task->config, config, sizeof(task_config_t));
    memset(&task->stats, 0, sizeof(task_stats_t));
    task->stats.current_state = TASK_STATE_CREATED;
    task->stats.min_execution_time_ms = UINT32_MAX;
    task->is_active = true;
    task->create_time = esp_timer_get_time() / 1000;
    task->handle = NULL;
    task->timer = NULL;

    // 根据任务类型创建任务
    switch (config->type) {
        case TASK_TYPE_PERIODIC:
        case TASK_TYPE_ONESHOT:
        case TASK_TYPE_CONDITIONAL:
            // 创建FreeRTOS任务
            BaseType_t ret = xTaskCreate(
                task_wrapper,
                config->name ? config->name : "scheduler_task",
                config->stack_size > 0 ? config->stack_size : 2048,
                task,
                config->priority,
                &task->handle
            );
            
            if (ret != pdPASS) {
                ESP_LOGE(TAG, "Failed to create FreeRTOS task");
                task->is_active = false;
                xSemaphoreGive(scheduler_mutex);
                return INVALID_TASK_ID;
            }
            break;

        case TASK_TYPE_DELAYED:
            // 创建延迟定时器
            esp_timer_create_args_t timer_args = {
                .callback = periodic_timer_callback,
                .arg = task,
                .name = config->name ? config->name : "delayed_task"
            };
            
            esp_err_t err = esp_timer_create(&timer_args, &task->timer);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to create delayed timer: %s", esp_err_to_name(err));
                task->is_active = false;
                xSemaphoreGive(scheduler_mutex);
                return INVALID_TASK_ID;
            }
            
            // 启动延迟定时器
            err = esp_timer_start_once(task->timer, config->delay_ms * 1000);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start delayed timer: %s", esp_err_to_name(err));
                esp_timer_delete(task->timer);
                task->is_active = false;
                xSemaphoreGive(scheduler_mutex);
                return INVALID_TASK_ID;
            }
            break;

        default:
            ESP_LOGE(TAG, "Unsupported task type: %d", config->type);
            task->is_active = false;
            xSemaphoreGive(scheduler_mutex);
            return INVALID_TASK_ID;
    }

    task_count++;
    scheduler_stats.total_tasks_created++;
    scheduler_stats.active_tasks++;

    ESP_LOGI(TAG, "Task created: ID=%lu, type=%d, name=%s", 
             id, config->type, config->name ? config->name : "unnamed");

    xSemaphoreGive(scheduler_mutex);
    return id;
}

/**
 * @brief 删除任务
 */
esp_err_t task_scheduler_delete_task(task_id_t id)
{
    if (!is_initialized || id == INVALID_TASK_ID) {
        return ESP_ERR_INVALID_ARG;
    }

    // 获取互斥锁
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take scheduler mutex");
        return ESP_ERR_TIMEOUT;
    }

    scheduler_task_t *task = find_task_by_id(id);
    if (task == NULL) {
        ESP_LOGE(TAG, "Task not found: ID=%lu", id);
        xSemaphoreGive(scheduler_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    // 停止定时器
    if (task->timer) {
        esp_timer_stop(task->timer);
        esp_timer_delete(task->timer);
        task->timer = NULL;
    }

    // 删除FreeRTOS任务
    if (task->handle) {
        vTaskDelete(task->handle);
        task->handle = NULL;
    }

    // 清理任务
    task->is_active = false;
    task->stats.current_state = TASK_STATE_COMPLETED;
    task_count--;
    scheduler_stats.active_tasks--;

    ESP_LOGI(TAG, "Task deleted: ID=%lu", id);

    xSemaphoreGive(scheduler_mutex);
    return ESP_OK;
}

/**
 * @brief 暂停任务
 */
esp_err_t task_scheduler_suspend_task(task_id_t id)
{
    if (!is_initialized || id == INVALID_TASK_ID) {
        return ESP_ERR_INVALID_ARG;
    }

    // 获取互斥锁
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    scheduler_task_t *task = find_task_by_id(id);
    if (task == NULL) {
        xSemaphoreGive(scheduler_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    if (task->handle) {
        vTaskSuspend(task->handle);
        task->stats.current_state = TASK_STATE_SUSPENDED;
        ESP_LOGI(TAG, "Task suspended: ID=%lu", id);
    }

    if (task->timer) {
        esp_timer_stop(task->timer);
    }

    xSemaphoreGive(scheduler_mutex);
    return ESP_OK;
}

/**
 * @brief 恢复任务
 */
esp_err_t task_scheduler_resume_task(task_id_t id)
{
    if (!is_initialized || id == INVALID_TASK_ID) {
        return ESP_ERR_INVALID_ARG;
    }

    // 获取互斥锁
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    scheduler_task_t *task = find_task_by_id(id);
    if (task == NULL) {
        xSemaphoreGive(scheduler_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    if (task->handle) {
        vTaskResume(task->handle);
        task->stats.current_state = TASK_STATE_READY;
        ESP_LOGI(TAG, "Task resumed: ID=%lu", id);
    }

    if (task->timer && task->config.type == TASK_TYPE_DELAYED) {
        esp_timer_start_once(task->timer, task->config.delay_ms * 1000);
    }

    xSemaphoreGive(scheduler_mutex);
    return ESP_OK;
}

/**
 * @brief 获取任务统计信息
 */
esp_err_t task_scheduler_get_task_stats(task_id_t id, task_stats_t *stats)
{
    if (!is_initialized || id == INVALID_TASK_ID || stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 获取互斥锁
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    scheduler_task_t *task = find_task_by_id(id);
    if (task == NULL) {
        xSemaphoreGive(scheduler_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    memcpy(stats, &task->stats, sizeof(task_stats_t));

    xSemaphoreGive(scheduler_mutex);
    return ESP_OK;
}

/**
 * @brief 获取调度器统计信息
 */
esp_err_t task_scheduler_get_stats(scheduler_stats_t *stats)
{
    if (!is_initialized || stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 获取互斥锁
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // 更新当前时间
    scheduler_stats.uptime_ms = esp_timer_get_time() / 1000 - scheduler_stats.start_time;
    
    memcpy(stats, &scheduler_stats, sizeof(scheduler_stats_t));

    xSemaphoreGive(scheduler_mutex);
    return ESP_OK;
}

/**
 * @brief 停止所有任务
 */
esp_err_t task_scheduler_stop_all_tasks(void)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Stopping all tasks...");

    // 获取互斥锁
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].is_active) {
            // 停止定时器
            if (tasks[i].timer) {
                esp_timer_stop(tasks[i].timer);
                esp_timer_delete(tasks[i].timer);
                tasks[i].timer = NULL;
            }

            // 删除FreeRTOS任务
            if (tasks[i].handle) {
                vTaskDelete(tasks[i].handle);
                tasks[i].handle = NULL;
            }

            tasks[i].is_active = false;
            tasks[i].stats.current_state = TASK_STATE_COMPLETED;
        }
    }

    task_count = 0;
    scheduler_stats.active_tasks = 0;

    xSemaphoreGive(scheduler_mutex);

    ESP_LOGI(TAG, "All tasks stopped");
    return ESP_OK;
}

/**
 * @brief 任务包装函数
 */
static void task_wrapper(void *param)
{
    scheduler_task_t *task = (scheduler_task_t *)param;
    uint32_t start_time, end_time, execution_time;

    ESP_LOGI(TAG, "Task started: ID=%lu, type=%d", task->id, task->config.type);

    task->stats.current_state = TASK_STATE_RUNNING;
    task->last_wakeup_time = xTaskGetTickCount();

    while (task->is_active) {
        bool should_execute = false;

        // 检查执行条件
        switch (task->config.type) {
            case TASK_TYPE_PERIODIC:
                should_execute = true;
                break;
                
            case TASK_TYPE_ONESHOT:
                should_execute = (task->stats.execution_count == 0);
                break;
                
            case TASK_TYPE_CONDITIONAL:
                should_execute = task->config.condition ? 
                    task->config.condition(task->config.param) : false;
                break;
                
            default:
                should_execute = false;
                break;
        }

        if (should_execute) {
            // 记录开始时间
            start_time = esp_timer_get_time() / 1000;
            task->stats.last_execution_time = start_time;

            // 执行任务函数
            task->config.function(task->config.param);

            // 记录结束时间并更新统计
            end_time = esp_timer_get_time() / 1000;
            execution_time = end_time - start_time;
            
            update_task_stats(task, execution_time);

            // 检查是否超时
            if (task->config.max_execution_time_ms > 0 && 
                execution_time > task->config.max_execution_time_ms) {
                ESP_LOGW(TAG, "Task %lu execution time exceeded: %lums > %lums", 
                         task->id, execution_time, task->config.max_execution_time_ms);
                task->stats.missed_deadlines++;
            }

            // 一次性任务执行完毕后退出
            if (task->config.type == TASK_TYPE_ONESHOT) {
                break;
            }
        }

        // 周期性任务等待下一个周期
        if (task->config.type == TASK_TYPE_PERIODIC && task->config.period_ms > 0) {
            vTaskDelayUntil(&task->last_wakeup_time, pdMS_TO_TICKS(task->config.period_ms));
            task->stats.next_execution_time = esp_timer_get_time() / 1000 + task->config.period_ms;
        } else {
            vTaskDelay(pdMS_TO_TICKS(10)); // 短暂延迟避免占用太多CPU
        }
    }

    task->stats.current_state = TASK_STATE_COMPLETED;

    // 调用完成回调
    if (task->config.callback) {
        task->config.callback(task->id, true, task->config.param);
    }

    // 自动删除任务
    if (task->config.auto_delete) {
        task_scheduler_delete_task(task->id);
    }

    ESP_LOGI(TAG, "Task completed: ID=%lu", task->id);
    vTaskDelete(NULL);
}

/**
 * @brief 周期性定时器回调函数
 */
static void periodic_timer_callback(void *arg)
{
    scheduler_task_t *task = (scheduler_task_t *)arg;
    
    if (task && task->is_active && task->config.function) {
        uint32_t start_time = esp_timer_get_time() / 1000;
        
        // 执行任务函数
        task->config.function(task->config.param);
        
        uint32_t execution_time = esp_timer_get_time() / 1000 - start_time;
        update_task_stats(task, execution_time);

        // 调用完成回调
        if (task->config.callback) {
            task->config.callback(task->id, true, task->config.param);
        }

        // 自动删除延迟任务
        if (task->config.type == TASK_TYPE_DELAYED && task->config.auto_delete) {
            task_scheduler_delete_task(task->id);
        }
    }
}

/**
 * @brief 查找任务
 */
static scheduler_task_t* find_task_by_id(task_id_t id)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].is_active && tasks[i].id == id) {
            return &tasks[i];
        }
    }
    return NULL;
}

/**
 * @brief 查找空闲任务槽
 */
static scheduler_task_t* find_free_task_slot(void)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        if (!tasks[i].is_active) {
            return &tasks[i];
        }
    }
    return NULL;
}

/**
 * @brief 更新任务统计信息
 */
static void update_task_stats(scheduler_task_t *task, uint32_t execution_time)
{
    task->stats.execution_count++;
    task->stats.total_execution_time_ms += execution_time;
    
    if (task->stats.execution_count > 0) {
        task->stats.avg_execution_time_ms = 
            task->stats.total_execution_time_ms / task->stats.execution_count;
    }
    
    if (execution_time > task->stats.max_execution_time_ms) {
        task->stats.max_execution_time_ms = execution_time;
    }
    
    if (execution_time < task->stats.min_execution_time_ms) {
        task->stats.min_execution_time_ms = execution_time;
    }

    // 更新调度器统计
    scheduler_stats.total_executions++;
    scheduler_stats.total_execution_time_ms += execution_time;
    
    if (scheduler_stats.total_executions > 0) {
        scheduler_stats.avg_execution_time_ms = 
            scheduler_stats.total_execution_time_ms / scheduler_stats.total_executions;
    }
}

/**
 * @brief 清理已完成的任务
 */
static void __attribute__((unused)) cleanup_completed_tasks(void)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].is_active && 
            tasks[i].stats.current_state == TASK_STATE_COMPLETED &&
            tasks[i].config.auto_delete) {
            
            task_scheduler_delete_task(tasks[i].id);
        }
    }
}

/**
 * @brief 获取活跃任务数量
 */
uint32_t task_scheduler_get_active_task_count(void)
{
    return task_count;
}

/**
 * @brief 检查任务是否存在
 */
bool task_scheduler_task_exists(task_id_t id)
{
    if (!is_initialized || id == INVALID_TASK_ID) {
        return false;
    }

    bool exists = false;
    
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        exists = (find_task_by_id(id) != NULL);
        xSemaphoreGive(scheduler_mutex);
    }

    return exists;
}
