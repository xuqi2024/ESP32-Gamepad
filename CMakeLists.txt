# ESP-IDF项目最低版本要求
cmake_minimum_required(VERSION 3.16)

# 项目基本信息
set(PROJECT_NAME "esp32_gamepad")
set(PROJECT_VER "1.0.0")

# 包含ESP-IDF构建系统
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 定义项目
project(${PROJECT_NAME})

# 项目描述
set(PROJECT_DESCRIPTION "ESP32 Bluetooth Gamepad Controller for RC Cars and Planes")
