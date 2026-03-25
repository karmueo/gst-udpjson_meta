# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

`gst-udpjson_meta` 是一个 GStreamer 插件，通过 UDP 多播接收 JSON 元数据并附加到 DeepStream 元数据流，同时支持 C-UAV 光电设备报文协议解析。

## 编译命令

```bash
cd /home/nvidia/work/deepstream-app-custom/src/gst-udpjson_meta
cmake -B build -S .
cmake --build build
sudo cmake --install build
```

## 核心组件

| 文件 | 用途 |
|------|------|
| `gstudpjsonmeta.cpp` | GStreamer 插件主体，实现 `GstBaseTransform`，处理 UDP 接收和元数据附加 |
| `gstudpjsonmeta_cuav.cpp` | C-UAV 协议解析器实现，支持引导信息、光电系统参数、伺服控制等报文 |
| `gstudpjsonmeta_cuav.h` | C-UAV 协议数据结构定义（报文头、引导信息、光电参数、伺服控制） |
| `gstudpjsonmeta.h` | 插件公开 API（回调设置函数、属性配置） |
| `python_tools/` | C-UAV 协议测试工具（emulator、sender、receiver） |

## C-UAV 协议报文体系

**报文分类**：
- `0x71xx` - 目标跟踪类：引导信息(0x7111)、目标信息(0x7112/0x7113)
- `0x72xx` - 光电控制类：系统参数(0x7201)、伺服控制(0x7204)、跟踪控制(0x7203)

**关键报文类型**：
| ID | 名称 | 用途 |
|----|------|------|
| 0x7111 | 引导信息 | 目标位置、速度、角度、距离等导航数据 |
| 0x7201 | 光电系统参数 | 伺服状态、可见光/红外焦距、跟踪状态 |
| 0x7204 | 光电伺服控制 | 水平/垂直角度/速度控制 |

**协议解析流程**：
`UDP 数据` → `JSON 解析` → `CUAVParser` → `根据 msg_id 分发到对应回调`

## 插件属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `multicast-ip` | string | UDP 组播地址 |
| `port` | uint | UDP 组播端口 |
| `iface` | string | 绑定网卡名 |
| `recv-buf-size` | uint | 接收缓冲区大小 |
| `cache-ttl-ms` | uint | 缓存有效期(毫秒) |
| `max-cache-size` | uint | 最大缓存条目数 |
| `enable-cuav-parser` | boolean | 启用 C-UAV 协议解析 |
| `cuav-multicast-port` | uint | C-UAV 组播端口 |
| `cuav-ctrl-port` | uint | C-UAV 控制/引导端口 |
| `cuav-debug` | boolean | C-UAV 调试打印 |

## 回调 API

插件提供三个回调注册接口，用于接收解析后的 C-UAV 数据：
- `gst_udpjson_meta_set_guidance_callback()` - 注册引导信息回调
- `gst_udpjson_meta_set_eo_system_callback()` - 注册光电系统参数回调
- `gst_udpjson_meta_set_servo_control_callback()` - 注册伺服控制回调

## 测试工具

```bash
# 模拟光电设备发送 C-UAV 报文
python python_tools/cuav_sender.py --port 5000

# 接收并解析 C-UAV 报文
python python_tools/cuav_receiver.py --port 5000

# 端到端测试
python python_tools/test_cuav_e2e.py

# 发送引导信息测试
python python_tools/send_guidance_test.py
```

## 架构设计

```
gstudpjsonmeta.cpp
├── GstUdpJsonMeta (GstBaseTransform)
│   ├── UDP 接收线程 (recv_thread)
│   ├── C-UAV 解析器 (cuav_parser)
│   └── 元数据缓存 (cache, cache_lock)
│
gstudpjsonmeta_cuav.cpp
├── CUAVParser (解析器主体)
├── cuav_parser_parse() - 入口函数
└── cuav_parser_handle_xxx() - 各报文类型处理
```

## 依赖

- GStreamer 1.0 (`gstreamer-1.0`, `gstreamer-base-1.0`)
- JSON-GLib (`json-glib-1.0`)
- NVIDIA DeepStream SDK (`nvdsgst_helper`, `nvdsgst_meta`, `nvds_meta`)
