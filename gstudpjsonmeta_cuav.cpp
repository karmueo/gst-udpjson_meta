#include "gstudpjsonmeta_cuav.h"
#include <string.h>
#include <stdio.h>
#include <cctype>
#include <gst/gst.h>

/**
 * @brief C-UAV 报文解析器（私有结构体）
 */
struct _CUAVParser
{
    /* 回调函数和用户数据 */
    CUAVGuidanceCallback guidance_callback;
    gpointer guidance_user_data;
    CUAVEOSystemCallback eo_system_callback;
    gpointer eo_system_user_data;
    CUAVServoControlCallback servo_callback;
    gpointer servo_user_data;
    CUAVRawMessageCallback raw_callback;
    gpointer raw_user_data;
    /* 调试控制 */
    gboolean debug_enabled;
};

CUAVParser *cuav_parser_new(void)
{
    CUAVParser *parser = (CUAVParser *)g_malloc0(sizeof(CUAVParser));
    parser->debug_enabled = FALSE;
    return parser;
}

void cuav_parser_free(CUAVParser *parser)
{
    if (parser)
    {
        g_free(parser);
    }
}

void cuav_parser_set_debug(CUAVParser *parser, gboolean enable)
{
    if (parser)
    {
        parser->debug_enabled = enable;
    }
}

/**
 * @brief 解析 JSON 数值
 */
static gboolean cuav_parse_double(JsonObject *obj, const gchar *name, gdouble *out)
{
    JsonNode *node = json_object_get_member(obj, name);
    if (!node)
        return FALSE;

    GType vtype = json_node_get_value_type(node);
    if (vtype == G_TYPE_DOUBLE)
    {
        *out = json_node_get_double(node);
        return TRUE;
    }
    if (vtype == G_TYPE_FLOAT)
    {
        *out = (gdouble)json_node_get_double(node);
        return TRUE;
    }
    if (vtype == G_TYPE_INT64)
    {
        *out = (gdouble)json_node_get_int(node);
        return TRUE;
    }
    if (vtype == G_TYPE_INT)
    {
        *out = (gdouble)json_node_get_int(node);
        return TRUE;
    }
    if (vtype == G_TYPE_STRING)
    {
        *out = g_ascii_strtod(json_node_get_string(node), NULL);
        return TRUE;
    }
    return FALSE;
}

static gboolean cuav_parse_float(JsonObject *obj, const gchar *name, gfloat *out)
{
    gdouble val = 0;
    if (cuav_parse_double(obj, name, &val))
    {
        *out = (gfloat)val;
        return TRUE;
    }
    return FALSE;
}

static gboolean cuav_parse_uint16(JsonObject *obj, const gchar *name, guint16 *out)
{
    JsonNode *node = json_object_get_member(obj, name);
    if (!node)
        return FALSE;

    GType vtype = json_node_get_value_type(node);
    if (vtype == G_TYPE_INT64)
    {
        gint64 val = json_node_get_int(node);
        *out = (guint16)(val > 0 ? val : 0);
        return TRUE;
    }
    if (vtype == G_TYPE_INT)
    {
        gint64 val = json_node_get_int(node);
        *out = (guint16)(val > 0 ? val : 0);
        return TRUE;
    }
    if (vtype == G_TYPE_STRING)
    {
        *out = (guint16)g_ascii_strtoull(json_node_get_string(node), NULL, 10);
        return TRUE;
    }
    return FALSE;
}

static gboolean cuav_parse_uint8(JsonObject *obj, const gchar *name, guint8 *out)
{
    guint16 val16 = 0;
    if (cuav_parse_uint16(obj, name, &val16))
    {
        *out = (guint8)val16;
        return TRUE;
    }
    return FALSE;
}

static gboolean cuav_parse_int16(JsonObject *obj, const gchar *name, gint16 *out)
{
    JsonNode *node = json_object_get_member(obj, name);
    if (!node)
        return FALSE;

    GType vtype = json_node_get_value_type(node);
    if (vtype == G_TYPE_INT64)
    {
        gint64 val = json_node_get_int(node);
        *out = (gint16)val;
        return TRUE;
    }
    if (vtype == G_TYPE_INT)
    {
        *out = (gint16)json_node_get_int(node);
        return TRUE;
    }
    if (vtype == G_TYPE_STRING)
    {
        *out = (gint16)g_ascii_strtoll(json_node_get_string(node), NULL, 10);
        return TRUE;
    }
    return FALSE;
}

static gboolean cuav_parse_uint32(JsonObject *obj, const gchar *name, guint32 *out)
{
    JsonNode *node = json_object_get_member(obj, name);
    if (!node)
        return FALSE;

    GType vtype = json_node_get_value_type(node);
    if (vtype == G_TYPE_INT64)
    {
        guint64 val = json_node_get_int(node);
        *out = (guint32)(val > 0 ? val : 0);
        return TRUE;
    }
    if (vtype == G_TYPE_INT)
    {
        gint64 val = json_node_get_int(node);
        *out = (guint32)(val > 0 ? val : 0);
        return TRUE;
    }
    if (vtype == G_TYPE_STRING)
    {
        *out = (guint32)g_ascii_strtoull(json_node_get_string(node), NULL, 10);
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief 解析公共报文头
 */
static void cuav_parse_common_header(JsonObject *common, CUAVCommonHeader *header,
                                     guint64 recv_ts_us)
{
    memset(header, 0, sizeof(CUAVCommonHeader));

    cuav_parse_uint16(common, "msg_id", &header->msg_id);
    cuav_parse_uint16(common, "msg_sn", (guint16 *)&header->msg_sn);
    cuav_parse_uint8(common, "msg_type", &header->msg_type);
    cuav_parse_uint16(common, "tx_sys_id", &header->tx_sys_id);
    cuav_parse_uint16(common, "tx_dev_type", &header->tx_dev_type);
    cuav_parse_uint16(common, "tx_dev_id", &header->tx_dev_id);
    cuav_parse_uint16(common, "tx_subdev_id", &header->tx_subdev_id);
    cuav_parse_uint16(common, "rx_sys_id", &header->rx_sys_id);
    cuav_parse_uint16(common, "rx_dev_type", &header->rx_dev_type);
    cuav_parse_uint16(common, "rx_dev_id", &header->rx_dev_id);
    cuav_parse_uint16(common, "rx_subdev_id", &header->rx_subdev_id);

    cuav_parse_uint16(common, "yr", &header->yr);
    cuav_parse_uint8(common, "mo", &header->mo);
    cuav_parse_uint8(common, "dy", &header->dy);
    cuav_parse_uint8(common, "h", &header->h);
    cuav_parse_uint8(common, "min", &header->min);
    cuav_parse_uint8(common, "sec", &header->sec);
    cuav_parse_float(common, "msec", &header->msec);

    cuav_parse_uint8(common, "cont_type", &header->cont_type);
    cuav_parse_uint16(common, "cont_sum", &header->cont_sum);

    header->recv_ts_us = recv_ts_us;
}

/**
 * @brief 解析引导信息
 */
static void cuav_parse_guidance(JsonObject *specific, CUAVGuidanceInfo *guidance)
{
    memset(guidance, 0, sizeof(CUAVGuidanceInfo));

    cuav_parse_uint16(specific, "yr", &guidance->yr);
    cuav_parse_uint8(specific, "mo", &guidance->mo);
    cuav_parse_uint8(specific, "dy", &guidance->dy);
    cuav_parse_uint8(specific, "h", &guidance->h);
    cuav_parse_uint8(specific, "min", &guidance->min);
    cuav_parse_uint8(specific, "sec", &guidance->sec);
    cuav_parse_float(specific, "msec", &guidance->msec);

    cuav_parse_uint32(specific, "tar_id", &guidance->tar_id);
    cuav_parse_uint16(specific, "tar_category", &guidance->tar_category);
    cuav_parse_uint8(specific, "guid_stat", &guidance->guid_stat);

    cuav_parse_double(specific, "ecef_x", &guidance->ecef_x);
    cuav_parse_double(specific, "ecef_y", &guidance->ecef_y);
    cuav_parse_double(specific, "ecef_z", &guidance->ecef_z);
    cuav_parse_double(specific, "ecef_vx", &guidance->ecef_vx);
    cuav_parse_double(specific, "ecef_vy", &guidance->ecef_vy);
    cuav_parse_double(specific, "ecef_vz", &guidance->ecef_vz);

    cuav_parse_float(specific, "h_dvi_pct", &guidance->h_dvi_pct);
    cuav_parse_float(specific, "v_dvi_pct", &guidance->v_dvi_pct);

    cuav_parse_double(specific, "enu_r", &guidance->enu_r);
    cuav_parse_double(specific, "enu_a", &guidance->enu_a);
    cuav_parse_double(specific, "enu_e", &guidance->enu_e);
    cuav_parse_double(specific, "enu_v", &guidance->enu_v);
    cuav_parse_double(specific, "enu_h", &guidance->enu_h);

    cuav_parse_double(specific, "lon", &guidance->lon);
    cuav_parse_double(specific, "lat", &guidance->lat);
    cuav_parse_double(specific, "alt", &guidance->alt);
}

/**
 * @brief 解析光电系统参数
 */
static void cuav_parse_eo_system(JsonObject *specific, CUAVEOSystemParam *eo_param)
{
    memset(eo_param, 0, sizeof(CUAVEOSystemParam));

    cuav_parse_uint8(specific, "sv_stat", &eo_param->sv_stat);
    cuav_parse_uint16(specific, "sv_err", &eo_param->sv_err);
    cuav_parse_uint8(specific, "st_mode_h", &eo_param->st_mode_h);
    cuav_parse_uint8(specific, "st_mode_v", &eo_param->st_mode_v);
    cuav_parse_float(specific, "st_loc_h", &eo_param->st_loc_h);
    cuav_parse_float(specific, "st_loc_v", &eo_param->st_loc_v);

    cuav_parse_uint8(specific, "pt_stat", &eo_param->pt_stat);
    cuav_parse_uint16(specific, "pt_err", &eo_param->pt_err);
    cuav_parse_float(specific, "pt_focal", &eo_param->pt_focal);
    cuav_parse_uint16(specific, "pt_focus", &eo_param->pt_focus);
    cuav_parse_float(specific, "pt_fov_h", &eo_param->pt_fov_h);
    cuav_parse_float(specific, "pt_fov_v", &eo_param->pt_fov_v);

    cuav_parse_uint8(specific, "ir_stat", &eo_param->ir_stat);
    cuav_parse_uint16(specific, "ir_err", &eo_param->ir_err);
    cuav_parse_float(specific, "ir_focal", &eo_param->ir_focal);
    cuav_parse_uint16(specific, "ir_focus", &eo_param->ir_focus);
    cuav_parse_float(specific, "ir_fov_h", &eo_param->ir_fov_h);
    cuav_parse_float(specific, "ir_fov_v", &eo_param->ir_fov_v);

    cuav_parse_uint8(specific, "dm_stat", &eo_param->dm_stat);
    cuav_parse_uint16(specific, "dm_err", &eo_param->dm_err);
    cuav_parse_uint8(specific, "dm_dev", &eo_param->dm_dev);

    cuav_parse_uint8(specific, "trk_dev", &eo_param->trk_dev);
    cuav_parse_uint8(specific, "pt_trk_link", &eo_param->pt_trk_link);
    cuav_parse_uint8(specific, "ir_trk_link", &eo_param->ir_trk_link);
    cuav_parse_uint8(specific, "trk_str", &eo_param->trk_str);
    cuav_parse_uint8(specific, "trk_mod", &eo_param->trk_mod);
    cuav_parse_uint8(specific, "det_trk", &eo_param->det_trk);
    cuav_parse_uint8(specific, "trk_stat", &eo_param->trk_stat);
    cuav_parse_uint8(specific, "pt_zoom", &eo_param->pt_zoom);
    cuav_parse_uint8(specific, "ir_zoom", &eo_param->ir_zoom);
    cuav_parse_uint8(specific, "pt_focus_mode", &eo_param->pt_focus_mode);
    cuav_parse_uint8(specific, "ir_focus_mode", &eo_param->ir_focus_mode);
}

/**
 * @brief 解析光电伺服控制
 */
static void cuav_parse_servo_control(JsonObject *specific, CUAVServoControl *servo)
{
    memset(servo, 0, sizeof(CUAVServoControl));

    cuav_parse_uint8(specific, "dev_id", &servo->dev_id);
    cuav_parse_uint8(specific, "dev_en", &servo->dev_en);
    cuav_parse_uint8(specific, "ctrl_en", &servo->ctrl_en);
    cuav_parse_uint8(specific, "mode_h", &servo->mode_h);
    cuav_parse_uint8(specific, "mode_v", &servo->mode_v);
    cuav_parse_uint8(specific, "speed_en_h", &servo->speed_en_h);
    cuav_parse_uint8(specific, "speed_h", &servo->speed_h);
    cuav_parse_uint8(specific, "speed_en_v", &servo->speed_en_v);
    cuav_parse_uint8(specific, "speed_v", &servo->speed_v);
    cuav_parse_uint8(specific, "loc_en_h", &servo->loc_en_h);
    cuav_parse_float(specific, "loc_h", &servo->loc_h);
    cuav_parse_uint8(specific, "loc_en_v", &servo->loc_en_v);
    cuav_parse_float(specific, "loc_v", &servo->loc_v);
    cuav_parse_uint8(specific, "offset_en", &servo->offset_en);
    cuav_parse_int16(specific, "offset_h", &servo->offset_h);
    cuav_parse_int16(specific, "offset_v", &servo->offset_v);
}

/**
 * @brief 从 cont 数组中获取第一个具体信息对象
 */
static JsonObject *cuav_get_specific_from_cont(JsonArray *cont)
{
    guint len = json_array_get_length(cont);
    for (guint i = 0; i < len; i++)
    {
        JsonNode *node = json_array_get_element(cont, i);
        if (node && JSON_NODE_HOLDS_OBJECT(node))
        {
            JsonObject *item = json_node_get_object(node);
            if (json_object_has_member(item, "具体信息"))
            {
                JsonNode *spec_node = json_object_get_member(item, "具体信息");
                if (spec_node && JSON_NODE_HOLDS_OBJECT(spec_node))
                {
                    return json_node_get_object(spec_node);
                }
            }
        }
    }
    return NULL;
}

gboolean cuav_parser_parse(CUAVParser *parser, const gchar *data, gssize len)
{
    JsonParser *json_parser = NULL;
    JsonNode *root = NULL;
    JsonObject *root_obj = NULL;
    JsonObject *common = NULL;
    JsonObject *specific = NULL;
    guint16 msg_id = 0;
    guint64 recv_ts_us = 0;
    CUAVCommonHeader header;
    gboolean result = FALSE;

    if (!parser || !data || len <= 0)
        return FALSE;

    recv_ts_us = (guint64)g_get_monotonic_time();

    json_parser = json_parser_new();
    if (!json_parser_load_from_data(json_parser, data, len, NULL))
    {
        if (parser->debug_enabled)
        {
            GST_WARNING("[CUAV] Failed to parse JSON");
        }
        goto cleanup;
    }

    root = json_parser_get_root(json_parser);
    if (!root || !JSON_NODE_HOLDS_OBJECT(root))
    {
        GST_WARNING("Root is not a JSON object");
        goto cleanup;
    }

    root_obj = json_node_get_object(root);

    /* 获取公共内容 */
    if (json_object_has_member(root_obj, "公共内容"))
    {
        JsonNode *common_node = json_object_get_member(root_obj, "公共内容");
        if (common_node && JSON_NODE_HOLDS_OBJECT(common_node))
        {
            common = json_node_get_object(common_node);
        }
    }

    if (!common)
    {
        GST_WARNING("No common header found");
        goto cleanup;
    }

    /* 解析公共报文头 */
    cuav_parse_common_header(common, &header, recv_ts_us);
    msg_id = header.msg_id;

    /* 获取具体信息 */
    if (json_object_has_member(root_obj, "具体信息"))
    {
        JsonNode *spec_node = json_object_get_member(root_obj, "具体信息");
        if (spec_node && JSON_NODE_HOLDS_OBJECT(spec_node))
        {
            specific = json_node_get_object(spec_node);
        }
    }
    else if (json_object_has_member(root_obj, "cont"))
    {
        JsonNode *cont_node = json_object_get_member(root_obj, "cont");
        if (cont_node && JSON_NODE_HOLDS_ARRAY(cont_node))
        {
            specific = cuav_get_specific_from_cont(json_node_get_array(cont_node));
        }
    }

    if (!specific)
    {
        GST_WARNING("No specific info found for msg_id=0x%04X", msg_id);
        goto cleanup;
    }

    /* 根据 msg_id 分发处理 */
    switch (msg_id)
    {
    case CUAV_MSG_ID_GUIDANCE:
    {
        CUAVGuidanceInfo guidance;
        cuav_parse_guidance(specific, &guidance);

        /* 调试打印 */
        if (parser->debug_enabled)
        {
            cuav_print_guidance(&guidance);
        }

        if (parser->guidance_callback)
        {
            parser->guidance_callback(&header, &guidance, parser->guidance_user_data);
        }
        if (parser->raw_callback)
        {
            parser->raw_callback(&header, specific, parser->raw_user_data);
        }
        GST_DEBUG("Parsed GUIDANCE: tar_id=%u, guid_stat=%u, enu_a=%.2f, enu_e=%.2f",
                  guidance.tar_id, guidance.guid_stat, guidance.enu_a, guidance.enu_e);
        result = TRUE;
        break;
    }
    case CUAV_MSG_ID_EO_SYSTEM:
    {
        CUAVEOSystemParam eo_param;
        cuav_parse_eo_system(specific, &eo_param);

        /* 调试打印 */
        if (parser->debug_enabled)
        {
            cuav_print_eo_system(&eo_param);
        }

        if (parser->eo_system_callback)
        {
            parser->eo_system_callback(&header, &eo_param, parser->eo_system_user_data);
        }
        if (parser->raw_callback)
        {
            parser->raw_callback(&header, specific, parser->raw_user_data);
        }
        GST_DEBUG("Parsed EO_SYSTEM: sv_stat=%u, st_loc_h=%.2f, st_loc_v=%.2f",
                  eo_param.sv_stat, eo_param.st_loc_h, eo_param.st_loc_v);
        result = TRUE;
        break;
    }
    case CUAV_MSG_ID_EO_SERVO:
    {
        CUAVServoControl servo;
        cuav_parse_servo_control(specific, &servo);

        /* 调试打印 */
        if (parser->debug_enabled)
        {
            cuav_print_servo_control(&servo);
        }

        if (parser->servo_callback)
        {
            parser->servo_callback(&header, &servo, parser->servo_user_data);
        }
        if (parser->raw_callback)
        {
            parser->raw_callback(&header, specific, parser->raw_user_data);
        }
        GST_DEBUG("Parsed EO_SERVO: mode_h=%u, mode_v=%u, loc_h=%.2f, loc_v=%.2f",
                  servo.mode_h, servo.mode_v, servo.loc_h, servo.loc_v);
        result = TRUE;
        break;
    }
    default:
        if (parser->debug_enabled)
        {
            GST_INFO("[CUAV] 未处理报文: msg_id=0x%04X", msg_id);
        }
        if (parser->raw_callback)
        {
            parser->raw_callback(&header, specific, parser->raw_user_data);
        }
        result = TRUE;
        break;
    }

cleanup:
    if (json_parser)
        g_object_unref(json_parser);
    return result;
}

void cuav_parser_set_guidance_callback(CUAVParser *parser, CUAVGuidanceCallback callback,
                                       gpointer user_data)
{
    if (parser)
    {
        parser->guidance_callback = callback;
        parser->guidance_user_data = user_data;
    }
}

void cuav_parser_set_eo_system_callback(CUAVParser *parser, CUAVEOSystemCallback callback,
                                        gpointer user_data)
{
    if (parser)
    {
        parser->eo_system_callback = callback;
        parser->eo_system_user_data = user_data;
    }
}

void cuav_parser_set_servo_control_callback(CUAVParser *parser, CUAVServoControlCallback callback,
                                            gpointer user_data)
{
    if (parser)
    {
        parser->servo_callback = callback;
        parser->servo_user_data = user_data;
    }
}

void cuav_parser_set_raw_callback(CUAVParser *parser, CUAVRawMessageCallback callback,
                                  gpointer user_data)
{
    if (parser)
    {
        parser->raw_callback = callback;
        parser->raw_user_data = user_data;
    }
}

/**
 * @brief 报文类型名称表
 */
static const gchar *cuav_msg_type_names[] = {
    "控制",    /* 0 */
    "回馈",    /* 1 */
    "查询",    /* 2 */
    "数据流",  /* 3 */
    "未知",    /* 4-99 */
    "未知",    /* 100 */
};

const gchar *cuav_get_msg_type_name(guint8 msg_type)
{
    if (msg_type <= 3)
        return cuav_msg_type_names[msg_type];
    if (msg_type == 100)
        return cuav_msg_type_names[5];
    return cuav_msg_type_names[4];
}

/**
 * @brief 报文ID名称表
 */
static const struct
{
    guint16 msg_id;
    const gchar *name;
} cuav_msg_id_names[] = {
    {CUAV_MSG_ID_CMD, "指令"},
    {CUAV_MSG_ID_DEV_CONFIG, "设备配置参数"},
    {CUAV_MSG_ID_GUIDANCE, "引导信息"},
    {CUAV_MSG_ID_TARGET1, "目标信息1"},
    {CUAV_MSG_ID_TARGET2, "目标信息2"},
    {CUAV_MSG_ID_EO_SYSTEM, "光电系统参数"},
    {CUAV_MSG_ID_EO_BIT, "光电BIT状态"},
    {CUAV_MSG_ID_EO_TRACK, "光电跟踪控制"},
    {CUAV_MSG_ID_EO_SERVO, "光电伺服控制"},
    {CUAV_MSG_ID_EO_PT, "可见光控制"},
    {CUAV_MSG_ID_EO_IR, "红外控制"},
    {CUAV_MSG_ID_EO_DM, "光电测距控制"},
    {CUAV_MSG_ID_EO_BOX, "手框目标区"},
    {CUAV_MSG_ID_EO_REC, "光电录像"},
    {CUAV_MSG_ID_EO_AUX, "配套控制"},
    {CUAV_MSG_ID_EO_IMG, "图像控制"},
};

const gchar *cuav_get_msg_id_name(guint16 msg_id)
{
    guint n = sizeof(cuav_msg_id_names) / sizeof(cuav_msg_id_names[0]);
    for (guint i = 0; i < n; i++)
    {
        if (cuav_msg_id_names[i].msg_id == msg_id)
            return cuav_msg_id_names[i].name;
    }
    static gchar buf[32];
    snprintf(buf, sizeof(buf), "未知(0x%04X)", msg_id);
    return buf;
}

/**
 * @brief 目标类型名称表
 */
static const struct
{
    guint16 target_type;
    const gchar *name;
} cuav_target_type_names[] = {
    {CUAV_TARGET_UNKNOWN, "不明"},
    {CUAV_TARGET_BIRDS, "鸟群"},
    {CUAV_TARGET_BALLOON, "空飘物"},
    {CUAV_TARGET_AIRPLANE, "飞机"},
    {CUAV_TARGET_CAR, "汽车"},
    {CUAV_TARGET_BIG_BIRD, "大鸟"},
    {CUAV_TARGET_SMALL_BIRD, "小鸟"},
    {CUAV_TARGET_PERSON, "行人"},
    {CUAV_TARGET_CRUISE_MISSILE, "巡航导弹"},
    {CUAV_TARGET_UAV, "无人机"},
    {CUAV_TARGET_UNKNOWN2, "未知"},
};

const gchar *cuav_get_target_type_name(guint16 target_type)
{
    guint n = sizeof(cuav_target_type_names) / sizeof(cuav_target_type_names[0]);
    for (guint i = 0; i < n; i++)
    {
        if (cuav_target_type_names[i].target_type == target_type)
            return cuav_target_type_names[i].name;
    }
    static gchar buf[32];
    snprintf(buf, sizeof(buf), "未知(%u)", target_type);
    return buf;
}

void cuav_print_guidance(const CUAVGuidanceInfo *guidance)
{
    printf("[CUAV] === 引导信息 ===\n");
    printf("[CUAV]   时间: %u-%02u-%02u %02u:%02u:%02u.%.0f\n",
           guidance->yr, guidance->mo, guidance->dy,
           guidance->h, guidance->min, guidance->sec, guidance->msec);
    printf("[CUAV]   批号: %u, 类别: %u(%s), 状态: %u\n",
           guidance->tar_id, guidance->tar_category,
           cuav_get_target_type_name(guidance->tar_category),
           guidance->guid_stat);
    printf("[CUAV]   ECEF: (%.2f, %.2f, %.2f)\n",
           guidance->ecef_x, guidance->ecef_y, guidance->ecef_z);
    printf("[CUAV]   ENU: 距离=%.2f, 方位=%.2f°, 俯仰=%.2f°\n",
           guidance->enu_r, guidance->enu_a, guidance->enu_e);
    printf("[CUAV]   经纬高: (%.6f, %.6f, %.2f)\n",
           guidance->lon, guidance->lat, guidance->alt);
    fflush(stdout);
}

void cuav_print_eo_system(const CUAVEOSystemParam *eo_param)
{
    static const gchar *sv_stat_names[] = {"无效", "正常", "自检", "预热", "错误"};
    static const gchar *trk_stat_names[] = {"非跟踪", "跟踪正常", "未知", "失锁", "丢失"};

    printf("[CUAV] === 光电系统参数 ===\n");
    printf("[CUAV]   伺服状态: %u(%s)\n",
           eo_param->sv_stat,
           eo_param->sv_stat <= 4 ? sv_stat_names[eo_param->sv_stat] : "未知");
    printf("[CUAV]   伺服指向: 水平=%.2f°, 垂直=%.2f°\n",
           eo_param->st_loc_h, eo_param->st_loc_v);
    printf("[CUAV]   可见光: 焦距=%.1f, 聚焦=%u\n",
           eo_param->pt_focal, eo_param->pt_focus);
    printf("[CUAV]   红外: 焦距=%.1f, 聚焦=%u\n",
           eo_param->ir_focal, eo_param->ir_focus);
    printf("[CUAV]   跟踪: 设备=%u, 联动=光电%u/红外%u, 状态=%u(%s)\n",
           eo_param->trk_dev, eo_param->pt_trk_link,
           eo_param->ir_trk_link, eo_param->trk_stat,
           eo_param->trk_stat <= 4 ? trk_stat_names[eo_param->trk_stat] : "未知");
    fflush(stdout);
}

void cuav_print_servo_control(const CUAVServoControl *servo)
{
    printf("[CUAV] === 光电伺服控制 ===\n");
    printf("[CUAV]   设备: dev_id=%u, dev_en=%u, ctrl_en=%u\n",
           servo->dev_id, servo->dev_en, servo->ctrl_en);
    printf("[CUAV]   控制模式: 水平=%s, 垂直=%s\n",
           servo->mode_h ? "跟踪" : "手动",
           servo->mode_v ? "跟踪" : "手动");
    printf("[CUAV]   速度: 水平=%u, 垂直=%u\n",
           servo->speed_h, servo->speed_v);
    printf("[CUAV]   位置: 水平=%.2f°, 垂直=%.2f°\n",
           servo->loc_h, servo->loc_v);
    fflush(stdout);
}
