# C-UAV Python 工具包

本目录包含基于 `C-UAV_PROTOCOL.md` 协议的 UDP 组播通信 Python 工具。

## 目录结构

```
python_tools/
├── __init__.py         # 包初始化
├── cuav_sender.py      # 组播报文发送模块
├── cuav_receiver.py    # 组播报文接收解析模块
├── cuav_emulator.py    # 光电设备模拟器
├── demo.py             # 示例程序
└── README.md           # 本文档
```

## 快速开始

### 安装依赖

```bash
pip install -r requirements.txt
# 或直接使用（无外部依赖）
```

### 发送报文

```python
from cuav_sender import CUAVMulticastSender

# 创建发送器
sender = CUAVMulticastSender(
    multicast_addr="230.1.88.51",
    multicast_port=8003
)

# 发送引导信息
sender.send_guidance(
    tar_id=1,
    tar_category=9,  # 无人机
    guid_stat=1,
    ecef_x=-1245634.5,
    ecef_y=5298212.3,
    ecef_z=2997512.1,
    lon=116.397,
    lat=39.916,
    alt=5000
)

# 发送跟踪控制
sender.send_tracking_control(trk_str=1, trk_mod=0)

sender.close()
```

### 接收报文

```python
from cuav_receiver import CUAVMulticastReceiver, CUAVMessageParser

# 创建接收器
receiver = CUAVMulticastReceiver(
    multicast_addr="230.1.88.51",
    multicast_port=8013
)

# 注册消息处理器
def on_message(result, addr):
    msg_id = result.get("msg_id")
    print(f"收到报文: 0x{msg_id:04X}")

receiver.add_handler("any", on_message)

# 开始接收
receiver.start()

# 保持运行
import time
while True:
    time.sleep(1)
```

## 示例程序

本工具包提供四个演示程序，分别演示不同的通信场景，适用于开发调试和功能验证。

### 发送演示

```bash
python demo.py send
```

**功能说明：**

发送演示程序展示了如何使用 `CUAVMulticastSender` 向上位机（指控中心）发送各种控制指令。该程序按顺序发送以下报文：

| 序号 | 报文类型 | 报文内容 | 用途 |
|------|----------|----------|------|
| 1 | 引导信息 | 目标批号=1、类别=无人机、ECEF坐标、经纬高等 | 模拟指控中心向光电设备下发目标引导信息，使光电转台指向目标方向 |
| 2 | 查询指令 | cmd_id=0, cmd_coef1=1 (查询BIT状态) | 查询光电设备的内置自检状态，了解设备当前健康情况 |
| 3 | 跟踪控制 | trk_str=1, trk_mod=0, det_trk=1 | 控制光电开始跟踪目标，设置自动跟踪模式、识别模式 |
| 4 | 伺服控制 | mode_h=0, mode_v=0, speed=100, loc_h=180°, loc_v=30° | 手动控制光电转台水平转动到180°、俯仰30°，速度100 |

**使用场景：**

- 开发阶段验证发送接口是否正常
- 调试光电设备响应是否符合预期
- 测试不同指令参数的效果
- 验证报文格式是否符合协议规范

**输出示例：**

```
=== C-UAV 组播发送演示 ===

1. 发送引导信息...
2. 发送查询指令 (查询BIT状态)...
3. 发送跟踪控制...
4. 发送伺服控制 (手动模式)...

发送完成！
```

---

### 接收演示

```bash
python demo.py receive
```

**功能说明：**

接收演示程序展示了如何使用 `CUAVMulticastReceiver` 接收并解析来自光电设备的报文。该程序：

1. 监听组播地址 `230.1.88.51:8013`（光电→指控反馈端口）
2. 接收所有类型的报文并自动解析
3. 打印报文的详细信息，包括：
   - 报文来源地址
   - 报文类型（控制/回馈/查询/数据流）
   - 报文ID及名称
   - 报文序号
   - 时间戳
   - 具体内容（JSON格式）

**报文处理器：**

| 处理器类型 | 触发条件 | 说明 |
|------------|----------|------|
| any | 所有报文 | 打印所有收到的报文 |
| ctrl | msg_type=0 | 打印控制类报文 |
| feedback | msg_type=1 | 打印回馈类报文 |
| stream | msg_type=3 | 打印数据流类报文 |

**使用场景：**

- 验证网络连通性，确认能收到光电设备数据
- 查看光电设备上报的真实报文格式
- 调试报文解析逻辑是否正确
- 监控光电设备运行状态
- 捕获并分析异常报文

**输出示例：**

```
=== C-UAV 组播接收演示 ===

开始监听组播报文...

按 Ctrl+C 退出

=== 收到报文 ===
来源: ('192.168.1.100', 54321)
报文类型: 数据流 (3)
报文ID: 0x7112 (目标信息1)
序号: 1001
时间: 2026-02-03 10:30:45.123
内容: {
  "tar_id": 1,
  "tar_a": 45.5,
  "tar_e": 30.2,
  ...
}
```

---

### 双向通信

```bash
python demo.py bidirectional
```

**功能说明：**

双向通信演示程序展示了完整的"查询-应答"交互流程，是最接近实际业务的演示场景。该程序：

1. **启动接收器**：监听 `8013` 端口，准备接收光电设备的回馈报文
2. **启动发送器**：向 `8003` 端口发送查询指令
3. **等待回馈**：接收并解析光电设备返回的回馈报文
4. **显示结果**：打印回馈的设备参数信息

**交互流程：**

```
指控中心 (Python)                    光电设备
     |                                   |
     |  --- 查询指令 (port 8003) --->    |
     |                                   |
     |                                   |
     |  <--- 回馈报文 (port 8013) ---    |
     |    (设备配置参数/BIT状态等)       |
     |                                   |
```

**发送的报文：**

| 报文 | 类型 | 内容 |
|------|------|------|
| 查询指令 | msg_type=2 (查询) | cmd_id=0, cmd_coef1=1 (查询BIT状态) |

**预期收到的回馈：**

| 报文ID | 名称 | 说明 |
|--------|------|------|
| 0x7202 | 光电BIT状态 | 设备各组件的自检结果 |

**使用场景：**

- 验证"查询-回馈"业务流程完整性
- 测试光电设备的响应时间和数据正确性
- 调试双向通信的网络配置
- 验证多报文类型的处理逻辑
- 端到端集成测试

**输出示例：**

```
=== C-UAV 双向通信演示 ===

发送查询指令...
[反馈报文] {'sv_stat_h': 1, 'sv_stat_v': 1, 'pt_stat': 1, 'ir_stat': 1, ...}
```

---

### 目标信息上报

```bash
python demo.py target
```

**功能说明：**

目标信息上报演示程序模拟光电设备检测到目标后，持续向上位机上报目标信息。这是光电设备最核心的数据流功能，展示了如何构建和发送目标信息报文（0x7112）。

**程序行为：**

1. 模拟连续检测到5个目标（模拟多次检测帧）
2. 每个目标的位置信息逐渐变化（模拟目标运动）
3. 发送报文类型为 `msg_type=3`（数据流）
4. 报文内容包含目标完整信息

**发送的报文：**

| 字段 | 值 | 说明 |
|------|-----|------|
| msg_id | 0x7112 | 目标信息1 |
| msg_type | 3 | 数据流 |
| dev_id | 0 | 可见光 |
| tar_id | 1-5 | 目标批号（递增） |
| tar_category | 9 | 目标类型=无人机 |
| trk_stat | 1 | 跟踪正常 |
| tar_a | 45°-53° | 方位角（递增） |
| tar_e | 30° | 俯仰角 |
| tar_rng | 5000-5400m | 距离（递增） |
| tar_cfid | 0.95 | 置信度 |
| tar_rect | [100,100,200,200] | 目标图像位置 |

**数据率说明：**

根据协议规定，目标信息数据率应 ≥ 50Hz（即每秒至少发送50条目标信息），演示程序通过控制发送间隔模拟不同数据率。

**使用场景：**

- 模拟光电设备向指控中心上报检测结果
- 测试上位机接收目标信息的正确性
- 验证目标信息的报文格式是否符合协议
- 调试目标跟踪算法的数据输出
- 评估数据流在高频率下的传输稳定性

**输出示例：**

```
=== C-UAV 目标信息上报演示 ===

模拟发送目标信息 (5条)...

  发送目标 1: 方位=45.00°, 俯仰=30.00°, 距离=5000m
  发送目标 2: 方位=47.00°, 俯仰=30.00°, 距离=5100m
  发送目标 3: 方位=49.00°, 俯仰=30.00°, 距离=5200m
  发送目标 4: 方位=51.00°, 俯仰=30.00°, 距离=5300m
  发送目标 5: 方位=53.00°, 俯仰=30.00°, 距离=5400m

发送完成！
```

**与实际业务的关联：**

在真实应用中，`target` 演示的逻辑应集成到光电检测系统中：

```
光电检测系统
    |
    +---> 视频输入 ---> YOLOv11检测 ---> SOT跟踪
    |                                            |
    |                                            v
    +---> 目标信息提取 <--- 坐标转换 <--- 姿态解算
    |
    v
CUAVMulticastSender.send()  --->  230.1.88.51:8013  --->  指控中心
```

---

### 模拟器

```bash
python demo.py emulator
```

**功能说明：**

光电设备模拟器（`CUAVEmulator` / `CUAVEmulatorWithTargetStream`）用于在没有真实光电设备的情况下，模拟光电设备的完整行为：

1. **监听 8003 端口**：接收来自指控中心的控制指令和查询指令
2. **发送回馈报文**：对接收到的指令进行响应，返回设备状态
3. **上报目标流**（可选）：持续向 8013 端口上报模拟目标信息

**两种模拟器类型：**

| 类名 | 特点 |
|------|------|
| `CUAVEmulator` | 仅响应指令，不上报目标流 |
| `CUAVEmulatorWithTargetStream` | 响应指令 + 持续上报目标信息（10Hz） |

**使用场景：**

- 开发阶段替代真实光电设备进行联调测试
- 验证指控中心软件的功能完整性
- 调试报文收发逻辑和网络配置
- 模拟各种设备状态和目标场景

**使用方法（需要两个终端）：**

**终端1 - 启动模拟器：**

```bash
python demo.py emulator
```

**终端2 - 发送测试指令：**

```bash
python demo.py query
```

**模拟器支持的指令：**

| cmd_id | cmd_coef1 | 功能 |
|--------|-----------|------|
| 0 | 0 | 查询系统参数 |
| 0 | 1 | 查询BIT状态 |
| 0x7203 | - | 光电跟踪控制 |
| 0x7204 | - | 光电伺服控制 |
| 0x7205 | - | 可见光控制 |
| 0x7206 | - | 红外控制 |
| msg_id=0x7111 | - | 引导信息 |

**输出示例（模拟器端）：**

```
=== C-UAV 光电模拟器演示（无目标流）===

启动模拟器...
  控制指令监听: 230.1.88.51:8003
  反馈报文发送: 230.1.88.51:8013

等待指令...

1. 查询BIT状态...
  收到查询指令: cmd_id=0, cmd_coef=1
  [回馈] msg_id=0x7202
```

**代码使用示例：**

```python
from cuav_emulator import CUAVEmulatorWithTargetStream

# 创建模拟器
emulator = CUAVEmulatorWithTargetStream(
    control_addr="230.1.88.51",
    control_port=8003,
    feedback_addr="230.1.88.51",
    feedback_port=8013
)

# 设置设备参数
emulator.set_config(
    sys_id=1,
    dev_type=1,
    dev_id=1,
    lon=116.397,
    lat=39.916
)

# 启动
emulator.start()

# 保持运行
import time
while True:
    time.sleep(1)

# 停止
emulator.stop()
```

---

### 查询演示（带模拟器）

```bash
python demo.py query
```

**功能说明：**

此演示配合模拟器使用，向模拟器发送多种测试指令并接收回馈报文，全面测试协议交互功能。

**发送的测试指令：**

| 序号 | 指令类型 | 说明 |
|------|----------|------|
| 1 | 查询BIT状态 | cmd_id=0, cmd_coef1=1 |
| 2 | 查询系统参数 | cmd_id=0, cmd_coef1=0 |
| 3 | 跟踪控制 | trk_str=1, trk_mod=0 |
| 4 | 伺服控制 | loc_h=180°, loc_v=30° |
| 5 | 引导信息 | tar_id=1, guid_stat=1 |

**输出示例：**

```
=== C-UAV 查询演示（带模拟器）===

发送查询指令到模拟器...

1. 查询BIT状态...
...
等待回馈报文 (3秒)...

=== 收到报文 ===
报文类型: 回馈 (1)
报文ID: 0x7202 (光电BIT状态)
...
```

## 协议常量

### 报文ID

| ID | 名称 |
|----|------|
| 0x7101 | 指令 |
| 0x7102 | 设备配置参数 |
| 0x7111 | 引导信息 |
| 0x7112 | 目标信息1 |
| 0x7113 | 目标信息2 |
| 0x7201 | 光电系统参数 |
| 0x7202 | 光电BIT状态 |
| 0x7203 | 光电跟踪控制 |
| 0x7204 | 光电伺服控制 |
| ... | ... |

### 报文类型

| 类型 | 说明 |
|------|------|
| 0 | 控制 |
| 1 | 回馈 |
| 2 | 查询 |
| 3 | 数据流 |
| 100 | 初始化 |

### 目标类型

| 类型 | 说明 |
|------|------|
| 0 | 不明 |
| 1 | 鸟群 |
| 2 | 空飘物 |
| 3 | 飞机 |
| 4 | 汽车 |
| 7 | 行人 |
| 8 | 巡航导弹 |
| 9 | 无人机 |

## 端口说明

| 端口 | 方向 | 说明 |
|------|------|------|
| 8003 | 指控→光电 | 控制指令、查询、引导 |
| 8013 | 光电→指控 | 状态回馈、数据流 |

## API 参考

### CUAVMulticastSender

```python
class CUAVMulticastSender:
    def __init__(self, multicast_addr, multicast_port, local_addr="", ttl=1)
    def send_cmd(cmd_id, cmd_coef1=0, ...) -> int
    def send_query(cmd_id, cmd_coef1=0, ...) -> int
    def send_guidance(tar_id, tar_category, guid_stat, ...) -> int
    def send_tracking_control(...) -> int
    def send_servo_control(...) -> int
    def close()
```

### CUAVMulticastReceiver

```python
class CUAVMulticastReceiver:
    def __init__(self, multicast_addr, multicast_port, local_addr="", buffer_size=65535)
    def start(local_addr="")
    def add_handler(handler_type, handler)
    def remove_handler(handler_type, handler)
    def stop()
```

### CUAVMessageParser

```python
class CUAVMessageParser:
    def register_handler(msg_id, handler)
    def parse(raw_data) -> Optional[Dict]
    def get_msg_type_name(msg_type) -> str
    def get_msg_id_name(msg_id) -> str
    def get_target_type_name(target_type) -> str
```

### CUAVEmulator

```python
class CUAVEmulator:
    def __init__(self, control_addr, control_port, feedback_addr, feedback_port, local_addr="")
    def start()
    def stop()
    def add_handler(handler_type, handler)
    def set_config(**kwargs)
```

**功能：** 基础光电设备模拟器，支持接收指令并返回回馈报文，不上报目标流。

### CUAVEmulatorWithTargetStream

```python
class CUAVEmulatorWithTargetStream(CUAVEmulator):
    def start(stream_interval=0.1)  # stream_interval 为目标上报间隔（秒）
```

**功能：** 扩展模拟器，在基础功能上增加目标信息流上报功能，默认 10Hz 上报。

## 注意事项

1. 组播地址和端口可通过配置文件修改
2. 站址和设备ID通过组播装订
3. BIT状态周期性自动上报
4. 中心下发控制到设备，设备收到控制后需反馈
5. 默认设备未配置时：`sys_id=999, dev_type=999, dev_id=999`
