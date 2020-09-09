/*
 * File      : onenet.h
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
 */
#ifndef _ONENET_H_
#define _ONENET_H_

#include <rtthread.h>
#include <paho_mqtt.h>
#include <cJSON.h>

#define ONENET_SW_VERSION              "1.0.0"



#define ONENET_CLIENT_MAX               3                                   //最大允许的client数量

#define ONENET_PARAM_NAME_LEN           32                                  //easyflash存储参数名称的最大长度
#define ONENET_GROUP_NAME_LEN           (2 + 1)                             //对于多组参数，参数名长度，不超过2位数

#define ONENET_INFO_NAME_LEN            64                                  //ONENET设备名称长度,仅注册用
#define ONENET_INFO_AUTH_LEN            64                                  //ONENET鉴权信息长度

#define ONENET_INFO_REG_URL_LEN         128                                 //ONENET注册URL长度
#define ONENET_INFO_REG_CODE_LEN        32                                  //ONENET注册码长度
#define ONENET_INFO_REG_MKEY_LEN        64                                  //ONENET注册master key长度
#define ONENET_INFO_REGED_LEN           8                                   //ONENET本地注册成功记录长度

#define ONENET_SERVER_URL_LEN           32                                  //ONENET登录URL
#define ONENET_SERVER_PORT_LEN          8                                   //ONENET登录端口号

#define ONENET_INFO_PROID_LEN           16                                  //ONENET登录产品ID长度
#define ONENET_INFO_DEVID_LEN           16                                  //ONENET登录设备ID长度
#define ONENET_INFO_APIKEY_LEN          64                                  //ONENET HTTP传输API_KEY长度

#define ONENET_DATASTREAM_NAME_MAX      32


/**
 * 定义存储OneNET连接的信息
 * 长度+1保证字符串结束符不越界
 */
struct rt_onenet_info
{
    char device_id[ONENET_INFO_DEVID_LEN+1];
    char pro_id[ONENET_INFO_PROID_LEN+1];
    char auth_info[ONENET_INFO_AUTH_LEN+1];
    char api_key[ONENET_INFO_APIKEY_LEN+1];
    char server_uri[ONENET_SERVER_URL_LEN+ONENET_SERVER_PORT_LEN+1];
};
typedef struct rt_onenet_info *rt_onenet_info_t;

/* onenet datastream info */
struct rt_onenet_ds_info
{
    char id[ONENET_DATASTREAM_NAME_MAX];
    char tags[ONENET_DATASTREAM_NAME_MAX];

    char update_time[ONENET_DATASTREAM_NAME_MAX];
    char create_time[ONENET_DATASTREAM_NAME_MAX];

    char unit[ONENET_DATASTREAM_NAME_MAX];
    char unit_symbol[ONENET_DATASTREAM_NAME_MAX];

    char current_value[ONENET_DATASTREAM_NAME_MAX];

};
typedef struct rt_onenet_ds_info *rt_onenet_ds_info_t;


/* OneNET MQTT initialize. */
rt_err_t onenet_mqtt_init(int group);
/* Publish MQTT data to subscribe topic. */
rt_err_t onenet_mqtt_publish(int group, const char *topic, const uint8_t *msg, size_t len);
/* Publish MQTT digit data to onenet. */
rt_err_t onenet_mqtt_upload_digit(int group, const char *ds_name, const double digit);

int find_group_by_mqclient(MQTTClient *client);


/* Publish MQTT binary data to onenet. */
rt_err_t onenet_mqtt_upload_bin(const char *ds_name, uint8_t *bin, size_t len);

/* Publish MQTT string data to onenet. */
rt_err_t onenet_mqtt_upload_string(const char *ds_name, const char *str);


/* Device send data to OneNET cloud. */
rt_err_t onenet_http_upload_digit(const char *ds_name, const double digit);
rt_err_t onenet_http_upload_string(const char *ds_name, const char *str);

/* Register a device to OneNET cloud. */
rt_err_t onenet_http_register_device(int group, const char *name, const char *auth_info, const char *url, const char *code, const char *mkey);
/* get a datastream from OneNET cloud. */
rt_err_t onenet_http_get_datastream(const char *ds_name, struct rt_onenet_ds_info *datastream);
/* get datapoints from OneNET cloud. Returned cJSON need to be free when user finished using the data. */
cJSON *onenet_get_dp_by_limit(char *ds_name, size_t limit);
cJSON *onenet_get_dp_by_start_end(char *ds_name, uint32_t start, uint32_t end, size_t limit);
cJSON *onenet_get_dp_by_start_duration(char *ds_name, uint32_t start, size_t duration, size_t limit);
/* Set the command response callback function. User needs to malloc memory for response data. */









/* ========================== User port function ============================ */
/* Check the device has been registered or not. */
rt_bool_t onenet_port_is_registed(int group);
/* Get device info for register. */
rt_err_t onenet_port_get_register_info(int group, char *dev_name, char *auth_info, char *regist_url, char *regist_code, char *regist_master_key);
/* Save device info. */
rt_err_t onenet_port_save_device_info(int group, char *dev_id, char *api_key);
/* Get device info. */
rt_err_t onenet_port_get_device_info(int group, char *pro_id, char *dev_id, char *auth_info, char* server_url, char *api_key);

#endif /* _ONENET_H_ */
