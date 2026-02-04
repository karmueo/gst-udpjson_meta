#!/usr/bin/env python3
"""
C-UAV 协议光电设备模拟器

模拟光电设备行为：
- 监听 8003 端口接收控制/查询指令
- 向 8013 端口发送回馈报文和数据流
"""

import json
import socket
import struct
import threading
import time
from datetime import datetime
from typing import Any, Callable, Dict, List, Optional


class CUAVEmulator:
    """
    C-UAV 光电设备模拟器

    模拟光电设备的基本行为，包括：
    - 接收控制指令和查询指令
    - 上报BIT状态和系统参数
    - 上报目标跟踪信息
    """

    # 设备配置（默认值）
    DEFAULT_CONFIG = {
        "sys_id": 1,
        "dev_type": 1,
        "dev_id": 1,
        "subdev_id": 1,
        "lon": 116.397,
        "lat": 39.916,
        "alt": 100,
        "version": "1.0.0"
    }

    def __init__(
        self,
        control_addr: str = "230.1.88.51",
        control_port: int = 8003,
        feedback_addr: str = "230.1.88.51",
        feedback_port: int = 8013,
        local_addr: str = ""
    ):
        """
        初始化模拟器

        Args:
            control_addr: 控制指令组播地址
            control_port: 控制指令端口 (8003)
            feedback_addr: 反馈组播地址
            feedback_port: 反馈端口 (8013)
            local_addr: 本地地址
        """
        self.control_addr = control_addr
        self.control_port = control_port
        self.feedback_addr = feedback_addr
        self.feedback_port = feedback_port
        self.local_addr = local_addr

        self._running = False
        self._control_sock = None
        self._feedback_sock = None
        self._receive_thread = None
        self._lock = threading.Lock()  # 线程锁

        # 设备状态
        self.config = self.DEFAULT_CONFIG.copy()
        self.servo_pos_h = 0.0   # 水平指向
        self.servo_pos_v = 0.0   # 垂直指向
        self.pt_focal = 1000.0   # 可见光焦距
        self.ir_focal = 1000.0   # 红外焦距
        self.tracking = False
        self.target_id = 0

        # 报文序号
        self._msg_sn = 0

        # 消息处理器
        self._handlers: Dict[str, List[Callable]] = {
            "cmd": [],       # 处理控制指令
            "query": [],     # 处理查询指令
            "guidance": []   # 处理引导信息
        }

    def _create_control_socket(self):
        """创建控制指令监听socket"""
        self._control_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        self._control_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        # 绑定组播地址
        if self.local_addr:
            self._control_sock.bind((self.local_addr, self.control_port))
        else:
            self._control_sock.bind(("0.0.0.0", self.control_port))

        # 加入组播组
        mreq = struct.pack(
            "4s4s",
            socket.inet_aton(self.control_addr),
            b'\x00\x00\x00\x00'
        )
        self._control_sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

    def _create_feedback_socket(self):
        """创建反馈发送socket"""
        self._feedback_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        self._feedback_sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)

    def _get_timestamp(self) -> Dict[str, Any]:
        """获取时间戳"""
        now = datetime.now()
        return {
            "yr": now.year,
            "mo": now.month,
            "dy": now.day,
            "h": now.hour,
            "min": now.minute,
            "sec": now.second,
            "msec": (time.time() % 1) * 1000
        }

    def _get_msg_sn(self) -> int:
        """获取报文序号"""
        self._msg_sn += 1
        return self._msg_sn

    def _parse_specific(self, raw_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        解析具体信息
        支持单信息格式 (具体信息) 和多信息格式 (cont 数组)
        """
        # 先尝试单信息格式
        specific = raw_data.get("具体信息", {})
        if specific:
            return specific

        # 再尝试多信息格式
        cont = raw_data.get("cont", [])
        if cont and isinstance(cont, list) and len(cont) > 0:
            first_item = cont[0]
            if isinstance(first_item, dict) and "具体信息" in first_item:
                return first_item["具体信息"]

        return {}

    def _build_common_header(
        self,
        msg_id: int,
        msg_type: int = 1,  # 回馈
        cont_type: int = 0,
        cont_sum: int = 1,
        rx_sys_id: int = 999,
        rx_dev_type: int = 999,
        rx_dev_id: int = 999
    ) -> Dict[str, Any]:
        """构建公共报文头"""
        return {
            "msg_id": msg_id,
            "msg_sn": self._get_msg_sn(),
            "msg_type": msg_type,
            "tx_sys_id": self.config["sys_id"],
            "tx_dev_type": self.config["dev_type"],
            "tx_dev_id": self.config["dev_id"],
            "tx_subdev_id": self.config["subdev_id"],
            "rx_sys_id": rx_sys_id,
            "rx_dev_type": rx_dev_type,
            "rx_dev_id": rx_dev_id,
            "rx_subdev_id": 999,
            **self._get_timestamp(),
            "cont_type": cont_type,
            "cont_sum": cont_sum
        }

    def _send_feedback(self, common: Dict[str, Any], specific: Dict[str, Any]):
        """发送回馈报文"""
        msg = {"公共内容": common, "具体信息": specific}
        data = json.dumps(msg, ensure_ascii=False)
        self._feedback_sock.sendto(
            data.encode('utf-8'),
            (self.feedback_addr, self.feedback_port)
        )
        print(f"  [回馈] msg_id=0x{common['msg_id']:04X}")

    def _handle_query(self, data: Dict[str, Any]):
        """处理查询指令"""
        cmd_id = data.get("cmd_id", 0)
        cmd_coef1 = data.get("cmd_coef1", 0)

        print(f"  收到查询指令: cmd_id={cmd_id}, cmd_coef1={cmd_coef1}")

        # 调用注册的处理器
        for handler in self._handlers["query"]:
            handler(cmd_id, cmd_coef1)

        # 生成回馈报文
        if cmd_id == 0 and cmd_coef1 == 0:
            # 查询系统参数
            self._send_device_config()
        elif cmd_id == 0 and cmd_coef1 == 1:
            # 查询BIT状态
            self._send_bit_status()
        elif cmd_id == 1:
            # 查询各模块参数
            self._send_module_status(cmd_coef1)

    def _handle_cmd(self, data: Dict[str, Any]):
        """处理控制指令"""
        cmd_id = data.get("cmd_id", 0)

        print(f"  收到控制指令: cmd_id=0x{cmd_id:04X}")

        # 调用注册的处理器
        for handler in self._handlers["cmd"]:
            handler(cmd_id, data)

        # 根据cmd_id处理
        if cmd_id == 0x7203:
            # 光电跟踪控制
            self._handle_tracking_control(data)
        elif cmd_id == 0x7204:
            # 光电伺服控制
            self._handle_servo_control(data)
        elif cmd_id == 0x7205:
            # 可见光控制
            self._handle_pt_control(data)
        elif cmd_id == 0x7206:
            # 红外控制
            self._handle_ir_control(data)

    def _handle_guidance(self, data: Dict[str, Any]):
        """处理引导信息"""
        tar_id = data.get("tar_id", 0)
        guid_stat = data.get("guid_stat", 0)

        print(f"  收到引导信息: tar_id={tar_id}, guid_stat={guid_stat}")

        # 更新目标信息
        self.target_id = tar_id

        # 调用注册的处理器
        for handler in self._handlers["guidance"]:
            handler(data)

        # 发送引导确认回馈
        self._send_guidance_ack()

    def _send_device_config(self):
        """发送设备配置参数回馈"""
        common = self._build_common_header(msg_id=0x7102, msg_type=1)
        specific = {
            "sys_set": 2,  # 查询反馈
            "lon": self.config["lon"],
            "lat": self.config["lat"],
            "alt": self.config["alt"],
            "sys_id": self.config["sys_id"],
            "dev_type": self.config["dev_type"],
            "dev_id": self.config["dev_id"],
            "subdev_id": self.config["subdev_id"],
            "net_set": 2,
            "loc_ip": "",
            "loc_port": 0,
            "bit_mas_ip": "",
            "bit_mas_port": 0,
            "mas_ip": "",
            "mas_port": 0,
            "proc_set": 2,
            "nor_agle": 0,
            "azi_agle": 0,
            "elv_agle": 0,
            "trk_set": 2,
            "fb_set": 2,
            "version": self.config["version"]
        }
        self._send_feedback(common, specific)

    def _send_bit_status(self):
        """发送BIT状态回馈"""
        common = self._build_common_header(msg_id=0x7202, msg_type=1)
        specific = {
            "sv_stat_h": 1,
            "sv_stat_v": 1,
            "sv_pwr": 1,
            "sv_max_t": 45.0,
            "pt_stat": 1,
            "pt_err": 0,
            "pt_pwr": 1,
            "pt_max_t": 50.0,
            "pt_avg_t": 40.0,
            "ir_stat": 1,
            "ir_err": 0,
            "ir_pwr": 1,
            "ir_max_t": 45.0,
            "ir_avg_t": 38.0,
            "laster_stat": 1,
            "laster_err": 0,
            "laser_pwr": 1,
            "laser_max_t": 40.0,
            "laser_avg_t": 35.0,
            "wiper_stat": 0,
            "defrost_stat": 0,
            "ofr_stat": 0
        }
        self._send_feedback(common, specific)

    def _send_module_status(self, cmd_coef1: int):
        """发送模块状态回馈"""
        if cmd_coef1 == 1:
            self._send_bit_status()
        elif cmd_coef1 == 2:
            # 伺服控制状态
            common = self._build_common_header(msg_id=0x7204, msg_type=1)
            specific = {"mode_h": 0, "mode_v": 0, "speed_h": 100, "speed_v": 100}
            self._send_feedback(common, specific)

    def _send_guidance_ack(self):
        """发送引导确认"""
        print("  [引导] 目标已锁定，准备跟踪")

    def _handle_tracking_control(self, data: Dict[str, Any]):
        """处理跟踪控制"""
        trk_str = data.get("trk_str", 0)
        self.tracking = (trk_str == 1)
        print(f"  [跟踪] {'开始跟踪' if self.tracking else '停止跟踪'}")

    def _handle_servo_control(self, data: Dict[str, Any]):
        """处理伺服控制"""
        loc_en_h = data.get("loc_en_h", 0)
        loc_en_v = data.get("loc_en_v", 0)
        speed_en_h = data.get("speed_en_h", 0)
        speed_en_v = data.get("speed_en_v", 0)

        if loc_en_h in (1, 3, 4):
            self.servo_pos_h = data.get("loc_h", 0)
        if loc_en_v in (1, 3, 4):
            self.servo_pos_v = data.get("loc_v", 0)

        print(f"  [伺服] 水平={self.servo_pos_h:.2f}°, 垂直={self.servo_pos_v:.2f}°")

    def _handle_pt_control(self, data: Dict[str, Any]):
        """处理可见光控制"""
        pt_focal_en = data.get("pt_focal_en", 0)
        if pt_focal_en == 1:
            self.pt_focal = data.get("pt_focal", 1000)
        elif pt_focal_en in (3, 4):
            self.pt_focal += data.get("pt_focal", 0)

        print(f"  [可见光] 焦距={self.pt_focal:.1f}")

    def _handle_ir_control(self, data: Dict[str, Any]):
        """处理红外控制"""
        ir_focal_en = data.get("ir_focal_en", 0)
        if ir_focal_en == 1:
            self.ir_focal = data.get("ir_focal", 1000)
        elif ir_focal_en in (3, 4):
            self.ir_focal += data.get("ir_focal", 0)

        print(f"  [红外] 焦距={self.ir_focal:.1f}")

    def _receive_loop(self):
        """接收循环"""
        while self._running:
            try:
                data, addr = self._control_sock.recvfrom(65535)
                try:
                    raw_data = json.loads(data.decode('utf-8'))
                    common = raw_data.get("公共内容", {})
                    msg_type = common.get("msg_type", -1)
                    msg_id = common.get("msg_id", -1)

                    if msg_type == 0 and msg_id == 0x7111:
                        # 引导信息 (优先级最高，因为msg_type也是0)
                        # 处理多信息格式 (cont 数组) 和单信息格式
                        specific = self._parse_specific(raw_data)
                        self._handle_guidance(specific)
                    elif msg_type == 0:
                        # 控制指令
                        specific = self._parse_specific(raw_data)
                        self._handle_cmd(specific)
                    elif msg_type == 2:
                        # 查询指令
                        specific = self._parse_specific(raw_data)
                        self._handle_query(specific)

                except json.JSONDecodeError:
                    pass
            except socket.error:
                if self._running:
                    pass
            except Exception as e:
                if self._running:
                    print(f"处理报文错误: {e}")

    def start(self):
        """启动模拟器"""
        if self._running:
            return

        self._running = True

        # 创建socket
        self._create_control_socket()
        self._create_feedback_socket()

        # 启动接收线程
        self._receive_thread = threading.Thread(target=self._receive_loop, daemon=True)
        self._receive_thread.start()

        print(f"光电模拟器启动")
        print(f"  控制指令监听: {self.control_addr}:{self.control_port}")
        print(f"  反馈报文发送: {self.feedback_addr}:{self.feedback_port}")
        print(f"\n等待指令...\n")

    def stop(self):
        """停止模拟器"""
        self._running = False

        if self._control_sock:
            try:
                self._control_sock.close()
            except Exception:
                pass

        if self._feedback_sock:
            try:
                self._feedback_sock.close()
            except Exception:
                pass

        print("\n光电模拟器已停止")

    def add_handler(self, handler_type: str, handler: Callable):
        """添加消息处理器"""
        if handler_type in self._handlers:
            self._handlers[handler_type].append(handler)

    def set_config(self, **kwargs):
        """设置设备配置"""
        self.config.update(kwargs)

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop()
        return False


class CUAVEmulatorWithTargetStream(CUAVEmulator):
    """
    带目标流上报的模拟器

    在基础模拟器上增加目标信息流上报功能
    伺服位置会根据引导信息自动更新
    """

    def __init__(
        self,
        control_addr: str = "230.1.88.51",
        control_port: int = 8003,
        feedback_addr: str = "230.1.88.51",
        feedback_port: int = 8013,
        local_addr: str = ""
    ):
        super().__init__(control_addr, control_port, feedback_addr, feedback_port, local_addr)
        self._stream_thread = None
        self._streaming = False
        self._status_thread = None
        self._status_streaming = False
        self._target_azimuth = 45.0  # 目标方位角
        self._target_elevation = 30.0  # 目标俯仰角
        self._target_range = 5000.0  # 目标距离
        self._target_inited = False  # 目标是否已初始化

    def start(self, stream_interval: float = 0.1):
        """
        启动模拟器

        Args:
            stream_interval: 目标信息上报间隔（秒），默认0.1s即10Hz
        """
        super().start()

        # 启动目标流上报
        self._streaming = True
        self._stream_thread = threading.Thread(
            target=self._stream_loop,
            args=(stream_interval,),
            daemon=True
        )
        self._stream_thread.start()
        print(f"  目标流上报: {1/stream_interval:.0f}Hz")

        # 启动系统状态上报 (2Hz)
        self._status_streaming = True
        self._status_thread = threading.Thread(
            target=self._status_loop,
            daemon=True
        )
        self._status_thread.start()
        print(f"  系统状态上报: 2Hz")

    def stop(self):
        """停止模拟器"""
        self._streaming = False
        self._status_streaming = False

        if self._stream_thread:
            self._stream_thread.join(timeout=1.0)

        if self._status_thread:
            self._status_thread.join(timeout=1.0)

        super().stop()

    def _handle_guidance(self, data: Dict[str, Any]):
        """处理引导信息 - 根据引导更新目标位置和伺服指向"""
        tar_id = data.get("tar_id", 0)
        guid_stat = data.get("guid_stat", 0)

        print(f"  收到引导信息: tar_id={tar_id}, guid_stat={guid_stat}")

        # 更新目标信息
        self.target_id = tar_id

        # 根据引导信息更新目标位置
        # 优先使用 enu_a/enu_e (方位/俯仰)，否则使用 lon/lat/alt 计算
        enu_a = data.get("enu_a")
        enu_e = data.get("enu_e")

        if enu_a is not None and enu_e is not None:
            # 使用引导信息中的方位和俯仰
            self._target_azimuth = float(enu_a)
            self._target_elevation = float(enu_e)
        else:
            # 根据经纬高计算方位俯仰（简化计算）
            lon = data.get("lon", 0)
            lat = data.get("lat", 0)
            alt = data.get("alt", 0)

            if lon != 0 and lat != 0:
                # 简化计算：假设目标是相对站址的位置
                # 实际应该用 ECEF 转换，这里简化处理
                station_lon = self.config.get("lon", 116.397)
                station_lat = self.config.get("lat", 39.916)

                # 方位角 = 目标方位 - 站址方位
                az_offset = (lon - station_lon) * 100  # 简化系数
                self._target_azimuth = (45.0 + az_offset) % 360

                # 俯仰角 = 目标高度对应的角度
                el_offset = (alt / 1000.0) * 5  # 简化系数
                self._target_elevation = 30.0 + el_offset

        # 更新目标距离
        self._target_range = data.get("enu_r", 5000.0)
        if self._target_range == 0:
            self._target_range = 5000.0

        # 伺服指向跟随目标位置（模拟跟踪）
        self.servo_pos_h = self._target_azimuth
        self.servo_pos_v = self._target_elevation

        # 收到有效引导后自动开启跟踪
        # guid_stat: 0=取消, 1=正常, 2=外推
        if guid_stat == 1:
            self.tracking = True
            print(f"  [跟踪] 自动开启跟踪")
        elif guid_stat == 0:
            self.tracking = False
            print(f"  [跟踪] 目标已取消")

        self._target_inited = True
        print(f"  [引导] 目标方位={self._target_azimuth:.2f}°, 俯仰={self._target_elevation:.2f}°")
        print(f"  [伺服] 当前指向水平={self.servo_pos_h:.2f}°, 垂直={self.servo_pos_v:.2f}°")

        # 调用注册的处理器
        for handler in self._handlers["guidance"]:
            handler(data)

        # 发送引导确认回馈
        self._send_guidance_ack()

    def _status_loop(self):
        """系统状态上报循环 (2Hz)"""
        while self._status_streaming:
            try:
                self._send_eo_system_status()
                time.sleep(0.5)
            except Exception:
                if self._status_streaming:
                    pass

    def _send_eo_system_status(self):
        """发送光电系统参数 (0x7201)"""
        # 使用锁保护共享变量的读取
        with self._lock:
            tracking = self.tracking
            servo_h = self.servo_pos_h
            servo_v = self.servo_pos_v

        common = self._build_common_header(
            msg_id=0x7201,
            msg_type=3,  # 数据流
            cont_type=0,
            cont_sum=1
        )
        specific = {
            "sv_stat": 1,  # 正常
            "sv_err": 0,
            "st_mode_h": 1 if tracking else 0,  # 跟踪模式
            "st_mode_v": 1 if tracking else 0,
            "st_loc_h": servo_h,
            "st_loc_v": servo_v,
            "pt_stat": 1,
            "pt_err": 0,
            "pt_focal": self.pt_focal,
            "pt_focus": 100,
            "pt_fov_h": 10.0,
            "pt_fov_v": 7.5,
            "ir_stat": 1,
            "ir_err": 0,
            "ir_focal": self.ir_focal,
            "ir_focus": 5,
            "ir_fov_h": 6.0,
            "ir_fov_v": 4.5,
            "dm_stat": 1,
            "dm_err": 0,
            "dm_dev": 0,
            "trk_dev": 3,
            "pt_trk_link": 1 if tracking else 0,
            "ir_trk_link": 1 if tracking else 0,
            "trk_str": 1 if tracking else 0,
            "trk_mod": 0,
            "det_trk": 1,
            "trk_stat": 1 if tracking else 0,
            "pt_trk_link": 1 if self.tracking else 0,
            "ir_trk_link": 1 if self.tracking else 0,
            "trk_str": 1 if self.tracking else 0,
            "trk_mod": 0,
            "det_trk": 1,
            "trk_stat": 1 if self.tracking else 0,
            "pt_zoom": 0,
            "ir_zoom": 0,
            "pt_focus_mode": 0,
            "ir_focus_mode": 0
        }

        msg = {"公共内容": common, "具体信息": specific}
        data = json.dumps(msg, ensure_ascii=False)
        self._feedback_sock.sendto(
            data.encode('utf-8'),
            (self.feedback_addr, self.feedback_port)
        )

    def _stream_loop(self, interval: float):
        """目标流上报循环"""
        while self._streaming:
            try:
                self._send_target_info()
                time.sleep(interval)
            except Exception:
                if self._streaming:
                    pass

    def _send_target_info(self):
        """发送目标信息"""
        # 使用锁保护共享变量的读写
        with self._lock:
            # 如果已收到引导信息，基于引导位置添加微小运动
            # 如果未收到引导，使用默认位置
            if self._target_inited:
                # 目标缓慢运动（模拟真实目标运动）
                self._target_azimuth += 0.02
                if self._target_azimuth > 360:
                    self._target_azimuth -= 360
                self._target_elevation += 0.01
                if self._target_elevation > 90:
                    self._target_elevation -= 180
                # 伺服跟随目标
                self.servo_pos_h = self._target_azimuth
                self.servo_pos_v = self._target_elevation
            else:
                # 未收到引导时的默认运动
                self._target_azimuth += 0.1
                if self._target_azimuth > 360:
                    self._target_azimuth -= 360
                self._target_elevation += 0.05
                if self._target_elevation > 90:
                    self._target_elevation -= 180

            tracking = self.tracking
            target_azimuth = self._target_azimuth
            target_elevation = self._target_elevation
            target_id = self.target_id
            target_range = self._target_range
            servo_h = self.servo_pos_h
            servo_v = self.servo_pos_v

        now = datetime.now()

        common = {
            "msg_id": 0x7112,
            "msg_sn": self._get_msg_sn(),
            "msg_type": 3,  # 数据流
            "tx_sys_id": self.config["sys_id"],
            "tx_dev_type": self.config["dev_type"],
            "tx_dev_id": self.config["dev_id"],
            "tx_subdev_id": self.config["subdev_id"],
            "rx_sys_id": 999,
            "rx_dev_type": 999,
            "rx_dev_id": 999,
            "rx_subdev_id": 999,
            "yr": now.year,
            "mo": now.month,
            "dy": now.day,
            "h": now.hour,
            "min": now.minute,
            "sec": now.second,
            "msec": (time.time() % 1) * 1000,
            "cont_type": 1,
            "cont_sum": 1
        }

        specific = {
            "yr": now.year,
            "mo": now.month,
            "dy": now.day,
            "h": now.hour,
            "min": now.minute,
            "sec": now.second,
            "msec": (time.time() % 1) * 1000,
            "dev_id": 0,
            "guid_id": target_id,
            "tar_id": target_id if target_id else 1,
            "trk_stat": 1 if tracking else 0,
            "trk_mod": 1,
            "tar_a": target_azimuth,
            "tar_e": target_elevation,
            "tar_av": 5.0,
            "tar_ev": 2.0,
            "tar_rng": max(0, target_range),
            "tar_rv": -50.0,
            "tar_category": 9,
            "tar_iden": "UAV",
            "tar_cfid": 0.95,
            "offset_h": 0,
            "offset_v": 0,
            "tar_rect": [320, 240, 200, 200],
            "fov_h": servo_h,
            "fov_v": servo_v,
            "tar_sum": 1,
            "lon": self.config["lon"],
            "lat": self.config["lat"],
            "alt": self.config["alt"]
        }

        msg = {"公共内容": common, "具体信息": specific}
        data = json.dumps(msg, ensure_ascii=False)
        self._feedback_sock.sendto(
            data.encode('utf-8'),
            (self.feedback_addr, self.feedback_port)
        )


def demo_emulator():
    """演示模拟器"""
    print("=== C-UAV 光电模拟器演示 ===\n")
    print("此程序模拟光电设备行为：")
    print("  1. 监听 8003 端口接收控制/查询指令")
    print("  2. 向 8013 端口发送回馈报文和数据流")
    print("\n按 Ctrl+C 退出\n")

    with CUAVEmulatorWithTargetStream() as emulator:
        # 设置设备参数
        emulator.set_config(
            sys_id=1,
            dev_type=1,
            dev_id=1,
            lon=116.397,
            lat=39.916
        )

        try:
            # 保持运行
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass


def demo_emulator_no_stream():
    """演示模拟器（无目标流上报）"""
    print("=== C-UAV 光电模拟器演示（无目标流）===\n")
    print("此程序模拟光电设备行为：")
    print("  1. 监听 8003 端口接收控制/查询指令")
    print("  2. 向 8013 端口发送回馈报文（不上报目标流）")
    print("\n按 Ctrl+C 退出\n")

    with CUAVEmulator() as emulator:
        emulator.set_config(
            sys_id=1,
            dev_type=1,
            dev_id=1
        )

        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    import sys

    if len(sys.argv) > 1 and sys.argv[1] == "--no-stream":
        demo_emulator_no_stream()
    else:
        demo_emulator()
