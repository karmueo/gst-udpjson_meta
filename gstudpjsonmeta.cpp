#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstudpjsonmeta.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <json-glib/json-glib.h>
#include <net/if.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "gstnvdsmeta.h"
#include "nvdsmeta.h"

GST_DEBUG_CATEGORY_STATIC(gst_udpjson_meta_debug);
#define GST_CAT_DEFAULT gst_udpjson_meta_debug

#define DEFAULT_MULTICAST_IP "239.255.0.1"
#define DEFAULT_PORT 6000
#define DEFAULT_CACHE_TTL_MS 1000
#define DEFAULT_MAX_CACHE_SIZE 2048
#define DEFAULT_JSON_KEY "value"
#define DEFAULT_OBJECT_ID_KEY "object_id"
#define DEFAULT_SOURCE_ID_KEY "source_id"

/* 用户元数据结构体 */
typedef struct
{
    gchar *key; /* JSON 键名 */
    gchar *value; /* JSON 值字符串 */
    guint64 recv_ts_us; /* 接收时间(微秒) */
} UdpJsonObjMeta;

/* 缓存键 */
typedef struct
{
    guint source_id; /* 源ID */
    guint64 object_id; /* 目标ID */
} UdpJsonCacheKey;

/* 缓存值 */
typedef struct
{
    gchar *value; /* JSON 值字符串 */
    guint64 recv_ts_us; /* 接收时间(微秒) */
} UdpJsonCacheValue;

enum
{
    PROP_0,
    PROP_MULTICAST_IP,
    PROP_PORT,
    PROP_IFACE,
    PROP_RECV_BUF_SIZE,
    PROP_JSON_KEY,
    PROP_OBJECT_ID_KEY,
    PROP_SOURCE_ID_KEY,
    PROP_CACHE_TTL_MS,
    PROP_MAX_CACHE_SIZE
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS("ANY"));
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS("ANY"));

#define gst_udpjson_meta_parent_class parent_class
G_DEFINE_TYPE(GstUdpJsonMeta, gst_udpjson_meta, GST_TYPE_BASE_TRANSFORM);

/**
 * @brief 计算缓存键的哈希值。
 *
 * @param key 缓存键指针。
 * @return 哈希值。
 */
static guint udpjson_cache_key_hash(gconstpointer key)
{
    const UdpJsonCacheKey *ckey = (const UdpJsonCacheKey *)key; /* 缓存键指针 */
    guint hash = 0; /* 哈希值 */
    hash = g_direct_hash(GINT_TO_POINTER(ckey->source_id));
    hash ^= g_int64_hash(&ckey->object_id);
    return hash;
}

/**
 * @brief 判断两个缓存键是否相等。
 *
 * @param a 缓存键A。
 * @param b 缓存键B。
 * @return 相等返回 TRUE。
 */
static gboolean udpjson_cache_key_equal(gconstpointer a, gconstpointer b)
{
    const UdpJsonCacheKey *ka = (const UdpJsonCacheKey *)a; /* 缓存键A */
    const UdpJsonCacheKey *kb = (const UdpJsonCacheKey *)b; /* 缓存键B */
    return (ka->source_id == kb->source_id) && (ka->object_id == kb->object_id);
}

/**
 * @brief 释放缓存键。
 *
 * @param data 缓存键指针。
 */
static void udpjson_cache_key_free(gpointer data)
{
    UdpJsonCacheKey *key = (UdpJsonCacheKey *)data; /* 缓存键 */
    g_free(key);
}

/**
 * @brief 释放缓存值。
 *
 * @param data 缓存值指针。
 */
static void udpjson_cache_value_free(gpointer data)
{
    UdpJsonCacheValue *val = (UdpJsonCacheValue *)data; /* 缓存值 */
    if (!val)
        return;
    g_free(val->value);
    g_free(val);
}

/**
 * @brief 复制用户元数据。
 *
 * @param data 用户元数据指针。
 * @param user_data 用户自定义数据。
 * @return 新的用户元数据指针。
 */
static gpointer udpjson_obj_meta_copy(gpointer data, gpointer user_data)
{
    const UdpJsonObjMeta *src = (const UdpJsonObjMeta *)data; /* 源数据 */
    UdpJsonObjMeta *dst = (UdpJsonObjMeta *)g_malloc0(sizeof(UdpJsonObjMeta)); /* 新元数据 */
    if (!src || !dst)
        return dst;
    dst->key = g_strdup(src->key);
    dst->value = g_strdup(src->value);
    dst->recv_ts_us = src->recv_ts_us;
    return dst;
}

/**
 * @brief 释放用户元数据。
 *
 * @param data 用户元数据指针。
 * @param user_data 用户自定义数据。
 */
static void udpjson_obj_meta_release(gpointer data, gpointer user_data)
{
    UdpJsonObjMeta *meta = (UdpJsonObjMeta *)data; /* 用户元数据 */
    if (!meta)
        return;
    g_free(meta->key);
    g_free(meta->value);
    g_free(meta);
}

/**
 * @brief 从 JSON 节点解析无符号整数。
 *
 * @param node JSON 节点。
 * @param out 输出的整数。
 * @return 解析成功返回 TRUE。
 */
static gboolean udpjson_parse_uint64(JsonNode *node, guint64 *out)
{
    GType vtype = 0; /* 值类型 */
    if (!node || !out)
        return FALSE;

    vtype = json_node_get_value_type(node);
    if (vtype == G_TYPE_STRING)
    {
        const gchar *str = json_node_get_string(node); /* 字符串 */
        if (!str)
            return FALSE;
        *out = g_ascii_strtoull(str, NULL, 10);
        return TRUE;
    }
    if (vtype == G_TYPE_INT64)
    {
        *out = (guint64)json_node_get_int(node);
        return TRUE;
    }
    if (vtype == G_TYPE_DOUBLE)
    {
        *out = (guint64)json_node_get_double(node);
        return TRUE;
    }
    if (vtype == G_TYPE_UINT64)
    {
        GValue val = G_VALUE_INIT; /* 临时值 */
        json_node_get_value(node, &val);
        *out = g_value_get_uint64(&val);
        g_value_unset(&val);
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief 将 JSON 节点转换为字符串。
 *
 * @param node JSON 节点。
 * @return 新分配的字符串，需调用 g_free 释放。
 */
static gchar *udpjson_node_to_string(JsonNode *node)
{
    GType vtype = 0; /* 值类型 */
    if (!node)
        return NULL;

    if (!JSON_NODE_HOLDS_VALUE(node))
    {
        return json_to_string(node, FALSE);
    }

    vtype = json_node_get_value_type(node);
    if (vtype == G_TYPE_STRING)
        return g_strdup(json_node_get_string(node));
    if (vtype == G_TYPE_INT64)
        return g_strdup_printf("%" G_GINT64_FORMAT, json_node_get_int(node));
    if (vtype == G_TYPE_DOUBLE)
        return g_strdup_printf("%f", json_node_get_double(node));
    if (vtype == G_TYPE_BOOLEAN)
        return g_strdup(json_node_get_boolean(node) ? "true" : "false");
    if (vtype == G_TYPE_UINT64)
    {
        GValue val = G_VALUE_INIT; /* 临时值 */
        guint64 out = 0; /* 解析值 */
        json_node_get_value(node, &val);
        out = g_value_get_uint64(&val);
        g_value_unset(&val);
        return g_strdup_printf("%" G_GUINT64_FORMAT, out);
    }

    return json_to_string(node, FALSE);
}

/**
 * @brief 更新缓存中的目标值。
 *
 * @param self 插件实例。
 * @param source_id 源ID。
 * @param object_id 目标ID。
 * @param value JSON 值字符串。
 */
static void udpjson_cache_update(GstUdpJsonMeta *self, guint source_id,
                                 guint64 object_id, const gchar *value)
{
    UdpJsonCacheKey *key = NULL; /* 缓存键 */
    UdpJsonCacheValue *val = NULL; /* 缓存值 */
    guint64 now_us = 0; /* 当前时间(微秒) */

    if (!self || !value)
        return;

    now_us = (guint64)g_get_monotonic_time();

    g_rw_lock_writer_lock(&self->cache_lock);

    if (self->max_cache_size > 0 && g_hash_table_size(self->cache) >= self->max_cache_size)
    {
        g_hash_table_remove_all(self->cache);
    }

    key = (UdpJsonCacheKey *)g_malloc0(sizeof(UdpJsonCacheKey));
    key->source_id = source_id;
    key->object_id = object_id;

    val = (UdpJsonCacheValue *)g_malloc0(sizeof(UdpJsonCacheValue));
    val->value = g_strdup(value);
    val->recv_ts_us = now_us;

    g_hash_table_replace(self->cache, key, val);
    g_rw_lock_writer_unlock(&self->cache_lock);
}

/**
 * @brief 解析 JSON 并更新缓存。
 *
 * @param self 插件实例。
 * @param data JSON 数据。
 * @param len 数据长度。
 */
static void udpjson_parse_and_cache(GstUdpJsonMeta *self, const gchar *data, gssize len)
{
    JsonParser *parser = NULL; /* JSON 解析器 */
    JsonNode *root = NULL; /* 根节点 */
    JsonObject *obj = NULL; /* JSON 对象 */
    JsonNode *obj_id_node = NULL; /* 目标ID节点 */
    JsonNode *src_id_node = NULL; /* 源ID节点 */
    JsonNode *val_node = NULL; /* 值节点 */
    guint64 object_id = 0; /* 目标ID */
    guint64 source_id64 = 0; /* 源ID */
    gchar *value_str = NULL; /* 值字符串 */

    if (!self || !data || len <= 0)
        return;

    parser = json_parser_new();
    if (!json_parser_load_from_data(parser, data, len, NULL))
    {
        g_object_unref(parser);
        return;
    }

    root = json_parser_get_root(parser);
    if (!root || !JSON_NODE_HOLDS_OBJECT(root))
    {
        g_object_unref(parser);
        return;
    }

    obj = json_node_get_object(root);
    if (!obj)
    {
        g_object_unref(parser);
        return;
    }

    if (self->object_id_key && json_object_has_member(obj, self->object_id_key))
        obj_id_node = json_object_get_member(obj, self->object_id_key);
    if (self->source_id_key && json_object_has_member(obj, self->source_id_key))
        src_id_node = json_object_get_member(obj, self->source_id_key);
    if (self->json_key && json_object_has_member(obj, self->json_key))
        val_node = json_object_get_member(obj, self->json_key);

    if (!obj_id_node || !val_node)
    {
        g_object_unref(parser);
        return;
    }

    if (!udpjson_parse_uint64(obj_id_node, &object_id))
    {
        g_object_unref(parser);
        return;
    }

    if (src_id_node && udpjson_parse_uint64(src_id_node, &source_id64))
    {
        /* 使用解析到的 source_id */
    }
    else
    {
        source_id64 = 0;
    }

    value_str = udpjson_node_to_string(val_node);
    if (value_str)
    {
        udpjson_cache_update(self, (guint)source_id64, object_id, value_str);
        g_free(value_str);
    }

    g_object_unref(parser);
}

/**
 * @brief UDP 接收线程入口。
 *
 * @param data 插件实例。
 * @return 线程返回值。
 */
static gpointer udpjson_recv_thread(gpointer data)
{
    GstUdpJsonMeta *self = (GstUdpJsonMeta *)data; /* 插件实例 */
    struct pollfd pfd; /* poll 结构 */
    gchar buf[8192]; /* 接收缓冲区 */

    if (!self)
        return NULL;

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = self->sockfd;
    pfd.events = POLLIN;

    while (!g_atomic_int_get(&self->stop_flag))
    {
        int ret = poll(&pfd, 1, 100); /* 100ms 轮询 */
        if (ret <= 0)
            continue;
        if (pfd.revents & POLLIN)
        {
            ssize_t len = recvfrom(self->sockfd, buf, sizeof(buf) - 1, 0, NULL, NULL); /* 读取长度 */
            if (len <= 0)
                continue;
            buf[len] = '\0';
            udpjson_parse_and_cache(self, buf, len);
        }
    }

    return NULL;
}

/**
 * @brief 初始化 UDP 套接字并加入组播。
 *
 * @param self 插件实例。
 * @return 成功返回 TRUE。
 */
static gboolean udpjson_setup_socket(GstUdpJsonMeta *self)
{
    struct sockaddr_in addr; /* 绑定地址 */
    struct ip_mreq mreq; /* 组播请求 */
    int reuse = 1; /* 复用标记 */
    int flags = 0; /* socket 标志 */

    if (!self)
        return FALSE;

    self->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (self->sockfd < 0)
    {
        GST_ERROR("Failed to create UDP socket: %s", strerror(errno));
        return FALSE;
    }

    if (setsockopt(self->sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        GST_WARNING("Failed to set SO_REUSEADDR: %s", strerror(errno));
    }

    if (self->recv_buf_size > 0)
    {
        int rcvbuf = (int)self->recv_buf_size; /* 接收缓冲区 */
        if (setsockopt(self->sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0)
        {
            GST_WARNING("Failed to set SO_RCVBUF: %s", strerror(errno));
        }
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((guint16)self->port);

    if (bind(self->sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        GST_ERROR("Failed to bind UDP socket: %s", strerror(errno));
        close(self->sockfd);
        self->sockfd = -1;
        return FALSE;
    }

    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr.s_addr = inet_addr(self->multicast_ip);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (self->iface && strlen(self->iface) > 0)
    {
        struct ifreq ifr; /* 网卡信息 */
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, self->iface, IFNAMSIZ - 1);
        if (ioctl(self->sockfd, SIOCGIFADDR, &ifr) == 0)
        {
            struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr; /* 网卡地址 */
            mreq.imr_interface = sin->sin_addr;
        }
        if (setsockopt(self->sockfd, SOL_SOCKET, SO_BINDTODEVICE, self->iface,
                       strlen(self->iface)) < 0)
        {
            GST_WARNING("Failed to bind device %s: %s", self->iface, strerror(errno));
        }
    }

    if (setsockopt(self->sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        GST_ERROR("Failed to join multicast group %s:%u: %s", self->multicast_ip,
                  self->port, strerror(errno));
        close(self->sockfd);
        self->sockfd = -1;
        return FALSE;
    }

    flags = fcntl(self->sockfd, F_GETFL, 0);
    if (flags >= 0)
    {
        if (fcntl(self->sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            GST_WARNING("Failed to set UDP socket non-blocking: %s", strerror(errno));
        }
    }

    return TRUE;
}

/**
 * @brief 释放 UDP 套接字。
 *
 * @param self 插件实例。
 */
static void udpjson_teardown_socket(GstUdpJsonMeta *self)
{
    if (!self)
        return;
    if (self->sockfd >= 0)
    {
        close(self->sockfd);
        self->sockfd = -1;
    }
}

/**
 * @brief GstBaseTransform: 启动插件。
 *
 * @param trans 基类指针。
 * @return 成功返回 TRUE。
 */
static gboolean gst_udpjson_meta_start(GstBaseTransform *trans)
{
    GstUdpJsonMeta *self = GST_UDPJSON_META(trans); /* 插件实例 */

    g_atomic_int_set(&self->stop_flag, 0);
    if (!udpjson_setup_socket(self))
        return FALSE;

    self->recv_thread = g_thread_new("udpjson-recv", udpjson_recv_thread, self);
    return TRUE;
}

/**
 * @brief GstBaseTransform: 停止插件。
 *
 * @param trans 基类指针。
 * @return 成功返回 TRUE。
 */
static gboolean gst_udpjson_meta_stop(GstBaseTransform *trans)
{
    GstUdpJsonMeta *self = GST_UDPJSON_META(trans); /* 插件实例 */

    g_atomic_int_set(&self->stop_flag, 1);
    if (self->recv_thread)
    {
        g_thread_join(self->recv_thread);
        self->recv_thread = NULL;
    }

    udpjson_teardown_socket(self);
    return TRUE;
}

/**
 * @brief 为目标附加用户元数据。
 *
 * @param self 插件实例。
 * @param batch_meta 批次元数据。
 * @param obj_meta 目标元数据。
 * @param value JSON 值字符串。
 * @param recv_ts_us 接收时间。
 */
static void udpjson_attach_obj_meta(GstUdpJsonMeta *self, NvDsBatchMeta *batch_meta,
                                    NvDsObjectMeta *obj_meta, const gchar *value,
                                    guint64 recv_ts_us)
{
    NvDsUserMeta *user_meta = NULL; /* 用户元数据 */
    UdpJsonObjMeta *meta = NULL; /* 用户数据 */

    if (!self || !batch_meta || !obj_meta || !value)
        return;

    user_meta = nvds_acquire_user_meta_from_pool(batch_meta);
    if (!user_meta)
        return;

    meta = (UdpJsonObjMeta *)g_malloc0(sizeof(UdpJsonObjMeta));
    meta->key = g_strdup(self->json_key ? self->json_key : DEFAULT_JSON_KEY);
    meta->value = g_strdup(value);
    meta->recv_ts_us = recv_ts_us;

    user_meta->user_meta_data = meta;
    user_meta->base_meta.meta_type = self->meta_type;
    user_meta->base_meta.copy_func = udpjson_obj_meta_copy;
    user_meta->base_meta.release_func = udpjson_obj_meta_release;
    user_meta->base_meta.batch_meta = batch_meta;

    nvds_add_user_meta_to_obj(obj_meta, user_meta);
}

/**
 * @brief GstBaseTransform: 就地处理缓冲区并追加目标元数据。
 *
 * @param trans 基类指针。
 * @param buf GStreamer 缓冲区。
 * @return GST_FLOW_OK。
 */
static GstFlowReturn gst_udpjson_meta_transform_ip(GstBaseTransform *trans, GstBuffer *buf)
{
    GstUdpJsonMeta *self = GST_UDPJSON_META(trans); /* 插件实例 */
    NvDsBatchMeta *batch_meta = NULL; /* 批次元数据 */
    guint64 now_us = 0; /* 当前时间 */

    if (!self || !buf)
        return GST_FLOW_OK;

    batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    if (!batch_meta)
        return GST_FLOW_OK;

    now_us = (guint64)g_get_monotonic_time();

    for (NvDsMetaList *l_frame = batch_meta->frame_meta_list; l_frame; l_frame = l_frame->next)
    {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)l_frame->data; /* 帧元数据 */
        guint source_id = frame_meta->source_id; /* 源ID */

        if (!frame_meta)
            continue;

        g_rw_lock_reader_lock(&self->cache_lock);

        for (NvDsMetaList *l_obj = frame_meta->obj_meta_list; l_obj; l_obj = l_obj->next)
        {
            NvDsObjectMeta *obj_meta = (NvDsObjectMeta *)l_obj->data; /* 目标元数据 */
            UdpJsonCacheKey lookup_key; /* 查找键 */
            UdpJsonCacheValue *cached = NULL; /* 缓存值 */
            guint64 age_ms = 0; /* 过期时间 */

            if (!obj_meta)
                continue;
            if (obj_meta->object_id == UNTRACKED_OBJECT_ID)
                continue;

            lookup_key.source_id = source_id;
            lookup_key.object_id = obj_meta->object_id;

            cached = (UdpJsonCacheValue *)g_hash_table_lookup(self->cache, &lookup_key);
            if (!cached)
                continue;

            if (self->cache_ttl_ms > 0)
            {
                age_ms = (now_us - cached->recv_ts_us) / 1000;
                if (age_ms > self->cache_ttl_ms)
                    continue;
            }

            udpjson_attach_obj_meta(self, batch_meta, obj_meta, cached->value, cached->recv_ts_us);
        }

        g_rw_lock_reader_unlock(&self->cache_lock);
    }

    return GST_FLOW_OK;
}

/**
 * @brief 设置插件属性。
 *
 * @param object GObject 指针。
 * @param property_id 属性ID。
 * @param value 属性值。
 * @param pspec 属性说明。
 */
static void gst_udpjson_meta_set_property(GObject *object, guint property_id,
                                          const GValue *value, GParamSpec *pspec)
{
    GstUdpJsonMeta *self = GST_UDPJSON_META(object); /* 插件实例 */

    switch (property_id)
    {
    case PROP_MULTICAST_IP:
        g_free(self->multicast_ip);
        self->multicast_ip = g_value_dup_string(value);
        break;
    case PROP_PORT:
        self->port = g_value_get_uint(value);
        break;
    case PROP_IFACE:
        g_free(self->iface);
        self->iface = g_value_dup_string(value);
        break;
    case PROP_RECV_BUF_SIZE:
        self->recv_buf_size = g_value_get_uint(value);
        break;
    case PROP_JSON_KEY:
        g_free(self->json_key);
        self->json_key = g_value_dup_string(value);
        break;
    case PROP_OBJECT_ID_KEY:
        g_free(self->object_id_key);
        self->object_id_key = g_value_dup_string(value);
        break;
    case PROP_SOURCE_ID_KEY:
        g_free(self->source_id_key);
        self->source_id_key = g_value_dup_string(value);
        break;
    case PROP_CACHE_TTL_MS:
        self->cache_ttl_ms = g_value_get_uint(value);
        break;
    case PROP_MAX_CACHE_SIZE:
        self->max_cache_size = g_value_get_uint(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

/**
 * @brief 获取插件属性。
 *
 * @param object GObject 指针。
 * @param property_id 属性ID。
 * @param value 输出属性值。
 * @param pspec 属性说明。
 */
static void gst_udpjson_meta_get_property(GObject *object, guint property_id,
                                          GValue *value, GParamSpec *pspec)
{
    GstUdpJsonMeta *self = GST_UDPJSON_META(object); /* 插件实例 */

    switch (property_id)
    {
    case PROP_MULTICAST_IP:
        g_value_set_string(value, self->multicast_ip);
        break;
    case PROP_PORT:
        g_value_set_uint(value, self->port);
        break;
    case PROP_IFACE:
        g_value_set_string(value, self->iface);
        break;
    case PROP_RECV_BUF_SIZE:
        g_value_set_uint(value, self->recv_buf_size);
        break;
    case PROP_JSON_KEY:
        g_value_set_string(value, self->json_key);
        break;
    case PROP_OBJECT_ID_KEY:
        g_value_set_string(value, self->object_id_key);
        break;
    case PROP_SOURCE_ID_KEY:
        g_value_set_string(value, self->source_id_key);
        break;
    case PROP_CACHE_TTL_MS:
        g_value_set_uint(value, self->cache_ttl_ms);
        break;
    case PROP_MAX_CACHE_SIZE:
        g_value_set_uint(value, self->max_cache_size);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

/**
 * @brief 释放插件实例。
 *
 * @param object GObject 指针。
 */
static void gst_udpjson_meta_finalize(GObject *object)
{
    GstUdpJsonMeta *self = GST_UDPJSON_META(object); /* 插件实例 */

    g_free(self->multicast_ip);
    g_free(self->iface);
    g_free(self->json_key);
    g_free(self->object_id_key);
    g_free(self->source_id_key);

    if (self->cache)
        g_hash_table_destroy(self->cache);

    g_rw_lock_clear(&self->cache_lock);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * @brief 初始化类。
 *
 * @param klass 类指针。
 */
static void gst_udpjson_meta_class_init(GstUdpJsonMetaClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass); /* GObject 类 */
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass); /* 元素类 */
    GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS(klass); /* 变换类 */

    gobject_class->set_property = gst_udpjson_meta_set_property;
    gobject_class->get_property = gst_udpjson_meta_get_property;
    gobject_class->finalize = gst_udpjson_meta_finalize;

    trans_class->start = GST_DEBUG_FUNCPTR(gst_udpjson_meta_start);
    trans_class->stop = GST_DEBUG_FUNCPTR(gst_udpjson_meta_stop);
    trans_class->transform_ip = GST_DEBUG_FUNCPTR(gst_udpjson_meta_transform_ip);

    gst_element_class_add_static_pad_template(element_class, &sink_factory);
    gst_element_class_add_static_pad_template(element_class, &src_factory);

    gst_element_class_set_details_simple(
        element_class, "DsUdpJsonMeta", "DsUdpJsonMeta Plugin",
        "Receive UDP multicast JSON and attach obj_user_meta_list", "DeepStream");

    g_object_class_install_property(
        gobject_class, PROP_MULTICAST_IP,
        g_param_spec_string("multicast-ip", "Multicast IP",
                            "UDP multicast group IP", DEFAULT_MULTICAST_IP,
                            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_PORT,
        g_param_spec_uint("port", "Port", "UDP port", 1, 65535, DEFAULT_PORT,
                          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_IFACE,
        g_param_spec_string("iface", "Interface",
                            "Network interface name (e.g., eth0)", NULL,
                            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_RECV_BUF_SIZE,
        g_param_spec_uint("recv-buf-size", "Recv Buffer Size",
                          "Socket receive buffer size", 0, G_MAXUINT,
                          0, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_JSON_KEY,
        g_param_spec_string("json-key", "JSON Key",
                            "JSON key to extract as value", DEFAULT_JSON_KEY,
                            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_OBJECT_ID_KEY,
        g_param_spec_string("object-id-key", "Object ID Key",
                            "JSON key for object id", DEFAULT_OBJECT_ID_KEY,
                            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_SOURCE_ID_KEY,
        g_param_spec_string("source-id-key", "Source ID Key",
                            "JSON key for source id", DEFAULT_SOURCE_ID_KEY,
                            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_CACHE_TTL_MS,
        g_param_spec_uint("cache-ttl-ms", "Cache TTL(ms)",
                          "Cache time-to-live in milliseconds", 0, G_MAXUINT,
                          DEFAULT_CACHE_TTL_MS,
                          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_MAX_CACHE_SIZE,
        g_param_spec_uint("max-cache-size", "Max Cache Size",
                          "Max number of cached objects", 0, G_MAXUINT,
                          DEFAULT_MAX_CACHE_SIZE,
                          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

/**
 * @brief 初始化实例。
 *
 * @param self 插件实例。
 */
static void gst_udpjson_meta_init(GstUdpJsonMeta *self)
{
    self->multicast_ip = g_strdup(DEFAULT_MULTICAST_IP);
    self->port = DEFAULT_PORT;
    self->iface = NULL;
    self->recv_buf_size = 0;
    self->json_key = g_strdup(DEFAULT_JSON_KEY);
    self->object_id_key = g_strdup(DEFAULT_OBJECT_ID_KEY);
    self->source_id_key = g_strdup(DEFAULT_SOURCE_ID_KEY);
    self->cache_ttl_ms = DEFAULT_CACHE_TTL_MS;
    self->max_cache_size = DEFAULT_MAX_CACHE_SIZE;
    self->sockfd = -1;
    self->recv_thread = NULL;
    self->stop_flag = 0;

    g_rw_lock_init(&self->cache_lock);
    self->cache = g_hash_table_new_full(udpjson_cache_key_hash, udpjson_cache_key_equal,
                                        udpjson_cache_key_free, udpjson_cache_value_free);

    self->meta_type = (NvDsMetaType)nvds_get_user_meta_type((gchar *)"NVDS_UDP_JSON_META");

    gst_base_transform_set_in_place(GST_BASE_TRANSFORM(self), TRUE);
}

/**
 * @brief 初始化插件。
 *
 * @param plugin 插件指针。
 * @return 成功返回 TRUE。
 */
static gboolean gst_udpjson_meta_plugin_init(GstPlugin *plugin)
{
    GST_DEBUG_CATEGORY_INIT(gst_udpjson_meta_debug, "udpjsonmeta", 0,
                            "udpjsonmeta plugin");
    return gst_element_register(plugin, "udpjsonmeta", GST_RANK_PRIMARY,
                                GST_TYPE_UDPJSON_META);
}

#define PACKAGE "udpjsonmeta"

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  udpjsonmeta,
                  "UDP JSON meta plugin",
                  gst_udpjson_meta_plugin_init,
                  "1.0",
                  "Proprietary",
                  "deepstream-app-custom",
                  "http://nvidia.com")
