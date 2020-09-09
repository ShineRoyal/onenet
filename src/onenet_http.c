/*
 * File      : onenet_http.c
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

#include <cJSON_util.h>
#include <webclient.h>

#include <onenet.h>

#include <onenet.h>
#define DBG_TAG "onenet.http"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>


#define ONENET_CON_URI_LEN             256      //ONENET注册URL长度
#define ONENET_HTTP_HEAD_LEN           1024     //ONENET注册HTTP头长度
#define ONENET_SEND_DATA_LEN           1024     //ONENET注册报文长度
#define ONENET_RECV_RESP_LEN           1024     //ONENET注册回复内容长度
#define ONENET_TIME_BUF_LEN            24


#if WEBCLIENT_SW_VERSION_NUM < 0x20000
#error "Please upgrade the webclient version "
#endif

#define WEBCLIENT_HEADER_ADD(session, fmt, ...)                                                         \
    do                                                                                                  \
    {                                                                                                   \
        if (webclient_header_fields_add(session, fmt, ##__VA_ARGS__) < 0)                               \
        {                                                                                               \
            log_e("webclient add header failed!");                                                      \
            goto __exit;                                                                                \
        }                                                                                               \
    } while(0);                                                                                         \
 

/**
 * 注册报文解析函数
 * 将返回的报文解析，并调用onenet_port_save_device_info写入easyflash中
 * @param group     参数分组ID
 * @param rec_buf
 * @param length
 * @return
 */
static rt_err_t response_register_handlers(int group, const unsigned char *rec_buf, const size_t length)
{
    cJSON *root = RT_NULL;
    cJSON *item = RT_NULL;
    cJSON *itemid = RT_NULL;
    cJSON *itemapikey = RT_NULL;

    RT_ASSERT(rec_buf);

    LOG_D("response is %.*s", length, rec_buf);

    root = cJSON_Parse((char *) rec_buf);
    if (!root)
    {
        log_e("onenet register device failed! cJSON Parse data error return NULL!");
        return -RT_ENOMEM;
    }

    item = cJSON_GetObjectItem(root, "errno");
    if (item->valueint == 0)
    {
        itemid = cJSON_GetObjectItem(root->child->next, "device_id");
        itemapikey = cJSON_GetObjectItem(root->child->next, "key");

        onenet_port_save_device_info(group, itemid->valuestring, itemapikey->valuestring);
    }
    else
    {
        log_e("onenet register device failed! errno is %d", item->valueint);
        return -RT_ERROR;
    }

    return RT_EOK;

}

/**
 * 调用webclient进行注册报文的上传与解析
 * @param group         参数分组ID
 * @param send_buffer   上传的报文body内容
 * @param reg_url       注册url
 * @param reg_code      注册码
 * @param mkey          注册时产品的master key
 * @return
 */
static rt_err_t onenet_upload_register_device(int group, char *send_buffer, const char *reg_url, const char *reg_code, const char *mkey)
{
    struct webclient_session *session = RT_NULL;
    char *buffer = send_buffer;
    char *URI = RT_NULL;
    size_t length;
    unsigned char *rec_buf;
    rt_err_t result = RT_EOK;

    RT_ASSERT(send_buffer);

    session = webclient_session_create(ONENET_HTTP_HEAD_LEN);
    if (session == RT_NULL)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    URI = (char *) rt_calloc(1, ONENET_CON_URI_LEN);
    if (URI == RT_NULL)
    {
        log_e("OneNet register device failed! No memory for URI buffer!");
        result = -RT_ENOMEM;
        goto __exit;
    }

    rec_buf = (unsigned char *) rt_calloc(1, ONENET_RECV_RESP_LEN);
    if (rec_buf == RT_NULL)
    {
        log_e("OneNet register device failed! No memory for response data buffer!");
        result = -RT_ENOMEM;
        goto __exit;
    }

    rt_snprintf(URI, ONENET_CON_URI_LEN, reg_url);
    //rt_snprintf(URI, ONENET_CON_URI_LEN, "http://swustal.zicp.net/register_de?register_code=");
    //rt_snprintf(URI, ONENET_CON_URI_LEN, "http://api.heclouds.com/register_de?register_code=");
    strncat(URI, reg_code, ONENET_CON_URI_LEN);

    WEBCLIENT_HEADER_ADD(session, "api-key: %s\r\n", mkey);
    WEBCLIENT_HEADER_ADD(session, "Content-Length: %d\r\n", strlen(buffer));
    WEBCLIENT_HEADER_ADD(session, "Content-Type: application/octet-stream\r\n");

    if (webclient_post(session, URI, buffer) != 200)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    length = webclient_read(session, rec_buf, ONENET_RECV_RESP_LEN);

    if (length > 0)
    {
        response_register_handlers(group, rec_buf, length);
    }
    else
    {
        log_e("OneNet register device failed! Handle response(%d) error!", session->resp_status);
    }

    __exit: if (session)
    {
        webclient_close(session);
    }
    if (URI)
    {
        rt_free(URI);
    }
    if (rec_buf)
    {
        rt_free(rec_buf);
    }
    return result;
}

/**
 * 组建注册信息
 * @param ds_name       注册时的设备名
 * @param auth_info     注册时的SN码
 * @param out_buff      组建完成的json格式注册信息
 * @return              RT_EOK组建成功
 */
static rt_err_t onenet_get_register_device_data(const char *ds_name, const char *auth_info, char *out_buff)
{
    rt_err_t result = RT_EOK;
    cJSON *root = RT_NULL;
    char *msg_str = RT_NULL;

    RT_ASSERT(ds_name);
    RT_ASSERT(auth_info);
    RT_ASSERT(out_buff);

    root = cJSON_CreateObject();
    if (!root)
    {
        log_e("MQTT register device failed! cJSON create object error return NULL!");
        return -RT_ENOMEM;
    }

    cJSON_AddStringToObject(root, "sn", auth_info);
    cJSON_AddStringToObject(root, "title", ds_name);

    /* render a cJSON structure to buffer */
    msg_str = cJSON_PrintUnformatted(root);
    if (!msg_str)
    {
        log_e("Device register device failed! cJSON print unformatted error return NULL!");
        result = -RT_ENOMEM;
        goto __exit;
    }

    strncpy(out_buff, msg_str, strlen(msg_str));

    __exit: if (root)
    {
        cJSON_Delete(root);
    }
    if (msg_str)
    {
        rt_free(msg_str);
    }

    return result;
}

/**
 * 向ONENET平台进行注册
 * @param group         参数分组ID
 * @param name          注册设备名
 * @param auth_info     注册SN码
 * @param url           注册URL
 * @param code          注册码
 * @param mkey          注册产品master key
 * @return  0 : register device success
 *         -5 : no memory
 */
rt_err_t onenet_http_register_device(int group, const char *name, const char *auth_info, const char *url, const char *code, const char *mkey)
{
    char *send_buffer = RT_NULL;
    rt_err_t result = RT_EOK;

    RT_ASSERT(name);
    RT_ASSERT(auth_info);

    send_buffer = (char *) rt_calloc(1, ONENET_SEND_DATA_LEN);
    if (!send_buffer)
    {
        log_e("ONENET register device failed! No memory for send buffer!");
        return -RT_ENOMEM;
    }

    /* get JSON format data */
    result = onenet_get_register_device_data(name, auth_info, send_buffer);
    if (result < 0)
    {
        goto __exit;
    }

    /* send data to cloud by HTTP */
    result = onenet_upload_register_device(group, send_buffer, url, code, mkey);
    if (result < 0)
    {
        goto __exit;
    }

    __exit: if (send_buffer)
    {
        rt_free(send_buffer);
    }

    return result;
}

//暂时还没来得及改的
//static cJSON *response_get_datapoints_handlers(const uint8_t *rec_buf)
//{
//    cJSON *root = RT_NULL;
//    cJSON *root_data = RT_NULL;
//    cJSON *itemdata = RT_NULL;
//    cJSON *item = RT_NULL;
//
//    RT_ASSERT(rec_buf);
//
//    root = cJSON_Parse((char *) rec_buf);
//    if (!root)
//    {
//        log_e("MQTT processing response data failed! cJSON Parse data error return NULL!");
//
//        return RT_NULL;
//    }
//
//    item = cJSON_GetObjectItem(root, "errno");
//    if (item->valueint == 0)
//    {
//        itemdata = cJSON_GetObjectItem(root, "data");
//        root_data = cJSON_Duplicate(itemdata, 1);
//    }
//    else
//    {
//        log_e("get datapoints failed! errno is %d", item->valueint);
//    }
//
//    if (root)
//    {
//        cJSON_Delete(root);
//    }
//
//    return root_data;
//
//}
//
//static cJSON *onenet_http_get_datapoints(char *datastream, char *start, char *end, int duration, size_t limit)
//{
//    struct webclient_session *session = RT_NULL;
//    char *URI = RT_NULL;
//    unsigned char *rec_buf = RT_NULL;
//    cJSON *itemdata = RT_NULL;
//    size_t res_len = 0;
//
//    RT_ASSERT(datastream);
//
//    URI = (char *) ONENET_CALLOC(1, ONENET_CON_URI_LEN);
//    if (URI == RT_NULL)
//    {
//        log_e("OneNet Send data failed! No memory for URI buffer!");
//        goto __exit;
//    }
//
//    rec_buf = (unsigned char *) ONENET_CALLOC(1, ONENET_RECV_RESP_LEN);
//    if (rec_buf == RT_NULL)
//    {
//        log_e("OneNet recvice response data failed! No memory for response data buffer!");
//        goto __exit;
//    }
//
//    rt_snprintf(URI, ONENET_CON_URI_LEN, "http://api.heclouds.com/devices/%s/datapoints?datastream_id=%s", onenet_info.device_id, datastream);
//
//    if (start != RT_NULL)
//    {
//        strcat(URI, "&start=");
//        strcat(URI, start);
//    }
//    if (end != RT_NULL)
//    {
//        strcat(URI, "&end=");
//        strcat(URI, end);
//    }
//    if (duration != RT_NULL)
//    {
//        char number[10];
//        strcat(URI, "&duration=");
//        rt_snprintf(number, 10, "%d", duration);
//        strcat(URI, number);
//    }
//    if (limit != RT_NULL)
//    {
//        char number[10];
//        strcat(URI, "&limit=");
//        rt_snprintf(number, 10, "%d", limit);
//        strcat(URI, number);
//    }
//
//    session = webclient_session_create(ONENET_HTTP_HEAD_LEN);
//    if (session == RT_NULL)
//    {
//        goto __exit;
//    }
//
//    WEBCLIENT_HEADER_ADD(session, "api-key: %s\r\n", onenet_info.api_key);
//    WEBCLIENT_HEADER_ADD(session, "Content-Type: application/octet-stream\r\n");
//
//    if (webclient_get(session, URI) != 200)
//    {
//        goto __exit;
//    }
//
//    res_len = webclient_content_length_get(session);
//
//    if (res_len > ONENET_HTTP_HEAD_LEN)
//    {
//        log_e("response data length(%d) is greater than the default recv buffer size(%d)!", res_len, ONENET_RECV_RESP_LEN);
//        goto __exit;
//    }
//    else if (webclient_read(session, rec_buf, ONENET_RECV_RESP_LEN) > 0)
//    {
//        itemdata = response_get_datapoints_handlers(rec_buf);
//    }
//
//    __exit: if (session)
//    {
//        webclient_close(session);
//    }
//    if (URI)
//    {
//        ONENET_FREE(URI);
//    }
//
//    if (rec_buf)
//    {
//        ONENET_FREE(rec_buf);
//    }
//
//    return itemdata;
//
//}
//
///**
// * get datapoints form onenet cloud by limit
// *
// * @param   ds_name     datastream name
// * @param   limit       the number of datapoints most returned by this request
// *
// * @return  cjson of datapoints
// *
// * @note    returned cJSON need to be free when user finished using the data.
// */
//cJSON *onenet_get_dp_by_limit(char *ds_name, size_t limit)
//{
//    RT_ASSERT(ds_name);
//
//    return onenet_http_get_datapoints(ds_name, RT_NULL, RT_NULL, RT_NULL, limit);
//}
//
///**
// * get datapoints form onenet cloud by start time and end time
// *
// * @param   ds_name     datastream name
// * @param   start       start time
// * @param   end         end time
// * @param   limit       the number of datapoints most returned by this request
// *
// * @return  cjson of datapoints
// *
// * @note    returned cJSON need to be free when user finished using the data.
// */
//cJSON *onenet_get_dp_by_start_end(char *ds_name, uint32_t start, uint32_t end, size_t limit)
//{
//    char start_buf[ONENET_TIME_BUF_LEN] = { 0 }, end_buf[ONENET_TIME_BUF_LEN] = { 0 };
//    struct tm *cur_tm;
//
//    RT_ASSERT(ds_name);
//
//    time_t time = (time_t) (start + 8 * 60 * 60);
//
//    cur_tm = localtime(&time);
//
//    rt_sprintf(start_buf, "%04d-%02d-%02dT%02d:%02d:%02d", cur_tm->tm_year + 1900, cur_tm->tm_mon + 1, cur_tm->tm_mday, cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec);
//
//    time = (time_t) (end + 8 * 60 * 60);
//
//    cur_tm = localtime(&time);
//
//    rt_sprintf(end_buf, "%04d-%02d-%02dT%02d:%02d:%02d", cur_tm->tm_year + 1900, cur_tm->tm_mon + 1, cur_tm->tm_mday, cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec);
//
//    return onenet_http_get_datapoints(ds_name, start_buf, end_buf, RT_NULL, limit);
//
//}
//
///**
// * get datapoints form onenet cloud by start time and duration
// *
// * @param   ds_name     datastream name
// * @param   start       start time
// * @param   duration    query time length
// * @param   limit       the number of datapoints most returned by this request
// *
// * @return  cjson of datapoints
// *
// * @note    returned cJSON need to be free when user finished using the data.
// */
//cJSON *onenet_get_dp_by_start_duration(char *ds_name, uint32_t start, size_t duration, size_t limit)
//{
//    struct tm *cur_tm;
//    char start_buf[ONENET_TIME_BUF_LEN] = { 0 };
//
//    RT_ASSERT(ds_name);
//
//    time_t time = (time_t) (start + 8 * 60 * 60);
//
//    cur_tm = localtime(&time);
//
//    rt_sprintf(start_buf, "%04d-%02d-%02dT%02d:%02d:%02d", cur_tm->tm_year + 1900, cur_tm->tm_mon + 1, cur_tm->tm_mday, cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec);
//
//    return onenet_http_get_datapoints(ds_name, start_buf, RT_NULL, duration, limit);
//
//}
//
//static rt_err_t response_get_datastreams_handlers(const unsigned char *rec_buf, struct rt_onenet_ds_info *datastream)
//{
//    cJSON *root = RT_NULL;
//    cJSON *item = RT_NULL;
//    cJSON *itemdata = RT_NULL;
//    cJSON *itemarray = RT_NULL;
//    rt_err_t result = RT_EOK;
//
//    RT_ASSERT(rec_buf);
//    RT_ASSERT(datastream);
//
//    root = cJSON_Parse((char *) rec_buf);
//    if (!root)
//    {
//        log_e("onenet get datastreams failed! cJSON Parse data error return NULL!");
//        return -RT_ENOMEM;
//    }
//
//    item = cJSON_GetObjectItem(root, "errno");
//    if (item->valueint == 0)
//    {
//        itemdata = cJSON_GetObjectItem(root, "data");
//
//        for (int i = 0; i < cJSON_GetArraySize(itemdata->child); i++)
//        {
//            itemarray = cJSON_GetArrayItem(itemdata->child, i);
//
//            if (strcmp(itemarray->string, "update_at") == 0)
//            {
//                rt_strncpy(datastream->update_time, itemarray->valuestring, ONENET_DATASTREAM_NAME_MAX);
//            }
//            else if (strcmp(itemarray->string, "unit") == 0)
//            {
//                rt_strncpy(datastream->unit, itemarray->valuestring, ONENET_DATASTREAM_NAME_MAX);
//            }
//            else if (strcmp(itemarray->string, "id") == 0)
//            {
//                rt_strncpy(datastream->id, itemarray->valuestring, ONENET_DATASTREAM_NAME_MAX);
//            }
//            else if (strcmp(itemarray->string, "unit_symbol") == 0)
//            {
//                rt_strncpy(datastream->unit_symbol, itemarray->valuestring, ONENET_DATASTREAM_NAME_MAX);
//            }
//            else if (strcmp(itemarray->string, "current_value") == 0)
//            {
//                rt_strncpy(datastream->current_value, itemarray->valuestring, ONENET_DATASTREAM_NAME_MAX);
//            }
//            else if (strcmp(itemarray->string, "create_time") == 0)
//            {
//                rt_strncpy(datastream->create_time, itemarray->valuestring, ONENET_DATASTREAM_NAME_MAX);
//            }
//            else if (strcmp(itemarray->string, "tags") == 0)
//            {
//                rt_strncpy(datastream->tags, itemarray->valuestring, ONENET_DATASTREAM_NAME_MAX);
//            }
//
//        }
//
//    }
//    else
//    {
//        log_e("onenet get datastreams failed! errno is %d", item->valueint);
//        result = -RT_ERROR;
//    }
//
//    if (root)
//    {
//        cJSON_Delete(root);
//    }
//
//    return result;
//
//}
//
///**
// * get datastream information from onenet cloud
// *
// * @param   ds_name     datastream name
// * @param   datastream  struct to save datastream information
// *
// * @return  0 : get datastream information success
// *         -1 : get response fail
// *         -5 : no memory
// */
//rt_err_t onenet_http_get_datastream(const char *ds_name, struct rt_onenet_ds_info *datastream)
//{
//    struct webclient_session *session = RT_NULL;
//    char *URI = RT_NULL;
//    unsigned char *rec_buf = RT_NULL;
//    rt_err_t result = RT_EOK;
//    size_t res_len = 0;
//
//    RT_ASSERT(ds_name);
//    RT_ASSERT(datastream);
//
//    URI = (char *) ONENET_CALLOC(1, ONENET_CON_URI_LEN);
//    if (URI == NULL)
//    {
//        log_e("onenet get datastreams failed! No memory for URI buffer!");
//        result = -RT_ENOMEM;
//        goto __exit;
//    }
//
//    rec_buf = (unsigned char *) ONENET_CALLOC(1, ONENET_RECV_RESP_LEN);
//    if (rec_buf == NULL)
//    {
//        log_e("onenet get datastreams failed! No memory for response data buffer!");
//        result = -RT_ENOMEM;
//        goto __exit;
//    }
//
//    rt_snprintf(URI, ONENET_CON_URI_LEN, "http://api.heclouds.com/devices/%s/datastreams?datastream_ids=%s", onenet_info.device_id, ds_name);
//
//    session = webclient_session_create(ONENET_HTTP_HEAD_LEN);
//    if (session == RT_NULL)
//    {
//        result = -RT_ERROR;
//        goto __exit;
//    }
//
//    WEBCLIENT_HEADER_ADD(session, "api-key: %s\r\n", onenet_info.api_key);
//    WEBCLIENT_HEADER_ADD(session, "Content-Type: application/octet-stream\r\n");
//
//    if (webclient_get(session, URI) != 200)
//    {
//        result = -RT_ERROR;
//        goto __exit;
//    }
//
//    res_len = webclient_content_length_get(session);
//
//    if (res_len > ONENET_HTTP_HEAD_LEN)
//    {
//        log_e("response data length(%d) is greater than the default recv buffer size(%d)!", res_len, ONENET_RECV_RESP_LEN);
//        goto __exit;
//    }
//    else if (webclient_read(session, rec_buf, ONENET_RECV_RESP_LEN) > 0)
//    {
//        if (response_get_datastreams_handlers(rec_buf, datastream) < 0)
//        {
//            result = -RT_ERROR;
//        }
//    }
//    else
//    {
//        result = -RT_ERROR;
//    }
//
//    __exit: if (session)
//    {
//        webclient_close(session);
//    }
//    if (URI)
//    {
//        ONENET_FREE(URI);
//    }
//
//    if (rec_buf)
//    {
//        ONENET_FREE(rec_buf);
//    }
//    return result;
//}
//
//static rt_err_t onenet_upload_data(char *send_buffer)
//{
//    struct webclient_session *session = RT_NULL;
//    char *buffer = send_buffer;
//    char *URI = RT_NULL;
//    rt_err_t result = RT_EOK;
//
//    RT_ASSERT(send_buffer);
//
//    URI = ONENET_CALLOC(1, ONENET_CON_URI_LEN);
//    if (URI == RT_NULL)
//    {
//        log_e("OneNet Send data failed! No memory for URI buffer!");
//        result = -RT_ENOMEM;
//        goto __exit;
//    }
//
//    rt_snprintf(URI, ONENET_CON_URI_LEN, "http://api.heclouds.com/devices/%s/datapoints?type=3", onenet_info.device_id);
//
//    session = webclient_session_create(ONENET_HTTP_HEAD_LEN);
//    if (session == RT_NULL)
//    {
//        result = -RT_ERROR;
//        goto __exit;
//    }
//
//    WEBCLIENT_HEADER_ADD(session, "api-key: %s\r\n", onenet_info.api_key);
//    WEBCLIENT_HEADER_ADD(session, "Content-Length: %d\r\n", strlen(buffer));
//    WEBCLIENT_HEADER_ADD(session, "Content-Type: application/octet-stream\r\n");
//
//    if (webclient_post(session, URI, buffer) != 200)
//    {
//        result = -RT_ERROR;
//        goto __exit;
//    }
//
//    LOG_D("buffer : %.*s", strlen(buffer), buffer);
//
//    __exit: if (session)
//    {
//        webclient_close(session);
//    }
//    if (URI)
//    {
//        ONENET_FREE(URI);
//    }
//
//    return result;
//}
//
//static rt_err_t onenet_get_string_data(const char *ds_name, const char *str, char **out_buff)
//{
//    rt_err_t result = RT_EOK;
//    cJSON *root = RT_NULL;
//
//    RT_ASSERT(ds_name);
//    RT_ASSERT(str);
//    RT_ASSERT(out_buff);
//
//    root = cJSON_CreateObject();
//    if (!root)
//    {
//        log_e("onenet publish string data failed! cJSON create object error return NULL!");
//        return -RT_ENOMEM;
//    }
//
//    cJSON_AddStringToObject(root, ds_name, str);
//
//    /* render a cJSON structure to buffer */
//    *out_buff = cJSON_PrintUnformatted(root);
//    if (!(*out_buff))
//    {
//        log_e("onenet publish string data failed! cJSON print unformatted error return NULL!");
//        result = -RT_ENOMEM;
//        goto __exit;
//    }
//
//    __exit: if (root)
//    {
//        cJSON_Delete(root);
//    }
//
//    return result;
//}
//
//static rt_err_t onenet_get_digit_data(const char *ds_name, const double digit, char **out_buff)
//{
//    rt_err_t result = RT_EOK;
//    cJSON *root = RT_NULL;
//
//    RT_ASSERT(ds_name);
//    RT_ASSERT(out_buff);
//
//    root = cJSON_CreateObject();
//    if (!root)
//    {
//        log_e("onenet publish digit data failed! cJSON create object error return NULL!");
//        return -RT_ENOMEM;
//    }
//
//    cJSON_AddNumberToObject(root, ds_name, digit);
//
//    /* render a cJSON structure to buffer */
//    *out_buff = cJSON_PrintUnformatted(root);
//    if (!(*out_buff))
//    {
//        log_e("onenet publish digit data failed! cJSON print unformatted error return NULL!");
//        result = -RT_ENOMEM;
//        goto __exit;
//    }
//
//    __exit: if (root)
//    {
//        cJSON_Delete(root);
//    }
//
//    return result;
//}
//
///**
// * upload digit data to OneNET cloud.
// *
// * @param   ds_name     datastream name
// * @param   digit       digit data
// *
// * @return  0 : upload data success
// *         -5 : no memory
// */
//rt_err_t onenet_http_upload_digit(const char *ds_name, const double digit)
//{
//    char *send_buffer = RT_NULL;
//    rt_err_t result = RT_EOK;
//
//    RT_ASSERT(ds_name);
//
//    /* get JSON format data */
//    result = onenet_get_digit_data(ds_name, digit, &send_buffer);
//    if (result < 0)
//    {
//        goto __exit;
//    }
//
//    /* send data to cloud by HTTP */
//    result = onenet_upload_data(send_buffer);
//    if (result < 0)
//    {
//        goto __exit;
//    }
//
//    __exit: if (send_buffer)
//    {
//        cJSON_free(send_buffer);
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
// * @return  0 : upload data success
// *         -5 : no memory
// */
//rt_err_t onenet_http_upload_string(const char *ds_name, const char *str)
//{
//    char *send_buffer = RT_NULL;
//    rt_err_t result = RT_EOK;
//
//    RT_ASSERT(ds_name);
//    RT_ASSERT(str);
//
//    /* get JSON format data */
//    result = onenet_get_string_data(ds_name, str, &send_buffer);
//    if (result < 0)
//    {
//        goto __exit;
//    }
//
//    /* send data to cloud by HTTP */
//    result = onenet_upload_data(send_buffer);
//    if (result < 0)
//    {
//        goto __exit;
//    }
//
//    __exit: if (send_buffer)
//    {
//        cJSON_free(send_buffer);
//    }
//
//    return result;
//}
