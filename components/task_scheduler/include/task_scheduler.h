/**
 * @file task_scheduler.h
 * @brief ESP32-Gamepad 任务调度器
 * 
 * 提供高级任务调度、优先级管理、资源分配和性能监控功能
 * 支持周期性任务、一次性任务、延迟任务和条件任务
 * 
 * @author ESP32-Gamepad Team
 * @date 2024
 */

#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 任务类型枚举 */
typedef enum {
    TASK_TYPE_PERIODIC = 0,     /**< 周期性任务 */
    TASK_TYPE_ONESHOT,          /**< 一次性任务 */
    TASK_TYPE_DELAYED,          /**< 延迟任务 */
    TASK_TYPE_CONDITIONAL,      /**< 条件任务 */
    TASK_TYPE_MAX
} task_type_t;

/* 任务状态枚举 */
typedef enum {
    TASK_STATE_CREATED = 0,     /**< 已创建 */
    TASK_STATE_READY,           /**< 准备就绪 */
    TASK_STATE_RUNNING,         /**< 运行中 */
    TASK_STATE_SUSPENDED,       /**< 已暂停 */
    TASK_STATE_COMPLETED,       /**< 已完成 */
    TASK_STATE_ERROR,           /**< 错误状态 */
    TASK_STATE_MAX
} task_state_t;

/* 任务优先级枚举 */
typedef enum {
    TASK_PRIORITY_CRITICAL = 10,    /**< 关键任务 */
    TASK_PRIORITY_HIGH = 8,         /**< 高优先级 */
    TASK_PRIORITY_NORMAL = 5,       /**< 普通优先级 */
    TASK_PRIORITY_LOW = 3,          /**< 低优先级 */
    TASK_PRIORITY_BACKGROUND = 1    /**< 后台任务 */
} task_priority_t;

/* 任务ID类型 */
typedef uint32_t task_id_t;

/* 无效任务ID */
#define INVALID_TASK_ID 0

/* 任务函数指针类型 */
typedef void (*task_function_t)(void *param);

/* 任务条件检查函数指针类型 */
typedef bool (*task_condition_t)(void *param);

/* 任务完成回调函数指针类型 */
typedef void (*task_completion_callback_t)(task_id_t task_id, bool success, void *param);

/* 任务配置结构 */
typedef struct {
    task_type_t type;                   /**< 任务类型 */
    task_priority_t priority;           /**< 任务优先级 */
    task_function_t function;           /**< 任务函数 */
    void *param;                        /**< 任务参数 */
    uint32_t period_ms;                 /**< 周期(ms)，仅周期性任务有效 */
    uint32_t delay_ms;                  /**< 延迟(ms)，仅延迟任务有效 */
    task_condition_t condition;         /**< 条件函数，仅条件任务有效 */
    task_completion_callback_t callback; /**< 完成回调 */
    uint32_t stack_size;                /**< 栈大小 */
    uint32_t max_execution_time_ms;     /**< 最大执行时间(ms) */
    bool auto_delete;                   /**< 是否自动删除 */
    const char *name;                   /**< 任务名称 */
} task_config_t;

/* 任务统计信息 */
typedef struct {
    uint32_t execution_count;           /**< 执行次数 */
    uint32_t total_execution_time_ms;   /**< 总执行时间(ms) */
    uint32_t avg_execution_time_ms;     /**< 平均执行时间(ms) */
    uint32_t max_execution_time_ms;     /**< 最大执行时间(ms) */
    uint32_t min_execution_time_ms;     /**< 最小执行时间(ms) */
    uint32_t missed_deadlines;          /**< 错过的截止时间 */
    uint32_t error_count;               /**< 错误次数 */
    task_state_t current_state;         /**< 当前状态 */
    uint32_t last_execution_time;       /**< 上次执行时间 */
    uint32_t next_execution_time;       /**< 下次执行时间 */
} task_stats_t;

/* 调度器统计信息 */
typedef struct {
    uint32_t total_tasks;               /**< 总任务数 */
    uint32_t active_tasks;              /**< 活跃任务数 */
    uint32_t completed_tasks;           /**< 已完成任务数 */
    uint32_t failed_tasks;              /**< 失败任务数 */
    uint32_t total_context_switches;    /**< 总上下文切换次数 */
    uint32_t cpu_utilization;           /**< CPU利用率(%) */
    uint32_t memory_usage;              /**< 内存使用量(字节) */
    uint32_t scheduler_overhead_us;     /**< 调度器开销(微秒) */
} scheduler_stats_t;

/* 调度器配置结构 */
typedef struct {
    uint32_t max_tasks;                 /**< 最大任务数 */
    uint32_t tick_rate_hz;              /**< 时钟频率(Hz) */
    bool enable_watchdog;               /**< 启用看门狗 */
    uint32_t watchdog_timeout_ms;       /**< 看门狗超时(ms) */
    bool enable_profiling;              /**< 启用性能分析 */
    bool enable_load_balancing;         /**< 启用负载均衡 */
} scheduler_config_t;

/**
 * @brief 初始化任务调度器
 * 
 * @param config 调度器配置，NULL使用默认配置
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_init(const scheduler_config_t *config);

/**
 * @brief 反初始化任务调度器
 * 
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_deinit(void);

/**
 * @brief 启动任务调度器
 * 
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_start(void);

/**
 * @brief 停止任务调度器
 * 
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_stop(void);

/**
 * @brief 创建任务
 * 
 * @param config 任务配置
 * @return task_id_t 任务ID，INVALID_TASK_ID表示失败
 */
task_id_t task_scheduler_create_task(const task_config_t *config);

/**
 * @brief 删除任务
 * 
 * @param task_id 任务ID
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_delete_task(task_id_t task_id);

/**
 * @brief 暂停任务
 * 
 * @param task_id 任务ID
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_suspend_task(task_id_t task_id);

/**
 * @brief 恢复任务
 * 
 * @param task_id 任务ID
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_resume_task(task_id_t task_id);

/**
 * @brief 获取任务状态
 * 
 * @param task_id 任务ID
 * @return task_state_t 任务状态
 */
task_state_t task_scheduler_get_task_state(task_id_t task_id);

/**
 * @brief 设置任务优先级
 * 
 * @param task_id 任务ID
 * @param priority 新优先级
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_set_task_priority(task_id_t task_id, task_priority_t priority);

/**
 * @brief 设置任务周期
 * 
 * @param task_id 任务ID
 * @param period_ms 新周期(ms)
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_set_task_period(task_id_t task_id, uint32_t period_ms);

/**
 * @brief 立即执行任务
 * 
 * @param task_id 任务ID
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_run_task_now(task_id_t task_id);

/**
 * @brief 获取任务统计信息
 * 
 * @param task_id 任务ID
 * @param stats 输出的统计信息
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_get_task_stats(task_id_t task_id, task_stats_t *stats);

/**
 * @brief 获取调度器统计信息
 * 
 * @param stats 输出的统计信息
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_get_scheduler_stats(scheduler_stats_t *stats);

/**
 * @brief 清除任务统计信息
 * 
 * @param task_id 任务ID，INVALID_TASK_ID清除所有
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_clear_stats(task_id_t task_id);

/**
 * @brief 获取系统时间戳
 * 
 * @return uint32_t 时间戳(ms)
 */
uint32_t task_scheduler_get_timestamp(void);

/**
 * @brief 延迟执行
 * 
 * @param delay_ms 延迟时间(ms)
 */
void task_scheduler_delay(uint32_t delay_ms);

/**
 * @brief 让出CPU时间片
 */
void task_scheduler_yield(void);

/**
 * @brief 获取当前任务ID
 * 
 * @return task_id_t 当前任务ID
 */
task_id_t task_scheduler_get_current_task_id(void);

/**
 * @brief 检查任务是否存在
 * 
 * @param task_id 任务ID
 * @return bool true存在，false不存在
 */
bool task_scheduler_task_exists(task_id_t task_id);

/**
 * @brief 列出所有任务
 * 
 * @param task_list 任务ID列表输出缓冲区
 * @param max_tasks 最大任务数
 * @return uint32_t 实际任务数
 */
uint32_t task_scheduler_list_tasks(task_id_t *task_list, uint32_t max_tasks);

/**
 * @brief 获取任务信息
 * 
 * @param task_id 任务ID
 * @param config 输出的任务配置
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_get_task_info(task_id_t task_id, task_config_t *config);

/**
 * @brief 设置全局完成回调
 * 
 * @param callback 回调函数
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_set_global_callback(task_completion_callback_t callback);

/**
 * @brief 启用/禁用调度器
 * 
 * @param enable true启用，false禁用
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_enable(bool enable);

/**
 * @brief 获取调度器状态
 * 
 * @return bool true运行中，false已停止
 */
bool task_scheduler_is_running(void);

/**
 * @brief 导出调度器报告
 * 
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t task_scheduler_export_report(char *buffer, size_t size);

/**
 * @brief 获取调度器版本
 * 
 * @return const char* 版本字符串
 */
const char* task_scheduler_get_version(void);

/* 便捷宏定义 */

/**
 * @brief 创建周期性任务的便捷宏
 */
#define CREATE_PERIODIC_TASK(name, func, param, period, priority) \
    task_scheduler_create_task(&(task_config_t){ \
        .type = TASK_TYPE_PERIODIC, \
        .priority = priority, \
        .function = func, \
        .param = param, \
        .period_ms = period, \
        .stack_size = 4096, \
        .auto_delete = false, \
        .name = name \
    })

/**
 * @brief 创建一次性任务的便捷宏
 */
#define CREATE_ONESHOT_TASK(name, func, param, priority) \
    task_scheduler_create_task(&(task_config_t){ \
        .type = TASK_TYPE_ONESHOT, \
        .priority = priority, \
        .function = func, \
        .param = param, \
        .stack_size = 4096, \
        .auto_delete = true, \
        .name = name \
    })

/**
 * @brief 创建延迟任务的便捷宏
 */
#define CREATE_DELAYED_TASK(name, func, param, delay, priority) \
    task_scheduler_create_task(&(task_config_t){ \
        .type = TASK_TYPE_DELAYED, \
        .priority = priority, \
        .function = func, \
        .param = param, \
        .delay_ms = delay, \
        .stack_size = 4096, \
        .auto_delete = true, \
        .name = name \
    })

#ifdef __cplusplus
}
#endif

#endif // TASK_SCHEDULER_H
