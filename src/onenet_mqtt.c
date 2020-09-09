/*
 * File      : onenet_mqtt.c
 * COPYRIGHT (C) 2006 - 2018, RT-Thread Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-04-24     chenyong     first version
 * 2020-08-05     Shine       Support multiple service areas
 */
#include <stdlib.h>
#include <string.h>
#include <string.h>

#include <cJSON_util.h>

#include <paho_mqtt.h>

#include <onenet.h>
#include <app_mqtt.h>

#ifdef RT_USING_DFS
#include <dfs_posix.h>
#endif

#include <onenet.h>
#define DBG_TAG "onenet.mqtt"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define  ONENET_TOPIC_DP    "$dp"

/**
 * MQTT client 队列
 */
static MQTTClient mq_client[ONENET_CLIENT_MAX];

/**
 * OneNET 设备队列
 */
static struct onenet_device
{
    struct rt_onenet_info *onenet_info;
} onenet_client[ONENET_CLIENT_MAX];

/**
 * 根据MQTTClient查找group号
 * @param client
 * @return group编号
 *  0：没找到
 *  1-ONENET_CLIENT_MAX：对应编号
 */
int find_group_by_mqclient(MQTTClient *client)
{
    for (int i = 0; i < ONENET_CLIENT_MAX; i++)
    {
        if (client == &mq_client[i])
            return i;
    }
    return 0;
}

/**
 * 所有消息的 defaultMessageHandler ,此处仅打印数据内容
 * @param client
 * @param msg_data
 */
static void mqtt_callback(MQTTClient *client, MessageData *msg_data)
{
    int group = find_group_by_mqclient(client);
    char *topic = rt_malloc(msg_data->topicName->lenstring.len + 1);
    char *payload = rt_malloc(msg_data->message->payloadlen + 1);

    strncpy(topic, msg_data->topicName->lenstring.data, msg_data->topicName->lenstring.len);
    topic[msg_data->topicName->lenstring.len] = '\0';
    strncpy(payload, msg_data->message->payload, msg_data->message->payloadlen);
    payload[msg_data->message->payloadlen] = '\0';

    log_d("mqtt_callback:group=>%d,topic=>%s,payload=>%s",group,topic,payload);

    rt_free(topic);
    rt_free(payload);
}

static void mqtt_connect_callback(MQTTClient *c)
{
    log_d("Enter mqtt_connect_callback!");
}

static void mqtt_online_callback(MQTTClient *c)
{
    paho_mqtt_subscribe(c, 1, "setting", mqtt_setting_sub_callback);
    log_d("Enter mqtt_online_callback!");
}

static void mqtt_offline_callback(MQTTClient *c)
{
    log_d("Enter mqtt_offline_callback!");
}

/**
 * ONENET MQTT线程入口
 * @param group                 参数分组ID
 * @param onenet_info           onenet连接信息
 * @return
 */
static rt_err_t onenet_mqtt_entry(int group, rt_onenet_info_t onenet_info)
{
    MQTTPacket_connectData condata = MQTTPacket_connectData_initializer;

    mq_client[group].uri = onenet_info->server_uri;
    memcpy(&(mq_client[group].condata), &condata, sizeof(condata));
    mq_client[group].condata.clientID.cstring = onenet_info->device_id;
    mq_client[group].condata.keepAliveInterval = 30;
    mq_client[group].condata.cleansession = 1;
    mq_client[group].condata.username.cstring = onenet_info->pro_id;
    mq_client[group].condata.password.cstring = onenet_info->auth_info;

    mq_client[group].buf_size = mq_client[group].readbuf_size = 1024 * 2;
    mq_client[group].buf = (unsigned char *) rt_calloc(1, mq_client[group].buf_size);
    mq_client[group].readbuf = (unsigned char *) rt_calloc(1, mq_client[group].readbuf_size);
    if (!(mq_client[group].buf && mq_client[group].readbuf))
    {
        log_e("No memory for MQTT client buffer!");
        return -RT_ENOMEM;
    }

    /* registered callback */
    mq_client[group].connect_callback = mqtt_connect_callback;
    mq_client[group].online_callback = mqtt_online_callback;
    mq_client[group].offline_callback = mqtt_offline_callback;

    mq_client[group].defaultMessageHandler = mqtt_callback;

    paho_mqtt_start(&mq_client[group]);

    return RT_EOK;
}

/**
 * 获取onenet的连接信息
 * @param group                 参数分组ID
 * @param onenet_info           返回的onenet连接信息
 * @return
 */
static rt_err_t onenet_get_info(int group, struct rt_onenet_info *onenet_info)
{
    rt_err_t ret = RT_EOK;
    char *name = rt_calloc(1, ONENET_INFO_NAME_LEN + 1);            //注册时设备名称
    char *auth_info = rt_calloc(1, ONENET_INFO_AUTH_LEN + 1);       //SN码
    char *reg_url = rt_calloc(1, ONENET_INFO_REG_URL_LEN + 1);      //注册URL
    char *reg_code = rt_calloc(1, ONENET_INFO_REG_CODE_LEN + 1);    //注册码
    char *reg_mkey = rt_calloc(1, ONENET_INFO_REG_MKEY_LEN + 1);    //注册master_key

    char *dev_id = rt_calloc(1, ONENET_INFO_DEVID_LEN + 1);         //设备ID
    char *pro_id = rt_calloc(1, ONENET_INFO_PROID_LEN + 1);         //产品ID

    char *server_url = rt_calloc(1, ONENET_SERVER_URL_LEN + ONENET_SERVER_PORT_LEN + 1);     //MQTT服务器IP
    char *api_key = rt_calloc(1, ONENET_INFO_APIKEY_LEN);       //HTTP方式上传api_key

    if (!onenet_port_is_registed(group))
    {
        if (onenet_port_get_register_info(group, name, auth_info, reg_url, reg_code, reg_mkey) < 0)
        {
            log_e("onenet get register info fail!");
            ret = -RT_ERROR;
        }

        if (onenet_http_register_device(group, name, auth_info, reg_url, reg_code, reg_mkey) < 0)
        {
            log_e("onenet register device fail! ");
            ret = -RT_ERROR;
        }
    }

    if (onenet_port_get_device_info(group, pro_id, dev_id, auth_info, server_url, api_key) < 0)
    {
        log_e("onenet get device id fail");
        ret = -RT_ERROR;
    }

    strncpy(onenet_info->pro_id, pro_id, strlen(pro_id));
    strncpy(onenet_info->device_id, dev_id, strlen(dev_id));
    strncpy(onenet_info->api_key, api_key, strlen(api_key));
    strncpy(onenet_info->auth_info, auth_info, strlen(auth_info));
    strncpy(onenet_info->server_uri, server_url, strlen(server_url));

    rt_free(name);
    rt_free(auth_info);
    rt_free(reg_url);
    rt_free(reg_code);
    rt_free(reg_mkey);
    rt_free(dev_id);
    rt_free(pro_id);
    rt_free(server_url);
    rt_free(api_key);

    return ret;
}

/**
 * ONENET MQTT初始化函数 *
 * @param group                 参数分组ID
 * @return  0 : init success
 *         -1 : get device info fail
 *         -2 : onenet mqtt client init fail
 *         -3 : the group is larger than the max onenet_client num
 *         -4 : no memey for onenet_mqtt_init
 */
rt_err_t onenet_mqtt_init(int group)
{
    int result = 0;
    struct rt_onenet_info *onenet_info; //用于保存onenet连接信息的指针

    if (group >= ONENET_CLIENT_MAX)
    {
        result = -3;
        goto __exit;
    }

    //onenet_client[group].onenet_info有数据说明被初始化了
    if (onenet_client[group].onenet_info != RT_NULL)
    {
        log_d("onenet mqtt already init!");
        return 0;
    }

    //为client中的onenet_info申请内存
    onenet_info = rt_malloc(sizeof(struct rt_onenet_info));
    memset(onenet_info, 0, sizeof(struct rt_onenet_info));
    if (onenet_info == RT_NULL)
    {
        result = -4;
        goto __exit;
    }
    //获取onenet的连接信息
    if (onenet_get_info(group, onenet_info) < 0)
    {
        result = -1;
        goto __exit;
    }

    onenet_client[group].onenet_info = onenet_info;

    if (onenet_mqtt_entry(group, onenet_info) < 0)
    {
        result = -2;
        goto __exit;
    }

    __exit: if (!result)
    {
        log_i("RT-Thread OneNET package(V%s) initialize success.", ONENET_SW_VERSION);
    }
    else
    {
        log_e("RT-Thread OneNET package(V%s) initialize failed(%d).", ONENET_SW_VERSION, result);
    }
    return result;
}

/**
 * MQTT发布消息msg到主题topic
 * @param group                 参数分组ID
 * @param topic   target topic
 * @param msg     message to be sent
 * @param len     message length
 *
 * @return  0 : publish success
 *         -1 : publish fail
 */
rt_err_t onenet_mqtt_publish(int group, const char *topic, const uint8_t *msg, size_t len)
{
    MQTTMessage message;

    RT_ASSERT(topic);
    RT_ASSERT(msg);

    message.qos = QOS1;
    message.retained = 0;
    message.payload = (void *) msg;
    message.payloadlen = len;

    if (MQTTPublish(&mq_client[group], topic, &message) < 0)
    {
        return -1;
    }

    return 0;
}

/**
 * 数字信息打包函数
 * @param ds_name   变量名
 * @param digit     变量值
 * @param out_buff  生成的buffer指针，由此函数malloc出的，需要调用的函数手动free
 * @param length    生成的buffer长度
 * @return
 */
static rt_err_t onenet_mqtt_get_digit_data(const char *ds_name, const double digit, char **out_buff, size_t *length)
{
    rt_err_t result = RT_EOK;
    cJSON *root = RT_NULL;
    char *msg_str = RT_NULL;

    RT_ASSERT(ds_name);
    RT_ASSERT(out_buff);
    RT_ASSERT(length);

    root = cJSON_CreateObject();
    if (!root)
    {
        log_e("MQTT publish digit data failed! cJSON create object error return NULL!");
        return -RT_ENOMEM;
    }

    cJSON_AddNumberToObject(root, ds_name, digit);

    /* render a cJSON structure to buffer */
    msg_str = cJSON_PrintUnformatted(root);
    if (!msg_str)
    {
        log_e("MQTT publish digit data failed! cJSON print unformatted error return NULL!");
        result = -RT_ENOMEM;
        goto __exit;
    }

    *out_buff = rt_malloc(strlen(msg_str) + 3);
    if (!(*out_buff))
    {
        log_e("ONENET mqtt upload digit data failed! No memory for send buffer!");
        return -RT_ENOMEM;
    }

    *length = strlen(msg_str);
    strncpy(&(*out_buff)[3], msg_str, *length);

    /* mqtt head and json length */
    (*out_buff)[0] = 0x03;
    (*out_buff)[1] = (*length & 0xff00) >> 8;
    (*out_buff)[2] = *length & 0xff;
    *length += 3;

    __exit: if (root)
    {
        cJSON_Delete(root);
    }
    if (msg_str)
    {
        cJSON_free(msg_str);
    }

    return result;
}

/**
 *  上传数字信息到OneNET平台
 * @param group     平台序号
 * @param ds_name   变量名
 * @param digit     变量值
 * @return  0 : upload digit data success
 *         -5 : no memory
 */
rt_err_t onenet_mqtt_upload_digit(int group, const char *ds_name, const double digit)
{
    char *send_buffer = RT_NULL;
    rt_err_t result = RT_EOK;
    size_t length = 0;

    RT_ASSERT(ds_name);

    result = onenet_mqtt_get_digit_data(ds_name, digit, &send_buffer, &length);
    if (result < 0)
    {
        goto __exit;
    }

    result = onenet_mqtt_publish(group, ONENET_TOPIC_DP, (uint8_t *) send_buffer, length);
    if (result < 0)
    {
        log_e("onenet publish failed!");
        goto __exit;
    }

    __exit: if (send_buffer)
    {
        rt_free(send_buffer);
    }

    return result;
}

//暂时还没来得及改的
//static rt_err_t onenet_mqtt_get_string_data(const char *ds_name, const char *str, char **out_buff, size_t *length)
//{
//    rt_err_t result = RT_EOK;
//    cJSON *root = RT_NULL;
//    char *msg_str = RT_NULL;
//
//    RT_ASSERT(ds_name);
//    RT_ASSERT(str);
//    RT_ASSERT(out_buff);
//    RT_ASSERT(length);
//
//    root = cJSON_CreateObject();
//    if (!root)
//    {
//        log_e("MQTT publish string data failed! cJSON create object error return NULL!");
//        return -RT_ENOMEM;
//    }
//
//    cJSON_AddStringToObject(root, ds_name, str);
//
//    /* render a cJSON structure to buffer */
//    msg_str = cJSON_PrintUnformatted(root);
//    if (!msg_str)
//    {
//        log_e("MQTT publish string data failed! cJSON print unformatted error return NULL!");
//        result = -RT_ENOMEM;
//        goto __exit;
//    }
//
//    *out_buff = ONENET_MALLOC(strlen(msg_str) + 3);
//    if (!(*out_buff))
//    {
//        log_e("ONENET mqtt upload string data failed! No memory for send buffer!");
//        return -RT_ENOMEM;
//    }
//
//    strncpy(&(*out_buff)[3], msg_str, strlen(msg_str));
//    *length = strlen(&(*out_buff)[3]);
//
//    /* mqtt head and json length */
//    (*out_buff)[0] = 0x03;
//    (*out_buff)[1] = (*length & 0xff00) >> 8;
//    (*out_buff)[2] = *length & 0xff;
//    *length += 3;
//
//    __exit: if (root)
//    {
//        cJSON_Delete(root);
//    }
//    if (msg_str)
//    {
//        cJSON_free(msg_str);
//    }
//
//    return result;
//}
//
///**
// * upload string data to OneNET cloud.
// *
// * @param   ds_name     datastream name
// * @param   str         string data
// *
// * @return  0 : upload digit data success
// *         -5 : no memory
// */
//rt_err_t onenet_mqtt_upload_string(const char *ds_name, const char *str)
//{
//    char *send_buffer = RT_NULL;
//    rt_err_t result = RT_EOK;
//    size_t length = 0;
//
//    RT_ASSERT(ds_name);
//    RT_ASSERT(str);
//
//    result = onenet_mqtt_get_string_data(ds_name, str, &send_buffer, &length);
//    if (result < 0)
//    {
//        goto __exit;
//    }
//
//    result = onenet_mqtt_publish(ONENET_TOPIC_DP, (uint8_t *) send_buffer, length);
//    if (result < 0)
//    {
//        log_e("onenet mqtt publish digit data failed!");
//        goto __exit;
//    }
//
//    __exit: if (send_buffer)
//    {
//        ONENET_FREE(send_buffer);
//    }
//
//    return result;
//}
//
///**
// * set the command responses call back function
// *
// * @param   cmd_rsp_cb  command responses call back function
// *
// * @return  0 : set success
// *         -1 : function is null
// */
//void onenet_set_cmd_rsp_cb(void (*cmd_rsp_cb)(uint8_t *recv_data, size_t recv_size, uint8_t **resp_data, size_t *resp_size))
//{
//
//    onenet_client[MQTT_CLIENT_MEMBER].cmd_rsp_cb = cmd_rsp_cb;
//
//}
//
//static rt_err_t onenet_mqtt_get_bin_data(const char *str, const uint8_t *bin, int binlen, uint8_t **out_buff, size_t *length)
//{
//    rt_err_t result = RT_EOK;
//    cJSON *root = RT_NULL;
//    char *msg_str = RT_NULL;
//
//    RT_ASSERT(str);
//    RT_ASSERT(bin);
//    RT_ASSERT(out_buff);
//    RT_ASSERT(length);
//
//    root = cJSON_CreateObject();
//    if (!root)
//    {
//        log_e("MQTT online push failed! cJSON create object error return NULL!");
//        return -RT_ENOMEM;
//    }
//
//    cJSON_AddStringToObject(root, "ds_id", str);
//
//    /* render a cJSON structure to buffer */
//    msg_str = cJSON_PrintUnformatted(root);
//    if (!msg_str)
//    {
//        log_e("Device online push failed! cJSON print unformatted error return NULL!");
//        result = -RT_ENOMEM;
//        goto __exit;
//    }
//
//    /* size = header(3) + json + binary length(4) + binary length +'\0' */
//    *out_buff = (uint8_t *) ONENET_MALLOC(strlen(msg_str) + 3 + 4 + binlen + 1);
//
//    strncpy(&(*out_buff)[3], msg_str, strlen(msg_str));
//    *length = strlen(&(*out_buff)[3]);
//
//    /* mqtt head and cjson length */
//    (*out_buff)[0] = 0x02;
//    (*out_buff)[1] = (*length & 0xff00) >> 8;
//    (*out_buff)[2] = *length & 0xff;
//    *length += 3;
//
//    /* binary data length */
//    (*out_buff)[(*length)++] = (binlen & 0xff000000) >> 24;
//    (*out_buff)[(*length)++] = (binlen & 0x00ff0000) >> 16;
//    (*out_buff)[(*length)++] = (binlen & 0x0000ff00) >> 8;
//    (*out_buff)[(*length)++] = (binlen & 0x000000ff);
//
//    memcpy(&((*out_buff)[*length]), bin, binlen);
//    *length = *length + binlen;
//
//    __exit: if (root)
//    {
//        cJSON_Delete(root);
//    }
//    if (msg_str)
//    {
//        cJSON_free(msg_str);
//    }
//
//    return result;
//}
//
///**
// * upload binary data to onenet cloud by path
// *
// * @param   ds_name     datastream name
// * @param   bin         binary file
// * @param   len         binary file length
// *
// * @return  0 : upload success
// *         -1 : invalid argument or open file fail
// */
//rt_err_t onenet_mqtt_upload_bin(const char *ds_name, uint8_t *bin, size_t len)
//{
//    size_t length = 0;
//    rt_err_t result = RT_EOK;
//    uint8_t *send_buffer = RT_NULL;
//
//    RT_ASSERT(ds_name);
//    RT_ASSERT(bin);
//
//    result = onenet_mqtt_get_bin_data(ds_name, bin, len, &send_buffer, &length);
//    if (result < 0)
//    {
//        result = -RT_ERROR;
//        goto __exit;
//    }
//
//    result = onenet_mqtt_publish(ONENET_TOPIC_DP, send_buffer, length);
//    if (result < 0)
//    {
//        log_e("onenet publish data failed(%d)!", result);
//        result = -RT_ERROR;
//        goto __exit;
//    }
//
//    __exit: if (send_buffer)
//    {
//        ONENET_FREE(send_buffer);
//    }
//
//    return result;
//}
//
//#ifdef RT_USING_DFS
///**
// * upload binary data to onenet cloud by path
// *
// * @param   ds_name     datastream name
// * @param   bin_path    binary file path
// *
// * @return  0 : upload success
// *         -1 : invalid argument or open file fail
// */
//rt_err_t onenet_mqtt_upload_bin_by_path(const char *ds_name, const char *bin_path)
//{
//    int fd;
//    size_t length = 0, bin_size = 0;
//    size_t bin_len = 0;
//    struct stat file_stat;
//    rt_err_t result = RT_EOK;
//    uint8_t *send_buffer = RT_NULL;
//    uint8_t * bin_array = RT_NULL;
//
//    RT_ASSERT(ds_name);
//    RT_ASSERT(bin_path);
//
//    if (stat(bin_path, &file_stat) < 0)
//    {
//        log_e("get file state fail!, bin path is %s", bin_path);
//        return -RT_ERROR;
//    }
//    else
//    {
//        bin_len = file_stat.st_size;
//        if (bin_len > 3 * 1024 * 1024)
//        {
//            log_e("bin length must be less than 3M, %s length is %d", bin_path, bin_len);
//            return -RT_ERROR;
//        }
//
//    }
//
//    fd = open(bin_path, O_RDONLY);
//    if (fd >= 0)
//    {
//        bin_array = (uint8_t *) ONENET_MALLOC(bin_len);
//
//        bin_size = read(fd, bin_array, file_stat.st_size);
//        close(fd);
//        if (bin_size <= 0)
//        {
//            log_e("read %s file fail!", bin_path);
//            result = -RT_ERROR;
//            goto __exit;
//        }
//    }
//    else
//    {
//        log_e("open %s file fail!", bin_path);
//        return -RT_ERROR;
//    }
//
//    result = onenet_mqtt_get_bin_data(ds_name, bin_array, bin_size, &send_buffer, &length);
//    if (result < 0)
//    {
//        result = -RT_ERROR;
//        goto __exit;
//    }
//
//    result = onenet_mqtt_publish(ONENET_TOPIC_DP, send_buffer, length);
//    if (result < 0)
//    {
//        log_e("onenet publish %s data failed(%d)!", bin_path, result);
//        result = -RT_ERROR;
//        goto __exit;
//    }
//
//    __exit: if (send_buffer)
//    {
//        ONENET_FREE(send_buffer);
//    }
//    if (bin_array)
//    {
//        ONENET_FREE(bin_array);
//    }
//
//    return result;
//}
//#endif /* RT_USING_DFS */

#ifdef FINSH_USING_MSH
#include <finsh.h>
MSH_CMD_EXPORT(onenet_mqtt_init, OneNET cloud mqtt initializate);
#endif
