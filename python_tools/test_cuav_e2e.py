#!/usr/bin/env python3
"""
C-UAV 协议端到端测试脚本

测试内容：
1. 引导信息 (0x7111) 发送和接收
2. 光电系统参数 (0x7201) 接收
3. 光电伺服控制 (0x7204) 发送

使用方法:
    python3 test_cuav_e2e.py

测试流程：
1. 启动光电设备模拟器（监听8003，发送8013）
2. 启动接收器（监听8013）
3. 发送测试报文序列
4. 验证解析结果
"""

import json
import socket
import struct
import threading
import time
import sys
from datetime import datetime

# C-UAV 协议常量
MULTICAST_ADDR = "230.1.88.51"
CTRL_PORT = 8003      # 指控中心发送控制指令的端口
FEEDBACK_PORT = 8013  # 光电设备发送反馈的端口

MSG_ID_GUIDANCE = 0x7111
MSG_ID_EO_SYSTEM = 0x7201
MSG_ID_EO_SERVO = 0x7204

MSG_TYPE_CTRL = 0
MSG_TYPE_STREAM = 3


class CUAVTestReceiver:
    """C-UAV 报文接收器"""

    def __init__(self, multicast_addr=MULTICAST_ADDR, recv_port=FEEDBACK_PORT):
        self.multicast_addr = multicast_addr
        self.recv_port = recv_port
        self.sock = None
        self.running = False
        self.guidance_count = 0
        self.eo_system_count = 0
        self.servo_count = 0
        self.last_guidance = None
        self.last_eo_system = None

    def _setup_socket(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("0.0.0.0", self.recv_port))
        mreq = struct.pack("4s4s", socket.inet_aton(self.multicast_addr), b'\x00\x00\x00\x00')
        self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

    def start(self):
        self._setup_socket()
        self.running = True
        self._thread = threading.Thread(target=self._receive_loop, daemon=True)
        self._thread.start()
        print(f"[接收器] 开始监听 {self.multicast_addr}:{self.recv_port}")

    def _receive_loop(self):
        while self.running:
            try:
                data, addr = self.sock.recvfrom(65535)
                self._handle_message(data)
            except Exception:
                if self.running:
                    pass

    def _handle_message(self, data):
        try:
            raw_data = json.loads(data.decode('utf-8'))
            common = raw_data.get("公共内容", {})
            msg_id = common.get("msg_id", 0)

            # 解析具体信息
            if "具体信息" in raw_data:
                specific = raw_data["具体信息"]
            else:
                return

            if msg_id == MSG_ID_GUIDANCE:
                self.guidance_count += 1
                self.last_guidance = {
                    "tar_id": specific.get("tar_id"),
                    "guid_stat": specific.get("guid_stat"),
                    "enu_a": specific.get("enu_a"),
                    "enu_e": specific.get("enu_e")
                }
                print(f"[接收] 引导信息 #{self.guidance_count}: tar_id={self.last_guidance['tar_id']}, "
                      f"guid_stat={self.last_guidance['guid_stat']}")

            elif msg_id == MSG_ID_EO_SYSTEM:
                self.eo_system_count += 1
                self.last_eo_system = {
                    "sv_stat": specific.get("sv_stat"),
                    "st_loc_h": specific.get("st_loc_h"),
                    "st_loc_v": specific.get("st_loc_v"),
                    "pt_focal": specific.get("pt_focal"),
                    "ir_focal": specific.get("ir_focal")
                }
                if self.eo_system_count <= 5:  # 只打印前5条
                    print(f"[接收] 光电系统参数 #{self.eo_system_count}: "
                          f"伺服=({self.last_eo_system['st_loc_h']:.1f}°, "
                          f"{self.last_eo_system['st_loc_v']:.1f}°)")

        except json.JSONDecodeError:
            pass

    def stop(self):
        self.running = False
        if self.sock:
            self.sock.close()
        print(f"[接收器] 已停止, 收到 {self.guidance_count} 条引导信息, {self.eo_system_count} 条光电系统参数")


class CUAVTestSender:
    """C-UAV 报文发送器"""

    def __init__(self, multicast_addr=MULTICAST_ADDR):
        self.multicast_addr = multicast_addr
        self.sock = None
        self._msg_sn = 0

    def _setup_socket(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)

    def _get_timestamp(self):
        now = datetime.now()
        return {
            "yr": now.year, "mo": now.month, "dy": now.day,
            "h": now.hour, "min": now.minute, "sec": now.second,
            "msec": (time.time() % 1) * 1000
        }

    def _get_msg_sn(self):
        self._msg_sn += 1
        return self._msg_sn

    def start(self):
        self._setup_socket()
        print(f"[发送器] 启动，发送到 {self.multicast_addr}:{CTRL_PORT}")

    def stop(self):
        if self.sock:
            self.sock.close()
        print("[发送器] 已停止")

    def send_guidance(self, tar_id=1, guid_stat=1, enu_a=45.0, enu_e=30.0, enu_r=5000):
        """发送引导信息"""
        common = {
            "msg_id": MSG_ID_GUIDANCE,
            "msg_sn": self._get_msg_sn(),
            "msg_type": MSG_TYPE_CTRL,
            "tx_sys_id": 3, "tx_dev_type": 3, "tx_dev_id": 1, "tx_subdev_id": 999,
            "rx_sys_id": 999, "rx_dev_type": 1, "rx_dev_id": 1, "rx_subdev_id": 999,
            **self._get_timestamp(),
            "cont_type": 1, "cont_sum": 1
        }
        specific = {
            **self._get_timestamp(),
            "tar_id": tar_id, "tar_category": 9, "guid_stat": guid_stat,
            "ecef_x": -1245634.5, "ecef_y": 5298212.3, "ecef_z": 2997512.1,
            "ecef_vx": 100.5, "ecef_vy": -50.2, "ecef_vz": 0.0,
            "enu_r": enu_r, "enu_a": enu_a, "enu_e": enu_e,
            "lon": 116.397, "lat": 39.916, "alt": 5000
        }
        msg = {"公共内容": common, "具体信息": specific}
        self.sock.sendto(json.dumps(msg, ensure_ascii=False).encode('utf-8'),
                         (self.multicast_addr, CTRL_PORT))
        print(f"[发送] 引导信息: tar_id={tar_id}, guid_stat={guid_stat}, enu=({enu_a:.1f}, {enu_e:.1f})")

    def send_servo_control(self, loc_h=90.0, loc_v=45.0):
        """发送伺服控制"""
        common = {
            "msg_id": 0x7101,
            "msg_sn": self._get_msg_sn(),
            "msg_type": MSG_TYPE_CTRL,
            "tx_sys_id": 3, "tx_dev_type": 3, "tx_dev_id": 1, "tx_subdev_id": 999,
            "rx_sys_id": 999, "rx_dev_type": 1, "rx_dev_id": 1, "rx_subdev_id": 999,
            **self._get_timestamp(),
            "cont_type": 0, "cont_sum": 1
        }
        specific = {
            "cmd_id": MSG_ID_EO_SERVO, "cmd_coef1": 0,
            "dev_id": 2, "dev_en": 1, "ctrl_en": 1,
            "mode_h": 0, "mode_v": 0,
            "speed_en_h": 1, "speed_h": 100,
            "speed_en_v": 1, "speed_v": 100,
            "loc_en_h": 1, "loc_h": loc_h,
            "loc_en_v": 1, "loc_v": loc_v,
            "offset_en": 0
        }
        msg = {"公共内容": common, "具体信息": specific}
        self.sock.sendto(json.dumps(msg, ensure_ascii=False).encode('utf-8'),
                         (self.multicast_addr, CTRL_PORT))
        print(f"[发送] 伺服控制: 水平={loc_h:.1f}°, 垂直={loc_v:.1f}°")


def run_test():
    """运行测试"""
    print("=" * 60)
    print("C-UAV 协议端到端测试")
    print("=" * 60)

    receiver = CUAVTestReceiver()
    sender = CUAVTestSender()

    receiver.start()
    sender.start()

    time.sleep(1)  # 等待稳定

    # 测试1: 发送引导信息
    print("\n--- 测试1: 发送引导信息 ---")
    sender.send_guidance(tar_id=1, guid_stat=1, enu_a=45.0, enu_e=30.0)
    time.sleep(0.3)
    sender.send_guidance(tar_id=2, guid_stat=1, enu_a=90.0, enu_e=45.0)
    time.sleep(0.3)
    sender.send_guidance(tar_id=1, guid_stat=0)  # 取消引导
    time.sleep(0.3)

    # 测试2: 发送伺服控制
    print("\n--- 测试2: 发送伺服控制 ---")
    sender.send_servo_control(loc_h=120.0, loc_v=60.0)
    time.sleep(0.3)
    sender.send_servo_control(loc_h=180.0, loc_v=30.0)
    time.sleep(0.3)

    # 等待接收
    print("\n--- 等待接收完成 ---")
    time.sleep(2)

    # 统计结果
    print("\n" + "=" * 60)
    print("测试结果:")
    print(f"  收到引导信息: {receiver.guidance_count} 条")
    print(f"  收到光电系统参数: {receiver.eo_system_count} 条")
    print("=" * 60)

    receiver.stop()
    sender.stop()

    # 返回成功/失败
    success = receiver.guidance_count > 0 and receiver.eo_system_count > 0
    return success


if __name__ == "__main__":
    try:
        success = run_test()
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        print("\n测试被用户中断")
        sys.exit(1)
    except Exception as e:
        print(f"\n测试失败: {e}")
        sys.exit(1)
