#!/usr/bin/env python3
"""
C-UAV 协议 UDP 组播通信工具包

提供基于 C-UAV_PROTOCOL.md 协议的 UDP 组播报文发送和接收功能

主要模块:
- cuav_sender: 组播报文发送器
- cuav_receiver: 组播报文接收解析器
- cuav_emulator: 光电设备模拟器
- demo: 示例程序
"""

from .cuav_sender import CUAVMulticastSender
from .cuav_receiver import CUAVMulticastReceiver, CUAVMessageParser
from .cuav_emulator import CUAVEmulator, CUAVEmulatorWithTargetStream

__version__ = "1.0.0"
__author__ = "C-UAV"

__all__ = [
    "CUAVMulticastSender",
    "CUAVMulticastReceiver",
    "CUAVMessageParser",
    "CUAVEmulator",
    "CUAVEmulatorWithTargetStream",
]
