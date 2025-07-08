#!/bin/bash

# ESP32-Gamepad 环境设置脚本
# 使用方法: source setup_env.sh

echo "🔧 ESP32-Gamepad 环境设置脚本"
echo "================================="

# 检查ESP-IDF是否已安装
IDF_PATH_DEFAULT="$HOME/esp/esp-idf"

if [ -z "$IDF_PATH" ]; then
    if [ -d "$IDF_PATH_DEFAULT" ]; then
        echo "📁 找到ESP-IDF安装目录: $IDF_PATH_DEFAULT"
        export IDF_PATH="$IDF_PATH_DEFAULT"
    else
        echo "❌ 未找到ESP-IDF安装目录"
        echo "请先安装ESP-IDF，参考 docs/03-快速开始指南.md"
        return 1
    fi
fi

# 设置ESP-IDF环境
if [ -f "$IDF_PATH/export.sh" ]; then
    echo "🚀 设置ESP-IDF环境..."
    source "$IDF_PATH/export.sh"
    echo "✅ ESP-IDF环境设置完成"
    echo "📋 ESP-IDF版本: $(idf.py --version)"
else
    echo "❌ 未找到ESP-IDF export.sh文件"
    return 1
fi

# 显示项目信息
echo ""
echo "🎮 ESP32-Gamepad 项目信息:"
echo "   项目目录: $(pwd)"
echo "   ESP-IDF路径: $IDF_PATH"
echo "   目标芯片: ESP32"
echo ""

# 显示可用命令
echo "📋 可用命令:"
echo "   idf.py set-target esp32    # 设置目标芯片"
echo "   idf.py menuconfig         # 配置项目"
echo "   idf.py build              # 编译项目"
echo "   idf.py flash              # 烧录程序"
echo "   idf.py monitor            # 监控串口"
echo "   idf.py clean              # 清理构建"
echo ""

# 检查串口设备
echo "🔌 可用串口设备:"
ls /dev/cu.* 2>/dev/null | head -5 || echo "   未找到串口设备"
echo ""

echo "✅ 环境设置完成！现在可以开始开发了。"
