#ifndef __GST_UDPJSON_META_H__
#define __GST_UDPJSON_META_H__

#include <gst/base/gstbasetransform.h>
#include <glib.h>
#include "nvdsmeta.h"
#include "gstudpjsonmeta_cuav.h"

G_BEGIN_DECLS

#define GST_TYPE_UDPJSON_META (gst_udpjson_meta_get_type())
#define GST_UDPJSON_META(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_UDPJSON_META, GstUdpJsonMeta))
#define GST_UDPJSON_META_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_UDPJSON_META, GstUdpJsonMetaClass))
#define GST_IS_UDPJSON_META(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_UDPJSON_META))
#define GST_IS_UDPJSON_META_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_UDPJSON_META))

typedef struct _GstUdpJsonMeta GstUdpJsonMeta;
typedef struct _GstUdpJsonMetaClass GstUdpJsonMetaClass;

struct _GstUdpJsonMeta
{
    GstBaseTransform parent;

    gchar *multicast_ip; /* 组播地址 */
    guint port; /* 组播端口 */
    gchar *iface; /* 绑定网卡名 */
    guint recv_buf_size; /* 接收缓冲区大小 */
    guint cache_ttl_ms; /* 缓存有效期(毫秒) */
    guint max_cache_size; /* 最大缓存条目数 */

    /* C-UAV 协议解析配置 */
    gboolean enable_cuav_parser; /* 是否启用 C-UAV 协议解析 */
    guint cuav_multicast_port; /* C-UAV 组播端口 */
    guint cuav_ctrl_port; /* C-UAV 控制/引导端口 */
    gboolean cuav_debug; /* C-UAV 调试打印 */
    CUAVParser *cuav_parser; /* C-UAV 报文解析器 */

    gint sockfd; /* UDP 套接字 */
    gint cuav_sockfd; /* C-UAV UDP 套接字 */
    gint cuav_ctrl_sockfd; /* C-UAV 控制/引导 UDP 套接字 */
    GThread *recv_thread; /* 接收线程 */
    gint stop_flag; /* 停止标记 */

    GRWLock cache_lock; /* 缓存读写锁 */
    GHashTable *cache; /* 数据缓存 */

    NvDsMetaType meta_type; /* 用户元数据类型 */
};

struct _GstUdpJsonMetaClass
{
    GstBaseTransformClass parent_class;
};

GType gst_udpjson_meta_get_type(void);

/**
 * @brief 设置引导信息回调
 *
 * @param element GstUdpJsonMeta 元素
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void gst_udpjson_meta_set_guidance_callback(GstUdpJsonMeta *element,
                                            CUAVGuidanceCallback callback,
                                            gpointer user_data);

/**
 * @brief 设置光电系统参数回调
 *
 * @param element GstUdpJsonMeta 元素
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void gst_udpjson_meta_set_eo_system_callback(GstUdpJsonMeta *element,
                                             CUAVEOSystemCallback callback,
                                             gpointer user_data);

/**
 * @brief 设置光电伺服控制回调
 *
 * @param element GstUdpJsonMeta 元素
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void gst_udpjson_meta_set_servo_control_callback(GstUdpJsonMeta *element,
                                                 CUAVServoControlCallback callback,
                                                 gpointer user_data);

/**
 * @brief 启用/禁用 C-UAV 协议解析
 *
 * @param element GstUdpJsonMeta 元素
 * @param enable 是否启用
 * @param port C-UAV 组播端口
 */
void gst_udpjson_meta_enable_cuav_parser(GstUdpJsonMeta *element,
                                         gboolean enable,
                                         guint port);

/**
 * @brief 启用/禁用 C-UAV 调试打印
 *
 * @param element GstUdpJsonMeta 元素
 * @param enable TRUE 启用调试打印
 */
void gst_udpjson_meta_set_cuav_debug(GstUdpJsonMeta *element, gboolean enable);

G_END_DECLS

#endif /* __GST_UDPJSON_META_H__ */
