#!/usr/bin/env python3
"""
发送 C-UAV 引导信息的测试脚本

向 8003 端口发送引导信息，测试 GStreamer 插件的解析功能
"""

import json
import socket
import struct
import time
from datetime import datetime

# C-UAV 协议配置
# 注意：此配置应与 perf-measurement.yml 中的 cuav-port 保持一致
MULTICAST_ADDR = "230.1.1.22"   # C-UAV 组播地址
CTRL_PORT = 8013                # 控制端口


def get_timestamp():
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


def send_guidance(sock, tar_id=1, tar_category=9, guid_stat=1,
                  enu_a=45.0, enu_e=30.0, enu_r=5000,
                  lon=116.397, lat=39.916, alt=5000):
    """发送引导信息"""
    ts = get_timestamp()

    msg = {
        "公共内容": {
            "msg_id": 0x7111,
            "msg_sn": int(time.time() * 1000) % 10000,
            "msg_type": 0,  # 控制
            "tx_sys_id": 3,
            "tx_dev_type": 3,
            "tx_dev_id": 1,
            "tx_subdev_id": 999,
            "rx_sys_id": 999,
            "rx_dev_type": 1,
            "rx_dev_id": 1,
            "rx_subdev_id": 999,
            **ts,
            "cont_type": 1,
            "cont_sum": 1
        },
        "具体信息": {
            **ts,
            "tar_id": tar_id,
            "tar_category": tar_category,
            "guid_stat": guid_stat,
            "ecef_x": -1245634.5,
            "ecef_y": 5298212.3,
            "ecef_z": 2997512.1,
            "ecef_vx": 100.5,
            "ecef_vy": -50.2,
            "ecef_vz": 0.0,
            "enu_r": enu_r,
            "enu_a": enu_a,
            "enu_e": enu_e,
            "enu_v": 0,
            "enu_h": 0,
            "lon": lon,
            "lat": lat,
            "alt": alt
        }
    }

    data = json.dumps(msg, ensure_ascii=False).encode('utf-8')
    sock.sendto(data, (MULTICAST_ADDR, CTRL_PORT))
    print(f"[发送] 引导信息: tar_id={tar_id}, guid_stat={guid_stat}, 方位={enu_a}°, 俯仰={enu_e}°")
    return msg


def main():
    """主函数"""
    print("=" * 60)
    print("C-UAV 引导信息发送测试")
    print("=" * 60)
    print(f"目标地址: {MULTICAST_ADDR}:{CTRL_PORT}")
    print("-" * 60)

    # 创建 UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)

    try:
        # 发送多个引导信息测试
        print("\n测试1: 发送无人机目标 (tar_id=1, 类别=9)")
        send_guidance(sock, tar_id=1, tar_category=9, guid_stat=1, enu_a=45.0, enu_e=30.0)
        time.sleep(0.5)

        print("\n测试2: 发送飞机目标 (tar_id=2, 类别=3)")
        send_guidance(sock, tar_id=2, tar_category=3, guid_stat=1, enu_a=90.0, enu_e=45.0)
        time.sleep(0.5)

        print("\n测试3: 发送外推目标 (tar_id=3, guid_stat=2)")
        send_guidance(sock, tar_id=3, tar_category=9, guid_stat=2, enu_a=135.0, enu_e=60.0)
        time.sleep(0.5)

        print("\n测试4: 取消目标跟踪 (tar_id=1, guid_stat=0)")
        send_guidance(sock, tar_id=1, tar_category=9, guid_stat=0)
        time.sleep(0.5)

        print("\n测试5: 发送新目标 (tar_id=4, 方位180°, 俯仰30°)")
        send_guidance(sock, tar_id=4, tar_category=9, guid_stat=1, enu_a=180.0, enu_e=30.0, enu_r=3000)

        print("\n" + "=" * 60)
        print("测试完成！请检查 GStreamer 插件是否收到并解析了这些报文")
        print("=" * 60)

    except KeyboardInterrupt:
        print("\n被用户中断")
    finally:
        sock.close()


if __name__ == "__main__":
    main()
