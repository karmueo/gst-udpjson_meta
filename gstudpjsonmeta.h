#ifndef __GST_UDPJSON_META_H__
#define __GST_UDPJSON_META_H__

#include <gst/base/gstbasetransform.h>
#include <glib.h>
#include "nvdsmeta.h"

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
    gchar *json_key; /* JSON 值键 */
    gchar *object_id_key; /* JSON 目标ID键 */
    gchar *source_id_key; /* JSON 源ID键 */
    guint cache_ttl_ms; /* 缓存有效期(毫秒) */
    guint max_cache_size; /* 最大缓存条目数 */

    gint sockfd; /* UDP 套接字 */
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

G_END_DECLS

#endif /* __GST_UDPJSON_META_H__ */
