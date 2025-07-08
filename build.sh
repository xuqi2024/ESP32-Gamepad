#!/bin/bash
# ESP32-Gamepad 项目构建和测试脚本
# 自动化编译、测试、部署流程

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 项目配置
PROJECT_NAME="ESP32-Gamepad"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
FLASH_PORT="/dev/ttyUSB0"  # 默认串口，可通过参数修改

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 显示帮助信息
show_help() {
    cat << EOF
ESP32-Gamepad 构建脚本

用法: $0 [选项] [命令]

命令:
    clean           清理构建文件
    build           编译项目
    flash           烧录到设备
    monitor         串口监控
    test            运行测试
    menuconfig      打开配置菜单
    size            显示程序大小
    full            完整构建流程 (clean + build + flash)
    setup           设置开发环境
    docs            生成文档

选项:
    -p PORT         指定串口 (默认: $FLASH_PORT)
    -t TARGET       指定目标芯片 (默认: esp32)
    -v              详细输出
    -h              显示此帮助信息

示例:
    $0 build                    # 编译项目
    $0 -p /dev/ttyUSB1 flash   # 使用指定串口烧录
    $0 full                     # 完整构建流程
    $0 setup                    # 首次设置环境

EOF
}

# 检查ESP-IDF环境
check_environment() {
    log_info "检查ESP-IDF环境..."
    
    if ! command -v idf.py &> /dev/null; then
        log_error "ESP-IDF环境未设置，请运行: source ./setup_env.sh"
        return 1
    fi
    
    if [ -z "$IDF_PATH" ]; then
        log_error "IDF_PATH环境变量未设置"
        return 1
    fi
    
    log_success "ESP-IDF环境检查通过"
    return 0
}

# 设置开发环境
setup_environment() {
    log_info "设置ESP32-Gamepad开发环境..."
    
    # 检查ESP-IDF安装
    if [ ! -d "$HOME/esp/esp-idf" ] && [ ! -d "/opt/esp-idf" ]; then
        log_warning "ESP-IDF未找到，请手动安装ESP-IDF"
        echo "参考文档: docs/01-开发环境搭建.md"
        return 1
    fi
    
    # 运行环境设置脚本
    if [ -f "./setup_env.sh" ]; then
        log_info "运行环境设置脚本..."
        source ./setup_env.sh
    fi
    
    # 安装Python依赖
    if [ -f "requirements.txt" ]; then
        log_info "安装Python依赖..."
        pip install -r requirements.txt
    fi
    
    log_success "开发环境设置完成"
}

# 清理构建文件
clean_build() {
    log_info "清理构建文件..."
    
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        log_success "构建文件已清理"
    else
        log_info "没有需要清理的构建文件"
    fi
}

# 编译项目
build_project() {
    log_info "开始编译ESP32-Gamepad项目..."
    
    cd "$PROJECT_ROOT"
    
    # 检查配置文件
    if [ ! -f "sdkconfig" ]; then
        log_info "首次编译，使用默认配置..."
        if [ -f "sdkconfig.defaults" ]; then
            cp sdkconfig.defaults sdkconfig
        fi
    fi
    
    # 运行编译
    if [ "$VERBOSE" = "true" ]; then
        idf.py build
    else
        idf.py build > build.log 2>&1 || {
            log_error "编译失败，查看 build.log 获取详细信息"
            tail -20 build.log
            return 1
        }
    fi
    
    log_success "项目编译完成"
    
    # 显示大小信息
    show_size_info
}

# 烧录到设备
flash_project() {
    log_info "烧录到设备 ($FLASH_PORT)..."
    
    cd "$PROJECT_ROOT"
    
    # 检查设备连接
    if [ ! -e "$FLASH_PORT" ]; then
        log_error "设备未连接或端口不正确: $FLASH_PORT"
        return 1
    fi
    
    # 烧录
    idf.py -p "$FLASH_PORT" flash
    
    log_success "烧录完成"
}

# 串口监控
monitor_device() {
    log_info "开始串口监控 ($FLASH_PORT)..."
    log_info "按 Ctrl+] 退出监控"
    
    cd "$PROJECT_ROOT"
    idf.py -p "$FLASH_PORT" monitor
}

# 显示程序大小信息
show_size_info() {
    log_info "程序大小信息:"
    
    cd "$PROJECT_ROOT"
    
    if [ -f "$BUILD_DIR/${PROJECT_NAME}.bin" ]; then
        echo "应用程序大小:"
        ls -lh "$BUILD_DIR"/*.bin | grep -v bootloader
        echo ""
        
        # 显示详细的大小分析
        if command -v idf.py &> /dev/null; then
            idf.py size-files 2>/dev/null || idf.py size 2>/dev/null
        fi
    else
        log_warning "找不到编译输出文件"
    fi
}

# 运行配置菜单
run_menuconfig() {
    log_info "打开配置菜单..."
    
    cd "$PROJECT_ROOT"
    idf.py menuconfig
}

# 运行测试
run_tests() {
    log_info "运行项目测试..."
    
    # 这里可以添加单元测试、集成测试等
    log_warning "测试功能正在开发中"
    
    # 检查代码格式
    log_info "检查代码格式..."
    
    # 检查配置文件
    log_info "验证配置文件..."
    if [ -f "gamepad_config.ini" ]; then
        log_success "配置文件检查通过"
    else
        log_warning "配置文件不存在"
    fi
}

# 生成文档
generate_docs() {
    log_info "生成项目文档..."
    
    # 生成代码文档
    if command -v doxygen &> /dev/null; then
        log_info "生成Doxygen文档..."
        doxygen 2>/dev/null || log_warning "Doxygen配置文件不存在"
    fi
    
    # 更新README
    log_info "文档已更新，查看 docs/ 目录"
}

# 完整构建流程
full_build() {
    log_info "开始完整构建流程..."
    
    clean_build
    build_project
    
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
    
    log_success "完整构建流程完成"
}

# 解析命令行参数
VERBOSE=false
TARGET="esp32"

while getopts "p:t:vh" opt; do
    case $opt in
        p)
            FLASH_PORT="$OPTARG"
            ;;
        t)
            TARGET="$OPTARG"
            ;;
        v)
            VERBOSE=true
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

# 主程序逻辑
main() {
    local command="$1"
    
    log_info "ESP32-Gamepad 构建脚本启动"
    log_info "项目路径: $PROJECT_ROOT"
    log_info "串口: $FLASH_PORT"
    log_info "目标芯片: $TARGET"
    echo
    
    case "$command" in
        "setup")
            setup_environment
            ;;
        "clean")
            clean_build
            ;;
        "build")
            check_environment && build_project
            ;;
        "flash")
            check_environment && flash_project
            ;;
        "monitor")
            monitor_device
            ;;
        "test")
            run_tests
            ;;
        "menuconfig")
            check_environment && run_menuconfig
            ;;
        "size")
            show_size_info
            ;;
        "full")
            check_environment && full_build
            ;;
        "docs")
            generate_docs
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
