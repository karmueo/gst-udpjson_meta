#!/usr/bin/env python3
"""
C-UAV 协议 UDP 组播接收解析模块

提供基于 C-UAV_PROTOCOL.md 协议的 UDP 组播报文接收和解析功能
"""

import json
import socket
import struct
import threading
from abc import ABC, abstractmethod
from datetime import datetime
from typing import Any, Callable, Dict, List, Optional, Set


class CUAVMessageParser:
    """C-UAV 报文解析器"""

    # 报文ID常量
    MSG_ID_CMD = 0x7101          # 指令
    MSG_ID_DEV_CONFIG = 0x7102   # 设备配置参数
    MSG_ID_GUIDANCE = 0x7111     # 引导信息
    MSG_ID_TARGET1 = 0x7112      # 目标信息1
    MSG_ID_TARGET2 = 0x7113      # 目标信息2
    MSG_ID_EO_SYSTEM = 0x7201    # 光电系统参数
    MSG_ID_EO_BIT = 0x7202       # 光电BIT状态
    MSG_ID_EO_TRACK = 0x7203     # 光电跟踪控制
    MSG_ID_EO_SERVO = 0x7204     # 光电伺服控制
    MSG_ID_EO_PT = 0x7205        # 可见光控制
    MSG_ID_EO_IR = 0x7206        # 红外控制
    MSG_ID_EO_DM = 0x7207        # 光电测距控制
    MSG_ID_EO_BOX = 0x7208       # 手框目标区
    MSG_ID_EO_REC = 0x7209       # 光电录像
    MSG_ID_EO_AUX = 0x720A       # 配套控制
    MSG_ID_EO_IMG = 0x720B       # 图像控制

    # 报文类型
    MSG_TYPE_CTRL = 0        # 控制
    MSG_TYPE_FEEDBACK = 1    # 回馈
    MSG_TYPE_QUERY = 2       # 查询
    MSG_TYPE_STREAM = 3      # 数据流
    MSG_TYPE_INIT = 100      # 初始化

    # 目标类型
    TARGET_UNKNOWN = 0
    TARGET_BIRDS = 1
    TARGET_BALLOON = 2
    TARGET_AIRPLANE = 3
    TARGET_CAR = 4
    TARGET_BIG_BIRD = 5
    TARGET_SMALL_BIRD = 6
    TARGET_PERSON = 7
    TARGET_CRUISE_MISSILE = 8
    TARGET_UAV = 9
    TARGET_UNKNOWN2 = 15

    def __init__(self):
        self._handlers: Dict[int, List[Callable]] = {}

    def register_handler(self, msg_id: int, handler: Callable[[Dict[str, Any]], None]):
        """
        注册报文处理器

        Args:
            msg_id: 报文ID
            handler: 处理函数，接收解析后的报文字典
        """
        if msg_id not in self._handlers:
            self._handlers[msg_id] = []
        self._handlers[msg_id].append(handler)

    def unregister_handler(self, msg_id: int, handler: Callable[[Dict[str, Any]], None]):
        """注销报文处理器"""
        if msg_id in self._handlers and handler in self._handlers[msg_id]:
            self._handlers[msg_id].remove(handler)

    def parse(self, raw_data: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """
        解析原始报文数据

        Args:
            raw_data: 原始JSON字典

        Returns:
            解析后的报文字典，解析失败返回None
        """
        try:
            # 提取公共内容和具体信息
            common = raw_data.get("公共内容", {})
            specific_list = []

            if "具体信息" in raw_data:
                specific_list = [raw_data["具体信息"]]
            elif "cont" in raw_data:
                for item in raw_data.get("cont", []):
                    if "具体信息" in item:
                        specific_list.append(item["具体信息"])
                    else:
                        specific_list.append(item)

            if not specific_list:
                return None

            # 构建解析结果
            result = {
                "common": common,
                "msg_id": common.get("msg_id"),
                "msg_sn": common.get("msg_sn"),
                "msg_type": common.get("msg_type"),
                "timestamp": self._parse_timestamp(common),
                "specific": specific_list[0] if len(specific_list) == 1 else specific_list,
                "cont_sum": common.get("cont_sum", 1)
            }

            # 调用注册的处理器
            msg_id = result["msg_id"]
            if msg_id in self._handlers:
                for handler in self._handlers[msg_id]:
                    handler(result)

            return result

        except Exception as e:
            print(f"解析报文失败: {e}")
            return None

    def _parse_timestamp(self, common: Dict[str, Any]) -> datetime:
        """解析时间戳"""
        try:
            return datetime(
                common.get("yr", 2000),
                common.get("mo", 1),
                common.get("dy", 1),
                common.get("h", 0),
                common.get("min", 0),
                common.get("sec", 0),
                int(common.get("msec", 0) * 1000)
            )
        except (ValueError, TypeError):
            return datetime.now()

    def get_msg_type_name(self, msg_type: int) -> str:
        """获取报文类型名称"""
        types = {
            0: "控制",
            1: "回馈",
            2: "查询",
            3: "数据流",
            100: "初始化"
        }
        return types.get(msg_type, f"未知({msg_type})")

    def get_msg_id_name(self, msg_id: int) -> str:
        """获取报文ID名称"""
        names = {
            self.MSG_ID_CMD: "指令",
            self.MSG_ID_DEV_CONFIG: "设备配置参数",
            self.MSG_ID_GUIDANCE: "引导信息",
            self.MSG_ID_TARGET1: "目标信息1",
            self.MSG_ID_TARGET2: "目标信息2",
            self.MSG_ID_EO_SYSTEM: "光电系统参数",
            self.MSG_ID_EO_BIT: "光电BIT状态",
            self.MSG_ID_EO_TRACK: "光电跟踪控制",
            self.MSG_ID_EO_SERVO: "光电伺服控制",
            self.MSG_ID_EO_PT: "可见光控制",
            self.MSG_ID_EO_IR: "红外控制",
            self.MSG_ID_EO_DM: "光电测距控制",
            self.MSG_ID_EO_BOX: "手框目标区",
            self.MSG_ID_EO_REC: "光电录像",
            self.MSG_ID_EO_AUX: "配套控制",
            self.MSG_ID_EO_IMG: "图像控制"
        }
        return names.get(msg_id, f"未知(0x{msg_id:04X})")

    def get_target_type_name(self, target_type: int) -> str:
        """获取目标类型名称"""
        types = {
            0: "不明",
            1: "鸟群",
            2: "空飘物",
            3: "飞机",
            4: "汽车",
            5: "大鸟",
            6: "小鸟",
            7: "行人",
            8: "巡航导弹",
            9: "无人机",
            15: "未知"
        }
        return types.get(target_type, f"未知({target_type})")

    def parse_guidance(self, specific: Dict[str, Any]) -> Dict[str, Any]:
        """
        解析引导信息 (0x7111)

        Args:
            specific: 引导信息具体内容

        Returns:
            解析后的引导信息字典
        """
        return {
            "tar_id": specific.get("tar_id"),           # 引导批号
            "tar_category": specific.get("tar_category"), # 目标类别
            "guid_stat": specific.get("guid_stat"),     # 目标状态: 0=取消 1=正常 2=外推
            "ecef_x": specific.get("ecef_x"),           # 地心坐标 X
            "ecef_y": specific.get("ecef_y"),           # 地心坐标 Y
            "ecef_z": specific.get("ecef_z"),           # 地心坐标 Z
            "ecef_vx": specific.get("ecef_vx"),         # 速度 X
            "ecef_vy": specific.get("ecef_vy"),         # 速度 Y
            "ecef_vz": specific.get("ecef_vz"),         # 速度 Z
            "h_dvi_pct": specific.get("h_dvi_pct"),     # 水平偏差百分比
            "v_dvi_pct": specific.get("v_dvi_pct"),     # 垂直偏差百分比
            "enu_r": specific.get("enu_r"),             # 目标距离
            "enu_a": specific.get("enu_a"),             # 目标方位
            "enu_e": specific.get("enu_e"),             # 目标俯仰
            "enu_v": specific.get("enu_v"),             # 目标速度
            "enu_h": specific.get("enu_h"),             # 目标相对高度
            "lon": specific.get("lon"),                 # 经度
            "lat": specific.get("lat"),                 # 纬度
            "alt": specific.get("alt"),                 # 高度
        }

    def parse_eo_system(self, specific: Dict[str, Any]) -> Dict[str, Any]:
        """
        解析光电系统参数 (0x7201)

        Args:
            specific: 光电系统参数具体内容

        Returns:
            解析后的光电系统参数字典
        """
        return {
            "sv_stat": specific.get("sv_stat"),         # 伺服状态: 0=无效 1=正常 2=自检 3=预热 4=错误
            "sv_err": specific.get("sv_err"),           # 伺服错误代码
            "st_mode_h": specific.get("st_mode_h"),     # 伺服水平模式: 0=手动 1=跟踪
            "st_mode_v": specific.get("st_mode_v"),     # 伺服垂直模式: 0=手动 1=跟踪
            "st_loc_h": specific.get("st_loc_h"),       # 伺服水平指向(度)
            "st_loc_v": specific.get("st_loc_v"),       # 伺服垂直指向(度)
            "pt_stat": specific.get("pt_stat"),         # 可见光状态
            "pt_err": specific.get("pt_err"),           # 可见光错误代码
            "pt_focal": specific.get("pt_focal"),       # 可见光焦距
            "pt_focus": specific.get("pt_focus"),       # 可见光聚焦
            "pt_fov_h": specific.get("pt_fov_h"),       # 可见光水平视场
            "pt_fov_v": specific.get("pt_fov_v"),       # 可见光垂直视场
            "ir_stat": specific.get("ir_stat"),         # 红外状态
            "ir_err": specific.get("ir_err"),           # 红外错误代码
            "ir_focal": specific.get("ir_focal"),       # 红外焦距
            "ir_focus": specific.get("ir_focus"),       # 红外聚焦
            "ir_fov_h": specific.get("ir_fov_h"),       # 红外水平视场
            "ir_fov_v": specific.get("ir_fov_v"),       # 红外垂直视场
            "dm_stat": specific.get("dm_stat"),         # 测距状态
            "dm_err": specific.get("dm_err"),           # 测距错误代码
            "dm_dev": specific.get("dm_dev"),           # 测距设备
            "trk_dev": specific.get("trk_dev"),         # 跟踪设备: 0=可见光 1=红外 3=多传感器联动
            "pt_trk_link": specific.get("pt_trk_link"), # 光电联动: 0=停止 1=开始
            "ir_trk_link": specific.get("ir_trk_link"), # 红外联动: 0=停止 1=开始
            "trk_str": specific.get("trk_str"),         # 跟踪开关: 0=停止 1=开始
            "trk_mod": specific.get("trk_mod"),         # 跟踪模式: 0=自动 1=半自动 2=手动
            "det_trk": specific.get("det_trk"),         # 检测跟踪: 0=检测 1=识别
            "trk_stat": specific.get("trk_stat"),       # 目标状态: 0=非跟踪 1=跟踪正常 3=失锁 4=丢失
            "pt_zoom": specific.get("pt_zoom"),         # 可见光自动变倍: 0=不自动 1=自动
            "ir_zoom": specific.get("ir_zoom"),         # 红外自动变倍: 0=不自动 1=自动
            "pt_focus_mode": specific.get("pt_focus_mode"), # 可见光聚焦模式: 0=自动 1=手动
            "ir_focus_mode": specific.get("ir_focus_mode"), # 红外聚焦模式: 0=自动 1=手动
        }

    def parse_servo_control(self, specific: Dict[str, Any]) -> Dict[str, Any]:
        """
        解析光电伺服控制 (0x7204)

        Args:
            specific: 伺服控制具体内容

        Returns:
            解析后的伺服控制字典
        """
        return {
            "dev_id": specific.get("dev_id"),           # 设备类型: 0=可见光 1=红外 2=两者
            "dev_en": specific.get("dev_en"),           # 使能: 0=关闭 1=上电
            "ctrl_en": specific.get("ctrl_en"),         # 控制使能: 0=无效 1=有效
            "mode_h": specific.get("mode_h"),           # 水平控制模式: 0=手动 1=跟踪
            "mode_v": specific.get("mode_v"),           # 垂直控制模式: 0=手动 1=跟踪
            "speed_en_h": specific.get("speed_en_h"),   # 水平速度使能: 0=无效 1=设置 2=获取 3=增加 4=减小
            "speed_h": specific.get("speed_h"),         # 水平速度 [1,200]
            "speed_en_v": specific.get("speed_en_v"),   # 垂直速度使能
            "speed_v": specific.get("speed_v"),         # 垂直速度 [1,200]
            "loc_en_h": specific.get("loc_en_h"),       # 水平位置使能: 0=无效 1=设置 2=获取 3=增加 4=减小
            "loc_h": specific.get("loc_h"),             # 水平位置(度)
            "loc_en_v": specific.get("loc_en_v"),       # 垂直位置使能
            "loc_v": specific.get("loc_v"),             # 垂直位置(度)
            "offset_en": specific.get("offset_en"),     # 脱靶量使能
            "offset_h": specific.get("offset_h"),       # 水平脱靶量(像素)
            "offset_v": specific.get("offset_v"),       # 垂直脱靶量(像素)
        }


class CUAVMulticastReceiver:
    """C-UAV 协议 UDP 组播接收器"""

    def __init__(
        self,
        multicast_addr: str = "230.1.88.51",
        multicast_port: int = 8013,
        local_addr: str = "",
        buffer_size: int = 65535
    ):
        """
        初始化接收器

        Args:
            multicast_addr: 组播地址
            multicast_port: 组播端口
            local_addr: 本地地址（留空则监听所有地址）
            buffer_size: 接收缓冲区大小
        """
        self.multicast_addr = multicast_addr
        self.multicast_port = multicast_port
        self.buffer_size = buffer_size
        self._running = False
        self._sock = None
        self._parser = CUAVMessageParser()
        self._receive_thread = None
        self._handlers: Dict[str, List[Callable]] = {
            "any": [],        # 处理所有报文
            "ctrl": [],       # 处理控制报文
            "feedback": [],   # 处理回馈报文
            "stream": []      # 处理数据流报文
        }

    @property
    def parser(self) -> CUAVMessageParser:
        """获取报文解析器"""
        return self._parser

    def _setup_socket(self, local_addr: str):
        """设置socket"""
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, self.buffer_size * 2)

        # 绑定组播地址
        if local_addr:
            self._sock.bind((local_addr, self.multicast_port))
        else:
            self._sock.bind(("0.0.0.0", self.multicast_port))

        # 加入组播组
        if local_addr:
            mreq = struct.pack(
                "4s4s",
                socket.inet_aton(self.multicast_addr),
                socket.inet_aton(local_addr)
            )
        else:
            # 监听所有网卡，使用 INADDR_ANY
            mreq = struct.pack(
                "4s4s",
                socket.inet_aton(self.multicast_addr),
                b'\x00\x00\x00\x00'
            )
        self._sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        self._sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)

    def start(self, local_addr: str = ""):
        """
        开始接收报文

        Args:
            local_addr: 本地地址
        """
        if self._running:
            return

        self._setup_socket(local_addr)
        self._running = True
        self._receive_thread = threading.Thread(target=self._receive_loop, daemon=True)
        self._receive_thread.start()
        print(f"开始监听 {self.multicast_addr}:{self.multicast_port}")

    def _receive_loop(self):
        """接收循环"""
        while self._running:
            try:
                data, addr = self._sock.recvfrom(self.buffer_size)
                self._handle_message(data, addr)
            except socket.error as e:
                if self._running:
                    print(f"接收报文错误: {e}")
            except Exception as e:
                print(f"处理报文错误: {e}")

    def _handle_message(self, data: bytes, addr: tuple):
        """处理接收到的报文"""
        try:
            raw_data = json.loads(data.decode('utf-8'))
            result = self._parser.parse(raw_data)

            if result:
                msg_type = result.get("msg_type")
                msg_type_name = self._parser.get_msg_type_name(msg_type)

                # 调用通用处理器
                for handler in self._handlers["any"]:
                    handler(result, addr)

                # 调用类型特定处理器
                if msg_type == self._parser.MSG_TYPE_CTRL:
                    for handler in self._handlers["ctrl"]:
                        handler(result, addr)
                elif msg_type == self._parser.MSG_TYPE_FEEDBACK:
                    for handler in self._handlers["feedback"]:
                        handler(result, addr)
                elif msg_type == self._parser.MSG_TYPE_STREAM:
                    for handler in self._handlers["stream"]:
                        handler(result, addr)

        except json.JSONDecodeError as e:
            print(f"JSON解析错误: {e}")
        except Exception as e:
            print(f"处理报文错误: {e}")

    def add_handler(self, handler_type: str, handler: Callable):
        """
        添加消息处理器

        Args:
            handler_type: 处理器类型 ("any", "ctrl", "feedback", "stream")
            handler: 处理函数，接收 (parsed_msg, addr) 参数
        """
        if handler_type in self._handlers:
            self._handlers[handler_type].append(handler)

    def remove_handler(self, handler_type: str, handler: Callable):
        """移除消息处理器"""
        if handler_type in self._handlers and handler in self._handlers[handler_type]:
            self._handlers[handler_type].remove(handler)

    def stop(self):
        """停止接收"""
        self._running = False
        if self._sock:
            try:
                self._sock.close()
            except Exception:
                pass
        if self._receive_thread:
            self._receive_thread.join(timeout=1.0)
        print("接收器已停止")

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop()
        return False


def print_message_handler(result: Dict[str, Any], addr: tuple):
    """打印报文处理器"""
    parser = CUAVMessageParser()
    msg_type = result.get("msg_type", -1)
    msg_id = result.get("msg_id", -1)

    print(f"\n=== 收到报文 ===")
    print(f"来源: {addr}")
    print(f"报文类型: {parser.get_msg_type_name(msg_type)} ({msg_type})")
    print(f"报文ID: 0x{msg_id:04X} ({parser.get_msg_id_name(msg_id)})")
    print(f"序号: {result.get('msg_sn')}")

    # 打印时间戳
    timestamp = result.get("timestamp")
    if timestamp:
        print(f"时间: {timestamp}")

    # 打印具体信息
    specific = result.get("specific")
    if specific:
        print(f"内容: {json.dumps(specific, ensure_ascii=False, indent=2)}")


def guidance_handler(result: Dict[str, Any], addr: tuple):
    """引导信息处理器"""
    parser = CUAVMessageParser()
    if result.get("msg_id") != parser.MSG_ID_GUIDANCE:
        return

    specific = result.get("specific", {})
    print(f"\n>>> 引导信息 <<<")
    print(f"  批号: {specific.get('tar_id')}")
    print(f"  类别: {parser.get_target_type_name(specific.get('tar_category', 0))}")
    print(f"  状态: {specific.get('guid_stat')} (0=取消 1=正常 2=外推)")
    print(f"  ECEF: ({specific.get('ecef_x'):.2f}, {specific.get('ecef_y'):.2f}, {specific.get('ecef_z'):.2f})")
    print(f"  经纬高: ({specific.get('lon'):.6f}, {specific.get('lat'):.6f}, {specific.get('alt'):.1f})")


def target_handler(result: Dict[str, Any], addr: tuple):
    """目标信息处理器"""
    parser = CUAVMessageParser()
    msg_id = result.get("msg_id")
    if msg_id not in (parser.MSG_ID_TARGET1, parser.MSG_ID_TARGET2):
        return

    specific = result.get("specific", {})
    target_type = specific.get("tar_category", 0)

    print(f"\n>>> 目标信息 <<<")
    print(f"  报文: {parser.get_msg_id_name(msg_id)}")
    print(f"  目标批号: {specific.get('tar_id')}")
    print(f"  目标类型: {parser.get_target_type_name(target_type)}")
    print(f"  状态: {specific.get('trk_stat')} (0=丢失 1=正常 2=外推)")
    print(f"  方位角: {specific.get('tar_a', 0):.4f}°")
    print(f"  俯仰角: {specific.get('tar_e', 0):.4f}°")
    print(f"  距离: {specific.get('tar_rng', 0):.1f}m")
    print(f"  置信度: {specific.get('tar_cfid', 0):.2f}")


def eo_status_handler(result: Dict[str, Any], addr: tuple):
    """光电状态处理器"""
    parser = CUAVMessageParser()
    if result.get("msg_id") != parser.MSG_ID_EO_SYSTEM:
        return

    specific = result.get("specific", {})
    print(f"\n>>> 光电系统状态 <<<")
    print(f"  伺服状态: {specific.get('sv_stat')} (0=无效 1=正常 2=自检 3=预热 4=错误)")
    print(f"  水平指向: {specific.get('st_loc_h', 0):.2f}°")
    print(f"  垂直指向: {specific.get('st_loc_v', 0):.2f}°")
    print(f"  可见光焦距: {specific.get('pt_focal', 0):.1f}")
    print(f"  红外焦距: {specific.get('ir_focal', 0):.1f}")
    print(f"  跟踪状态: {specific.get('trk_stat')} (0=非跟踪 1=跟踪正常 3=失锁 4=丢失)")


def demo_receive():
    """演示接收报文"""
    print("开始接收 C-UAV 组播报文 (Ctrl+C 退出)...")

    with CUAVMulticastReceiver(
        multicast_addr="230.1.88.51",
        multicast_port=8013
    ) as receiver:
        # 注册通用处理器
        receiver.add_handler("any", print_message_handler)

        # 注册特定类型处理器
        receiver.add_handler("ctrl", guidance_handler)
        receiver.add_handler("stream", target_handler)
        receiver.add_handler("stream", eo_status_handler)

        # 保持运行
        try:
            while True:
                import time
                time.sleep(1)
        except KeyboardInterrupt:
            print("\n正在停止...")


if __name__ == "__main__":
    demo_receive()
