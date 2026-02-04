#!/usr/bin/env python3
"""
C-UAV 协议 UDP 组播发送模块

提供基于 C-UAV_PROTOCOL.md 协议的 UDP 组播报文发送功能
"""

import json
import socket
import time
from datetime import datetime
from typing import Any, Dict, List, Optional


class CUAVMulticastSender:
    """C-UAV 协议 UDP 组播发送器"""

    DEFAULT_MULTICAST_ADDR = "230.1.88.51"
    DEFAULT_MULTICAST_PORT = 8003

    # 报文ID常量
    MSG_ID_CMD = 0x7101          # 指令
    MSG_ID_DEV_CONFIG = 0x7102   # 设备配置参数
    MSG_ID_GUIDANCE = 0x7111     # 引导信息
    MSG_ID_TARGET1 = 0x7112      # 目标信息1
    MSG_ID_TARGET2 = 0x7113      # 目标信息2

    # 报文类型
    MSG_TYPE_CTRL = 0        # 控制
    MSG_TYPE_FEEDBACK = 1    # 回馈
    MSG_TYPE_QUERY = 2       # 查询
    MSG_TYPE_STREAM = 3      # 数据流
    MSG_TYPE_INIT = 100      # 初始化

    # 设备类型
    DEV_TYPE_RADAR = 0
    DEV_TYPE_EO = 1          # 光电
    DEV_TYPE_RECEIVER = 2
    DEV_TYPE_FUSION = 3
    DEV_TYPE_UAV = 4
    DEV_TYPE_ADSB = 5
    DEV_TYPE_AIS = 6
    DEV_TYPE_JAMMING = 7

    def __init__(
        self,
        multicast_addr: str = DEFAULT_MULTICAST_ADDR,
        multicast_port: int = DEFAULT_MULTICAST_PORT,
        local_addr: str = "",
        ttl: int = 1
    ):
        """
        初始化发送器

        Args:
            multicast_addr: 组播地址
            multicast_port: 组播端口
            local_addr: 本地地址（留空则自动获取）
            ttl: 组播TTL值
        """
        self.multicast_addr = multicast_addr
        self.multicast_port = multicast_port
        self._msg_sn = 0

        # 创建UDP socket
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        self._sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65535)

        if local_addr:
            self._sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF,
                                   socket.inet_aton(local_addr))

    def _get_timestamp(self) -> Dict[str, Any]:
        """获取当前时间戳"""
        now = datetime.now()
        return {
            "yr": now.year,
            "mo": now.month,
            "dy": now.day,
            "h": now.hour,
            "min": now.minute,
            "sec": now.second,
            "msec": now.microsecond / 1000.0
        }

    def _build_common_header(
        self,
        msg_id: int,
        msg_type: int,
        tx_sys_id: int = 999,
        tx_dev_type: int = 1,
        tx_dev_id: int = 999,
        tx_subdev_id: int = 999,
        rx_sys_id: int = 999,
        rx_dev_type: int = 999,
        rx_dev_id: int = 999,
        rx_subdev_id: int = 999,
        cont_type: int = 0,
        cont_sum: int = 1
    ) -> Dict[str, Any]:
        """
        构建公共报文头

        Args:
            msg_id: 报文ID
            msg_type: 报文类型
            tx_*: 发送方信息
            rx_*: 接收方信息
            cont_type: 信息类型
            cont_sum: 信息数量
        """
        self._msg_sn += 1
        return {
            "msg_id": msg_id,
            "msg_sn": self._msg_sn,
            "msg_type": msg_type,
            "tx_sys_id": tx_sys_id,
            "tx_dev_type": tx_dev_type,
            "tx_dev_id": tx_dev_id,
            "tx_subdev_id": tx_subdev_id,
            "rx_sys_id": rx_sys_id,
            "rx_dev_type": rx_dev_type,
            "rx_dev_id": rx_dev_id,
            "rx_subdev_id": rx_subdev_id,
            **self._get_timestamp(),
            "cont_type": cont_type,
            "cont_sum": cont_sum
        }

    def send(
        self,
        common: Dict[str, Any],
        specific: Dict[str, Any],
        cont_type: int = 0
    ) -> int:
        """
        发送报文

        Args:
            common: 公共内容
            specific: 具体信息
            cont_type: 信息类型

        Returns:
            发送的字节数
        """
        if cont_type == 0:
            msg = {"公共内容": common, "具体信息": specific}
        else:
            # 多信息格式
            if isinstance(specific, list):
                cont = [{"具体信息": item} for item in specific]
            else:
                cont = [{"具体信息": specific}]
            msg = {"公共内容": common, "cont": cont}

        data = json.dumps(msg, ensure_ascii=False)
        return self._sock.sendto(data.encode('utf-8'), (self.multicast_addr, self.multicast_port))

    def send_cmd(
        self,
        cmd_id: int,
        cmd_coef1: int = 0,
        rx_dev_type: int = 999,
        rx_dev_id: int = 999
    ) -> int:
        """
        发送控制指令

        Args:
            cmd_id: 指令ID
            cmd_coef1: 指令参数1
            rx_dev_type: 接收方设备类型
            rx_dev_id: 接收方设备ID

        Returns:
            发送的字节数
        """
        common = self._build_common_header(
            self.MSG_ID_CMD,
            self.MSG_TYPE_CTRL,
            rx_dev_type=rx_dev_type,
            rx_dev_id=rx_dev_id
        )
        specific = {"cmd_id": cmd_id, "cmd_coef1": cmd_coef1}
        return self.send(common, specific)

    def send_query(
        self,
        cmd_id: int,
        cmd_coef1: int = 0,
        rx_dev_type: int = 999,
        rx_dev_id: int = 999
    ) -> int:
        """
        发送查询指令

        Args:
            cmd_id: 指令ID
            cmd_coef1: 指令参数1
            rx_dev_type: 接收方设备类型
            rx_dev_id: 接收方设备ID

        Returns:
            发送的字节数
        """
        common = self._build_common_header(
            self.MSG_ID_CMD,
            self.MSG_TYPE_QUERY,
            rx_dev_type=rx_dev_type,
            rx_dev_id=rx_dev_id
        )
        specific = {"cmd_id": cmd_id, "cmd_coef1": cmd_coef1}
        return self.send(common, specific)

    def send_guidance(
        self,
        tar_id: int,
        tar_category: int,
        guid_stat: int,
        ecef_x: float,
        ecef_y: float,
        ecef_z: float,
        ecef_vx: float = 0,
        ecef_vy: float = 0,
        ecef_vz: float = 0,
        lon: float = 0,
        lat: float = 0,
        alt: float = 0,
        enu_a: float = 0,
        enu_e: float = 0,
        enu_r: float = 0,
        rx_sys_id: int = 999,
        rx_dev_type: int = 1,
        rx_dev_id: int = 999
    ) -> int:
        """
        发送引导信息

        Args:
            tar_id: 引导批号
            tar_category: 目标类别
            guid_stat: 目标状态
            ecef_*: 地心坐标
            lon/lat/alt: 经纬高
            enu_a/enu_e/enu_r: 方位角/俯仰角/距离
            rx_*: 接收方信息

        Returns:
            发送的字节数
        """
        common = self._build_common_header(
            self.MSG_ID_GUIDANCE,
            self.MSG_TYPE_CTRL,
            rx_sys_id=rx_sys_id,
            rx_dev_type=rx_dev_type,
            rx_dev_id=rx_dev_id,
            cont_type=1
        )
        specific = {
            **self._get_timestamp(),
            "tar_id": tar_id,
            "tar_category": tar_category,
            "guid_stat": guid_stat,
            "ecef_x": ecef_x,
            "ecef_y": ecef_y,
            "ecef_z": ecef_z,
            "ecef_vx": ecef_vx,
            "ecef_vy": ecef_vy,
            "ecef_vz": ecef_vz,
            "enu_r": enu_r,
            "enu_a": enu_a,
            "enu_e": enu_e,
            "lon": lon,
            "lat": lat,
            "alt": alt
        }
        return self.send(common, specific, cont_type=1)

    def send_tracking_control(
        self,
        trk_end: int = 1,
        pt_trk_link: int = 1,
        ir_trk_link: int = 1,
        trk_str: int = 1,
        trk_dev: int = 3,
        trk_mod: int = 0,
        det_trk: int = 1,
        rx_sys_id: int = 999,
        rx_dev_type: int = 1,
        rx_dev_id: int = 999
    ) -> int:
        """
        发送光电跟踪控制

        Args:
            trk_end: 跟踪模块开关
            pt_trk_link: 光电联动
            ir_trk_link: 红外联动
            trk_str: 跟踪开关
            trk_dev: 跟踪设备
            trk_mod: 跟踪模式
            det_trk: 检测跟踪

        Returns:
            发送的字节数
        """
        common = self._build_common_header(
            self.MSG_ID_CMD,  # 使用CMD报文ID
            self.MSG_TYPE_CTRL,
            rx_sys_id=rx_sys_id,
            rx_dev_type=rx_dev_type,
            rx_dev_id=rx_dev_id
        )
        specific = {
            "cmd_id": 0x7203,  # 跟踪控制报文ID
            "cmd_coef1": 0,
            "trk_end": trk_end,
            "pt_trk_link": pt_trk_link,
            "ir_trk_link": ir_trk_link,
            "trk_str": trk_str,
            "trk_dev": trk_dev,
            "trk_mod": trk_mod,
            "det_trk": det_trk
        }
        return self.send(common, specific)

    def send_servo_control(
        self,
        dev_id: int = 2,
        dev_en: int = 1,
        ctrl_en: int = 1,
        mode_h: int = 0,
        mode_v: int = 0,
        speed_h: int = 50,
        speed_v: int = 50,
        loc_h: float = 0,
        loc_v: float = 0,
        rx_sys_id: int = 999,
        rx_dev_type: int = 1,
        rx_dev_id: int = 999
    ) -> int:
        """
        发送光电伺服控制

        Args:
            dev_id: 设备类型
            dev_en: 使能
            ctrl_en: 控制使能
            mode_h/v: 控制模式
            speed_h/v: 速度
            loc_h/v: 位置

        Returns:
            发送的字节数
        """
        common = self._build_common_header(
            self.MSG_ID_CMD,
            self.MSG_TYPE_CTRL,
            rx_sys_id=rx_sys_id,
            rx_dev_type=rx_dev_type,
            rx_dev_id=rx_dev_id
        )
        specific = {
            "cmd_id": 0x7204,
            "cmd_coef1": 0,
            "dev_id": dev_id,
            "dev_en": dev_en,
            "ctrl_en": ctrl_en,
            "mode_h": mode_h,
            "mode_v": mode_v,
            "speed_en_h": 3,  # 增加
            "speed_h": speed_h,
            "speed_en_v": 3,
            "speed_v": speed_v,
            "loc_en_h": 1,  # 设置
            "loc_h": loc_h,
            "loc_en_v": 1,
            "loc_v": loc_v,
            "offset_en": 0
        }
        return self.send(common, specific)

    def close(self):
        """关闭socket"""
        if self._sock:
            self._sock.close()
            self._sock = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False


def demo_send_guidance():
    """演示发送引导信息"""
    with CUAVMulticastSender() as sender:
        # 发送引导信息
        sender.send_guidance(
            tar_id=1,
            tar_category=9,  # 无人机
            guid_stat=1,     # 正常
            ecef_x=-1245634.5,
            ecef_y=5298212.3,
            ecef_z=2997512.1,
            ecef_vx=100.5,
            ecef_vy=-50.2,
            ecef_vz=0.0,
            lon=116.397,
            lat=39.916,
            alt=5000
        )
        print("引导信息已发送")

        time.sleep(0.1)

        # 发送跟踪控制
        sender.send_tracking_control(
            trk_str=1,
            trk_mod=0,
            det_trk=1
        )
        print("跟踪控制已发送")

        time.sleep(0.1)

        # 发送伺服控制 - 指向方位角180，俯仰角30
        sender.send_servo_control(
            mode_h=0,  # 手动
            mode_v=0,  # 手动
            speed_h=100,
            speed_v=100,
            loc_h=180.0,
            loc_v=30.0
        )
        print("伺服控制已发送")


if __name__ == "__main__":
    demo_send_guidance()
