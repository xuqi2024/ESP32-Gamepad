#!/bin/bash
# ESP32-Gamepad 一体化构建脚本
# 功能：环境设置、编译、烧录、监控

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 项目配置
PROJECT_NAME="ESP32-Gamepad"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ESP_IDF_PATH="$HOME/esp/v5.4.2/esp-idf"  # 用户的ESP-IDF路径
FLASH_PORT="/dev/cu.usbserial-0001"      # 默认串口

# 日志函数
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# 显示帮助信息
show_help() {
    cat << EOF
ESP32-Gamepad 构建脚本

用法: $0 [选项] [命令]

命令:
    setup           设置ESP-IDF环境
    build           编译项目
    flash           烧录到设备
    monitor         串口监控
    clean           清理构建文件
    menuconfig      打开配置菜单
    full            完整流程 (setup + build + flash)
    auto            自动化完整流程 (无需确认)

选项:
    -p PORT         指定串口 (默认: $FLASH_PORT)
    -v              详细输出
    -y              自动确认所有提示
    -h              显示此帮助信息

示例:
    $0 setup                    # 设置环境
    $0 build                    # 编译项目
    $0 -p /dev/cu.usbserial-1234 flash   # 指定串口烧录
    $0 full                     # 完整流程
    $0 auto                     # 自动化完整流程
    $0 -y full                  # 自动确认的完整流程

EOF
}

# 设置ESP-IDF环境
setup_environment() {
    log_info "设置ESP-IDF环境..."
    
    # 检查ESP-IDF路径
    if [ ! -d "$ESP_IDF_PATH" ]; then
        log_error "ESP-IDF目录不存在: $ESP_IDF_PATH"
        log_info "请安装ESP-IDF到正确路径，或修改脚本中的ESP_IDF_PATH变量"
        return 1
    fi
    
    # 设置环境变量
    export IDF_PATH="$ESP_IDF_PATH"
    
    # 加载ESP-IDF环境
    if [ -f "$IDF_PATH/export.sh" ]; then
        log_info "加载ESP-IDF环境..."
        source "$IDF_PATH/export.sh" > /dev/null 2>&1
        
        # 验证环境
        if command -v idf.py &> /dev/null; then
            log_success "ESP-IDF环境设置完成"
            if [ "$VERBOSE" = "true" ]; then
                echo "ESP-IDF版本: $(idf.py --version 2>/dev/null)"
                echo "Python版本: $(python3 --version 2>/dev/null)"
            fi
        else
            log_error "ESP-IDF环境设置失败"
            return 1
        fi
    else
        log_error "未找到export.sh文件: $IDF_PATH/export.sh"
        return 1
    fi
}

# 编译项目
build_project() {
    log_info "编译ESP32-Gamepad项目..."
    
    cd "$PROJECT_ROOT"
    
    # 设置目标芯片
    idf.py set-target esp32 > /dev/null 2>&1
    
    # 编译
    if [ "$VERBOSE" = "true" ]; then
        idf.py build
    else
        idf.py build > build.log 2>&1 || {
            log_error "编译失败，查看 build.log 获取详细信息"
            tail -10 build.log
            return 1
        }
    fi
    
    log_success "项目编译完成"
    
    # 显示大小信息
    if [ -f "build/${PROJECT_NAME}.bin" ]; then
        size=$(ls -lh "build/${PROJECT_NAME}.bin" | awk '{print $5}')
        log_info "程序大小: $size"
    fi
}

# 烧录到设备
flash_project() {
    # 自动检测串口
    detect_serial_port
    
    log_info "烧录到设备 ($FLASH_PORT)..."
    
    cd "$PROJECT_ROOT"
    
    # 检查设备连接
    if [ ! -e "$FLASH_PORT" ]; then
        log_error "设备未连接或端口不正确: $FLASH_PORT"
        log_info "可用串口设备:"
        ls /dev/cu.* 2>/dev/null | grep -v Bluetooth || echo "无"
        return 1
    fi
    
    # 烧录
    idf.py -p "$FLASH_PORT" flash
    
    log_success "烧录完成"
}

# 串口监控
monitor_device() {
    # 自动检测串口
    detect_serial_port
    
    log_info "开始串口监控 ($FLASH_PORT)..."
    log_info "按 Ctrl+] 退出监控"
    
    cd "$PROJECT_ROOT"
    idf.py -p "$FLASH_PORT" monitor
}

# 清理构建文件
clean_build() {
    log_info "清理构建文件..."
    
    cd "$PROJECT_ROOT"
    
    if [ -d "build" ]; then
        rm -rf build
        log_success "构建文件已清理"
    else
        log_info "没有需要清理的构建文件"
    fi
    
    # 清理日志文件
    if [ -f "build.log" ]; then
        rm -f build.log
        log_info "日志文件已清理"
    fi
}

# 自动检测串口
detect_serial_port() {
    # 如果用户通过 -p 参数指定了串口，直接使用
    if [ "$USER_SPECIFIED_PORT" = "true" ]; then
        log_info "使用用户指定的串口: $FLASH_PORT"
        return
    fi
    
    # 自动检测ESP32设备串口
    # 优先级顺序: usbserial -> SLAB_USBtoUART -> wchusbserial -> usbmodem
    for pattern in "usbserial" "SLAB_USBtoUART" "wchusbserial" "usbmodem"; do
        available_port=$(ls /dev/cu.*${pattern}* 2>/dev/null | head -1)
        if [ -n "$available_port" ]; then
            FLASH_PORT="$available_port"
            log_info "自动检测到ESP32串口: $FLASH_PORT"
            return
        fi
    done
    
    # 如果没有找到常见的ESP32串口，列出所有可用串口供用户选择
    all_ports=$(ls /dev/cu.* 2>/dev/null | grep -v -E "(Bluetooth|debug-console)" | head -5)
    if [ -n "$all_ports" ]; then
        log_warning "未找到常见的ESP32串口，可用串口设备:"
        echo "$all_ports"
        # 选择第一个非调试控制台的串口
        FLASH_PORT=$(echo "$all_ports" | head -1)
        log_info "尝试使用串口: $FLASH_PORT"
        log_warning "如果此串口不正确，请使用 -p 参数指定正确的串口"
    else
        log_error "未检测到任何串口设备"
        log_info "请确保ESP32设备已连接，或使用 -p 参数手动指定串口"
    fi
}

# 打开配置菜单
run_menuconfig() {
    log_info "打开配置菜单..."
    
    cd "$PROJECT_ROOT"
    idf.py menuconfig
}

# 完整构建流程
full_build() {
    log_info "开始完整构建流程..."
    
    setup_environment
    build_project
    
    if [ "$AUTO_CONFIRM" = "true" ]; then
        log_info "自动模式：开始烧录..."
        flash_project
        log_info "自动模式：开始串口监控..."
        monitor_device
    else
        read -p "是否要烧录到设备? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            flash_project
            
            read -p "是否要开始串口监控? (y/N): " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                monitor_device
            fi
        fi
    fi
    
    log_success "完整构建流程完成"
}

# 自动化完整构建流程
auto_build() {
    log_info "开始自动化构建流程..."
    
    setup_environment
    build_project
    
    log_info "自动烧录到设备..."
    flash_project
    
    log_info "自动开始串口监控..."
    log_info "按 Ctrl+] 退出监控"
    monitor_device
    
    log_success "自动化构建流程完成"
}

# 解析命令行参数
VERBOSE=false
AUTO_CONFIRM=false
USER_SPECIFIED_PORT=false

while getopts "p:vyh" opt; do
    case $opt in
        p)
            FLASH_PORT="$OPTARG"
            USER_SPECIFIED_PORT=true
            ;;
        v)
            VERBOSE=true
            ;;
        y)
            AUTO_CONFIRM=true
            ;;
        h)
            show_help
            exit 0
            ;;
        \?)
            log_error "无效选项: -$OPTARG"
            show_help
            exit 1
            ;;
    esac
done

shift $((OPTIND-1))

# 主程序
main() {
    local command="$1"
    
    echo "🎮 ESP32-Gamepad 构建脚本"
    echo "========================="
    
    case "$command" in
        "setup")
            setup_environment
            ;;
        "build")
            setup_environment && build_project
            ;;
        "flash")
            setup_environment && flash_project
            ;;
        "monitor")
            monitor_device
            ;;
        "clean")
            clean_build
            ;;
        "menuconfig")
            setup_environment && run_menuconfig
            ;;
        "full")
            full_build
            ;;
        "auto")
            auto_build
            ;;
        "")
            log_error "请指定命令"
            show_help
            exit 1
            ;;
        *)
            log_error "未知命令: $command"
            show_help
            exit 1
            ;;
    esac
}

# 运行主程序
main "$@"
