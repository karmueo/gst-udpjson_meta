#ifndef __GST_UDPJSON_META_CUAV_H__
#define __GST_UDPJSON_META_CUAV_H__

#include <glib.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/**
 * @brief C-UAV 报文ID常量定义
 */
typedef enum
{
    CUAV_MSG_ID_CMD = 0x7101,         /* 指令 */
    CUAV_MSG_ID_DEV_CONFIG = 0x7102,  /* 设备配置参数 */
    CUAV_MSG_ID_GUIDANCE = 0x7111,    /* 引导信息 */
    CUAV_MSG_ID_TARGET1 = 0x7112,     /* 目标信息1 */
    CUAV_MSG_ID_TARGET2 = 0x7113,     /* 目标信息2 */
    CUAV_MSG_ID_EO_SYSTEM = 0x7201,   /* 光电系统参数 */
    CUAV_MSG_ID_EO_BIT = 0x7202,      /* 光电BIT状态 */
    CUAV_MSG_ID_EO_TRACK = 0x7203,    /* 光电跟踪控制 */
    CUAV_MSG_ID_EO_SERVO = 0x7204,    /* 光电伺服控制 */
    CUAV_MSG_ID_EO_PT = 0x7205,       /* 可见光控制 */
    CUAV_MSG_ID_EO_IR = 0x7206,       /* 红外控制 */
    CUAV_MSG_ID_EO_DM = 0x7207,       /* 光电测距控制 */
    CUAV_MSG_ID_EO_BOX = 0x7208,      /* 手框目标区 */
    CUAV_MSG_ID_EO_REC = 0x7209,      /* 光电录像 */
    CUAV_MSG_ID_EO_AUX = 0x720A,      /* 配套控制 */
    CUAV_MSG_ID_EO_IMG = 0x720B       /* 图像控制 */
} CUAVMessageId;

/**
 * @brief C-UAV 报文类型定义
 */
typedef enum
{
    CUAV_MSG_TYPE_CTRL = 0,       /* 控制 */
    CUAV_MSG_TYPE_FEEDBACK = 1,   /* 回馈 */
    CUAV_MSG_TYPE_QUERY = 2,      /* 查询 */
    CUAV_MSG_TYPE_STREAM = 3,     /* 数据流 */
    CUAV_MSG_TYPE_INIT = 100      /* 初始化 */
} CUAVMessageType;

/**
 * @brief 目标类型定义
 */
typedef enum
{
    CUAV_TARGET_UNKNOWN = 0,
    CUAV_TARGET_BIRDS = 1,
    CUAV_TARGET_BALLOON = 2,
    CUAV_TARGET_AIRPLANE = 3,
    CUAV_TARGET_CAR = 4,
    CUAV_TARGET_BIG_BIRD = 5,
    CUAV_TARGET_SMALL_BIRD = 6,
    CUAV_TARGET_PERSON = 7,
    CUAV_TARGET_CRUISE_MISSILE = 8,
    CUAV_TARGET_UAV = 9,
    CUAV_TARGET_UNKNOWN2 = 15
} CUAVTargetType;

/**
 * @brief 引导信息结构体 (msg_id = 0x7111)
 */
typedef struct
{
    guint16 yr;           /* 年 */
    guint8 mo;            /* 月 */
    guint8 dy;            /* 日 */
    guint8 h;             /* 时 */
    guint8 min;           /* 分 */
    guint8 sec;           /* 秒 */
    gfloat msec;          /* 毫秒 */
    guint32 tar_id;       /* 引导批号 */
    guint16 tar_category; /* 目标类别 */
    guint8 guid_stat;     /* 目标状态: 0=取消 1=正常 2=外推 */
    gdouble ecef_x;       /* 地心坐标 X */
    gdouble ecef_y;       /* 地心坐标 Y */
    gdouble ecef_z;       /* 地心坐标 Z */
    gdouble ecef_vx;      /* 速度 X */
    gdouble ecef_vy;      /* 速度 Y */
    gdouble ecef_vz;      /* 速度 Z */
    gfloat h_dvi_pct;     /* 水平偏差百分比 */
    gfloat v_dvi_pct;     /* 垂直偏差百分比 */
    gdouble enu_r;        /* 目标距离 */
    gdouble enu_a;        /* 目标方位 */
    gdouble enu_e;        /* 目标俯仰 */
    gdouble enu_v;        /* 目标速度 */
    gdouble enu_h;        /* 目标相对高度 */
    gdouble lon;          /* 经度 */
    gdouble lat;          /* 纬度 */
    gdouble alt;          /* 高度 */
} CUAVGuidanceInfo;

/**
 * @brief 光电系统参数结构体 (msg_id = 0x7201)
 */
typedef struct
{
    guint8 sv_stat;       /* 伺服状态: 0=无效 1=正常 2=自检 3=预热 4=错误 */
    guint16 sv_err;       /* 伺服错误代码 */
    guint8 st_mode_h;     /* 伺服水平模式: 0=手动 1=跟踪 */
    guint8 st_mode_v;     /* 伺服垂直模式: 0=手动 1=跟踪 */
    gfloat st_loc_h;      /* 伺服水平指向(度) [0,360] */
    gfloat st_loc_v;      /* 伺服垂直指向(度) [-90,90] */
    guint8 pt_stat;       /* 可见光状态 */
    guint16 pt_err;       /* 可见光错误代码 */
    gfloat pt_focal;      /* 可见光焦距 [134-16298] */
    guint16 pt_focus;     /* 可见光聚焦 */
    gfloat pt_fov_h;      /* 可见光水平视场 */
    gfloat pt_fov_v;      /* 可见光垂直视场 */
    guint8 ir_stat;       /* 红外状态 */
    guint16 ir_err;       /* 红外错误代码 */
    gfloat ir_focal;      /* 红外焦距 [851-1223] */
    guint16 ir_focus;     /* 红外聚焦 */
    gfloat ir_fov_h;      /* 红外水平视场 */
    gfloat ir_fov_v;      /* 红外垂直视场 */
    guint8 dm_stat;       /* 测距状态 */
    guint16 dm_err;       /* 测距错误代码 */
    guint8 dm_dev;        /* 测距设备 */
    guint8 trk_dev;       /* 跟踪设备: 0=可见光 1=红外 3=多传感器联动 */
    guint8 pt_trk_link;   /* 光电联动: 0=停止 1=开始 */
    guint8 ir_trk_link;   /* 红外联动: 0=停止 1=开始 */
    guint8 trk_str;       /* 跟踪开关: 0=停止 1=开始 */
    guint8 trk_mod;       /* 跟踪模式: 0=自动 1=半自动 2=手动 */
    guint8 det_trk;       /* 检测跟踪: 0=检测 1=识别 */
    guint8 trk_stat;      /* 目标状态: 0=非跟踪 1=跟踪正常 3=失锁 4=丢失 */
    guint8 pt_zoom;       /* 可见光自动变倍: 0=不自动 1=自动 */
    guint8 ir_zoom;       /* 红外自动变倍: 0=不自动 1=自动 */
    guint8 pt_focus_mode; /* 可见光聚焦模式: 0=自动 1=手动 */
    guint8 ir_focus_mode; /* 红外聚焦模式: 0=自动 1=手动 */
} CUAVEOSystemParam;

/**
 * @brief 光电伺服控制结构体 (msg_id = 0x7204)
 */
typedef struct
{
    guint8 dev_id;        /* 设备类型: 0=可见光 1=红外 2=两者 */
    guint8 dev_en;        /* 使能: 0=关闭 1=上电 */
    guint8 ctrl_en;       /* 控制使能: 0=无效 1=有效 */
    guint8 mode_h;        /* 水平控制模式: 0=手动 1=跟踪 */
    guint8 mode_v;        /* 垂直控制模式: 0=手动 1=跟踪 */
    guint8 speed_en_h;    /* 水平速度使能: 0=无效 1=设置 2=获取 3=增加 4=减小 */
    guint8 speed_h;       /* 水平速度 [1,200] */
    guint8 speed_en_v;    /* 垂直速度使能 */
    guint8 speed_v;       /* 垂直速度 [1,200] */
    guint8 loc_en_h;      /* 水平位置使能: 0=无效 1=设置 2=获取 3=增加 4=减小 */
    gfloat loc_h;         /* 水平位置(度) */
    guint8 loc_en_v;      /* 垂直位置使能 */
    gfloat loc_v;         /* 垂直位置(度) */
    guint8 offset_en;     /* 脱靶量使能 */
    gint16 offset_h;      /* 水平脱靶量(像素) */
    gint16 offset_v;      /* 垂直脱靶量(像素) */
} CUAVServoControl;

/**
 * @brief 公共报文头结构体
 */
typedef struct
{
    guint16 msg_id;           /* 报文ID */
    guint32 msg_sn;           /* 报文计数 */
    guint8 msg_type;          /* 报文类型 */
    guint16 tx_sys_id;        /* 发送方系统号 */
    guint16 tx_dev_type;      /* 发送方设备类型 */
    guint16 tx_dev_id;        /* 发送方设备编号 */
    guint16 tx_subdev_id;     /* 发送方分系统编号 */
    guint16 rx_sys_id;        /* 接收方系统号 */
    guint16 rx_dev_type;      /* 接收方设备类型 */
    guint16 rx_dev_id;        /* 接收方设备编号 */
    guint16 rx_subdev_id;     /* 接收方分系统编号 */
    guint16 yr;               /* 年 */
    guint8 mo;                /* 月 */
    guint8 dy;                /* 日 */
    guint8 h;                 /* 时 */
    guint8 min;               /* 分 */
    guint8 sec;               /* 秒 */
    gfloat msec;              /* 毫秒 */
    guint8 cont_type;         /* 信息类型: 0=单信息 1=多目标 2=分时多目标 */
    guint16 cont_sum;         /* 信息数量 */
    guint64 recv_ts_us;       /* 接收时间戳(微秒) */
} CUAVCommonHeader;

/**
 * @brief 回调函数类型定义
 */
typedef void (*CUAVGuidanceCallback)(const CUAVCommonHeader *header,
                                     const CUAVGuidanceInfo *guidance,
                                     gpointer user_data);

typedef void (*CUAVEOSystemCallback)(const CUAVCommonHeader *header,
                                     const CUAVEOSystemParam *eo_param,
                                     gpointer user_data);

typedef void (*CUAVServoControlCallback)(const CUAVCommonHeader *header,
                                         const CUAVServoControl *servo,
                                         gpointer user_data);

typedef void (*CUAVRawMessageCallback)(const CUAVCommonHeader *header,
                                       JsonObject *specific,
                                       gpointer user_data);

/**
 * @brief C-UAV 报文解析器（不透明类型）
 */
typedef struct _CUAVParser CUAVParser;

/**
 * @brief 创建新的解析器实例
 *
 * @return 解析器实例
 */
CUAVParser *cuav_parser_new(void);

/**
 * @brief 释放解析器实例
 *
 * @param parser 解析器实例
 */
void cuav_parser_free(CUAVParser *parser);

/**
 * @brief 启用/禁用调试打印
 *
 * @param parser 解析器实例
 * @param enable TRUE 启用调试打印
 */
void cuav_parser_set_debug(CUAVParser *parser, gboolean enable);

/**
 * @brief 解析 C-UAV 报文
 *
 * @param parser 解析器实例
 * @param data JSON 数据
 * @param len 数据长度
 * @return 解析成功返回 TRUE
 */
gboolean cuav_parser_parse(CUAVParser *parser, const gchar *data, gssize len);

/**
 * @brief 注册引导信息回调
 *
 * @param parser 解析器实例
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void cuav_parser_set_guidance_callback(CUAVParser *parser,
                                       CUAVGuidanceCallback callback,
                                       gpointer user_data);

/**
 * @brief 注册光电系统参数回调
 *
 * @param parser 解析器实例
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void cuav_parser_set_eo_system_callback(CUAVParser *parser,
                                        CUAVEOSystemCallback callback,
                                        gpointer user_data);

/**
 * @brief 注册光电伺服控制回调
 *
 * @param parser 解析器实例
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void cuav_parser_set_servo_control_callback(CUAVParser *parser,
                                            CUAVServoControlCallback callback,
                                            gpointer user_data);

/**
 * @brief 注册原始报文回调
 *
 * @param parser 解析器实例
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void cuav_parser_set_raw_callback(CUAVParser *parser,
                                  CUAVRawMessageCallback callback,
                                  gpointer user_data);

/**
 * @brief 获取报文类型名称
 *
 * @param msg_type 报文类型
 * @return 类型名称字符串
 */
const gchar *cuav_get_msg_type_name(guint8 msg_type);

/**
 * @brief 获取报文ID名称
 *
 * @param msg_id 报文ID
 * @return ID名称字符串
 */
const gchar *cuav_get_msg_id_name(guint16 msg_id);

/**
 * @brief 获取目标类型名称
 *
 * @param target_type 目标类型
 * @return 类型名称字符串
 */
const gchar *cuav_get_target_type_name(guint16 target_type);

/**
 * @brief 打印引导信息（调试用）
 *
 * @param guidance 引导信息
 */
void cuav_print_guidance(const CUAVGuidanceInfo *guidance);

/**
 * @brief 打印光电系统参数（调试用）
 *
 * @param eo_param 光电系统参数
 */
void cuav_print_eo_system(const CUAVEOSystemParam *eo_param);

/**
 * @brief 打印伺服控制（调试用）
 *
 * @param servo 伺服控制
 */
void cuav_print_servo_control(const CUAVServoControl *servo);

G_END_DECLS

#endif /* __GST_UDPJSON_META_CUAV_H__ */
