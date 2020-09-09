/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-08-05     Shine       the first version,Easyflash is used for parameter reading
 *
 */

#include <rtthread.h>
#include <stdlib.h>
#include <string.h>

#include <easyflash.h>

#include <onenet.h>
#define DBG_TAG "port.onenet"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

//默认注册成功字符串
const char registed_default_data[ONENET_INFO_REGED_LEN] = "true";
/**
 *  设备注册成功后，用于登录参数的保存
 * @param group     参数分组ID
 * @param dev_id    平台返回给设备的ID，即MQTT协议登录用的Client ID
 * @param api_key   用于通过HTTP方式传输数据时，添加到header中的参数，提高API访问安全性，本项目采用MQTT方式进行数据传输，此变量未用到
 * @return          成功返回RT_EOK
 */
rt_err_t onenet_port_save_device_info(int group, char *dev_id, char *api_key)
{
    LOG_I("onenet_port_save_device_info:group=>%d,dev_id=>%s,api_key=>%s", group, dev_id, api_key);

    size_t set_size = 0;

    char *str_group = rt_malloc(ONENET_GROUP_NAME_LEN);
    char *str_param = rt_malloc(ONENET_PARAM_NAME_LEN);
    itoa(group, str_group, 10);

    strncpy(str_param, "dev_id", ONENET_PARAM_NAME_LEN - ONENET_GROUP_NAME_LEN);
    strncat(str_param, str_group, ONENET_PARAM_NAME_LEN);
    set_size = strlen(dev_id) > ONENET_INFO_DEVID_LEN ? ONENET_INFO_DEVID_LEN : strlen(dev_id);     //限制写入长度
    ef_set_env_blob(str_param, dev_id, set_size);

    strncpy(str_param, "api_key", ONENET_PARAM_NAME_LEN - ONENET_GROUP_NAME_LEN);
    strncat(str_param, str_group, ONENET_PARAM_NAME_LEN);
    set_size = strlen(api_key) > ONENET_INFO_APIKEY_LEN ? ONENET_INFO_APIKEY_LEN : strlen(api_key); //限制写入长度
    ef_set_env_blob(str_param, api_key, set_size);

    strncpy(str_param, "is_registed", ONENET_PARAM_NAME_LEN - ONENET_GROUP_NAME_LEN);
    strncat(str_param, str_group, ONENET_PARAM_NAME_LEN);
    set_size = strlen(registed_default_data) > ONENET_INFO_REGED_LEN ? ONENET_INFO_REGED_LEN : strlen(registed_default_data); //限制写入长度
    ef_set_env_blob(str_param, registed_default_data, set_size);

    rt_free(str_group);
    rt_free(str_param);

    return RT_EOK;
}

/* Get device name and auth info for register. */
/**
 *
 *  用于设备注册时，读取需要的参数
 * @param group                 参数分组ID
 * @param dev_name              用于平台显示名称的存放
 * @param auth_info             MQTT协议登录用的password，OneNET平台要求同一产品内auth_info不重复，建议使用SN码，这里拟采用4G模块的IMEI码
 * @param regist_url            注册时产品的URL
 * @param regist_code           注册时产品的注册码
 * @param regist_master_key     注册时产品的master_key
 * @return                      成功返回RT_EOK
 */
rt_err_t onenet_port_get_register_info(int group, char *dev_name, char *auth_info, char *regist_url, char *regist_code, char *regist_master_key)
{
    size_t get_size = 0;

    char *str_group = rt_malloc(ONENET_GROUP_NAME_LEN);
    char *str_param = rt_malloc(ONENET_PARAM_NAME_LEN);
    itoa(group, str_group, 10);

    strncpy(str_param, "dev_name", ONENET_PARAM_NAME_LEN - ONENET_GROUP_NAME_LEN);
    strncat(str_param, str_group, ONENET_PARAM_NAME_LEN);
    get_size = ef_get_env_blob(str_param, dev_name, ONENET_INFO_NAME_LEN, RT_NULL);
    dev_name[get_size] = '\0';

    strncpy(str_param, "sn", ONENET_PARAM_NAME_LEN - ONENET_GROUP_NAME_LEN);
    strncat(str_param, str_group, ONENET_PARAM_NAME_LEN);
    get_size = ef_get_env_blob(str_param, auth_info, ONENET_INFO_AUTH_LEN, RT_NULL);
    auth_info[get_size] = '\0';

    strncpy(str_param, "regist_url", ONENET_PARAM_NAME_LEN - ONENET_GROUP_NAME_LEN);
    strncat(str_param, str_group, ONENET_PARAM_NAME_LEN);
    get_size = ef_get_env_blob(str_param, regist_url, ONENET_INFO_REG_URL_LEN, RT_NULL);
    regist_url[get_size] = '\0';

    strncpy(str_param, "regist_code", ONENET_PARAM_NAME_LEN - ONENET_GROUP_NAME_LEN);
    strncat(str_param, str_group, ONENET_PARAM_NAME_LEN);
    get_size = ef_get_env_blob(str_param, regist_code, ONENET_INFO_REG_CODE_LEN, RT_NULL);
    regist_code[get_size] = '\0';

    strncpy(str_param, "regist_mkey", ONENET_PARAM_NAME_LEN - ONENET_GROUP_NAME_LEN);
    strncat(str_param, str_group, ONENET_PARAM_NAME_LEN);
    get_size = ef_get_env_blob(str_param, regist_master_key, ONENET_INFO_REG_MKEY_LEN, RT_NULL);
    regist_master_key[get_size] = '\0';

    rt_free(str_group);
    rt_free(str_param);

    LOG_I("onenet_port_get_register_info:group=>%d,dev_name=>%s,sn=>%s,regist_url=>%s,regist_code=>%s,regist_mkey=>%s", group, dev_name, auth_info, regist_url, regist_code,
          regist_master_key);
    return RT_EOK;
}

/**
 * 用于登录时，读取需要的参数
 * @param group                 参数分组ID
 * @param pro_id                MQTT产品ID
 * @param dev_id                MQTT设备ID
 * @param auth_info             MQTT登录密码
 * @param server_url            MQTT服务器ip地址
 * @param api_key               HTTP请求时需要用到api_key
 * @return                      成功返回RT_EOK
 */
rt_err_t onenet_port_get_device_info(int group, char *pro_id, char *dev_id, char *auth_info, char* server_url, char *api_key)
{
    size_t get_size = 0;

    char *server_port = rt_malloc(ONENET_SERVER_PORT_LEN);
    char *str_group = rt_malloc(ONENET_GROUP_NAME_LEN);
    char *str_param = rt_malloc(ONENET_PARAM_NAME_LEN);
    itoa(group, str_group, 10);

    strncpy(str_param, "pro_id", ONENET_PARAM_NAME_LEN - ONENET_GROUP_NAME_LEN);
    strncat(str_param, str_group, ONENET_PARAM_NAME_LEN);
    get_size = ef_get_env_blob(str_param, pro_id, ONENET_INFO_PROID_LEN, RT_NULL);
    pro_id[get_size] = '\0';

    strncpy(str_param, "dev_id", ONENET_PARAM_NAME_LEN - ONENET_GROUP_NAME_LEN);
    strncat(str_param, str_group, ONENET_PARAM_NAME_LEN);
    get_size = ef_get_env_blob(str_param, dev_id, ONENET_INFO_DEVID_LEN, RT_NULL);
    dev_id[get_size] = '\0';

    strncpy(str_param, "sn", ONENET_PARAM_NAME_LEN - ONENET_GROUP_NAME_LEN);
    strncat(str_param, str_group, ONENET_PARAM_NAME_LEN);
    get_size = ef_get_env_blob(str_param, auth_info, ONENET_INFO_AUTH_LEN, RT_NULL);
    auth_info[get_size] = '\0';

    strncpy(str_param, "server_url", ONENET_PARAM_NAME_LEN - ONENET_GROUP_NAME_LEN);
    strncat(str_param, str_group, ONENET_PARAM_NAME_LEN);
    get_size = ef_get_env_blob(str_param, server_url, ONENET_SERVER_URL_LEN, RT_NULL);
    server_url[get_size] = '\0';

    strncpy(str_param, "server_port", ONENET_PARAM_NAME_LEN - ONENET_GROUP_NAME_LEN);
    strncat(str_param, str_group, ONENET_PARAM_NAME_LEN);
    get_size = ef_get_env_blob(str_param, server_port, ONENET_SERVER_PORT_LEN, RT_NULL);
    server_port[get_size] = '\0';
    strncat(server_url, ":", ONENET_SERVER_URL_LEN);             //中间加冒号
    strncat(server_url, server_port, ONENET_SERVER_URL_LEN);     //将url和端口号组合

    strncpy(str_param, "api_key", ONENET_PARAM_NAME_LEN - ONENET_GROUP_NAME_LEN);
    strncat(str_param, str_group, ONENET_PARAM_NAME_LEN);
    get_size = ef_get_env_blob(str_param, api_key, ONENET_INFO_APIKEY_LEN, RT_NULL);
    api_key[get_size] = '\0';

    rt_free(server_port);
    rt_free(str_group);
    rt_free(str_param);

    LOG_I("onenet_port_get_device_info:group=>%d,pro_id=>%s,dev_id=>%s,sn=>%s,server_url=>%s,api_key=>%s", group, pro_id, dev_id, auth_info, server_url, api_key);
    return RT_EOK;
}

/**
 * 检查该组连接是否被注册
 * @param group                 参数分组ID
 * @return                      注册成功过，返回RT_EOK
 */
rt_bool_t onenet_port_is_registed(int group)
{
    rt_bool_t ret = RT_FALSE;
    size_t get_size = 0;

    char *is_registed = rt_malloc(ONENET_INFO_REGED_LEN);

    char *str_group = rt_malloc(ONENET_GROUP_NAME_LEN);
    char *str_param = rt_malloc(ONENET_PARAM_NAME_LEN);
    itoa(group, str_group, 10);

    strncpy(str_param, "is_registed", ONENET_PARAM_NAME_LEN - ONENET_GROUP_NAME_LEN);
    strncat(str_param, str_group, ONENET_PARAM_NAME_LEN);
    get_size = ef_get_env_blob(str_param, is_registed, ONENET_INFO_REGED_LEN, RT_NULL);
    is_registed[get_size] = '\0';

    if (strcmp(is_registed, registed_default_data) == 0)
        ret = RT_TRUE;

    rt_free(is_registed);
    rt_free(str_group);
    rt_free(str_param);

    return ret;
}
