/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-08-05     Shine       the first version
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <drv_common.h>

#include <stdlib.h>

#include <onenet.h>
#include <finsh.h>

#define DBG_TAG "app.onenet"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#include <easyflash.h>

int mqtt_connect_status = 0;

#include <arpa/inet.h>
#include <netdev.h>

#include <paho_mqtt.h>
/* upload random value to temperature*/
static void onenet_upload_entry(void *parameter)
{
    int value = 0;

    while (1)
    {
        value = rand() % 100;

        if (onenet_mqtt_upload_digit(1, "temperature", value) < 0)
        {
            log_e("upload has an error, stop uploading");
            break;
        }
        else
        {
            //log_d("buffer : {\"temperature\":%d}", value);
        }

        rt_thread_delay(rt_tick_from_millisecond(5 * 1000));
    }
}

int onenet_upload_cycle(void)
{
    rt_thread_t tid;
    tid = rt_thread_create("onenet_send", onenet_upload_entry, RT_NULL, 2 * 1024, RT_THREAD_PRIORITY_MAX / 3 - 1, 5);
    if (tid)
    {
        rt_thread_startup(tid);
    }

    return 0;
}
MSH_CMD_EXPORT(onenet_upload_cycle, send data to OneNET cloud cycle);


/**
 * OneNET自动连接线程，当网络连接正常时，开始执行mqtt初始化
 * @param parameter
 */
static void onenet_start_thread_entry(void *parameter)
{
    struct netdev *dev = RT_NULL;
    while (1)
    {
        dev = netdev_get_first_by_flags(NETDEV_FLAG_INTERNET_UP);
        if (dev == RT_NULL)
        {
            LOG_D("wait netdev internet up...");
            rt_thread_mdelay(1000);
        }
        else
        {
            LOG_I("local ip is:%d.%d.%d.%d", (((dev->ip_addr.addr) >> 0) & 0xFF), (((dev->ip_addr.addr) >> 8) & 0xFF), (((dev->ip_addr.addr) >> 16) & 0xFF),
                  (((dev->ip_addr.addr) >> 24) & 0xFF));
            break;
        }

    }
    mqtt_connect_status = 1;
    LOG_D("onenet begin...");
    /* set the onenet mqtt command response callback function */
    //onenet_set_cmd_rsp_cb(onenet_cmd_rsp_cb);   //设置onenet下发指令回调函数
    extern rt_err_t onenet_mqtt_init(int group);
    onenet_mqtt_init(1);


}

/**
 * 创建onenet启动线程
 * 用于网络连接正常时，onenet自动连接
 * */
static int onenet_start_thread_init(void)
{
    rt_thread_t thread = rt_thread_create("one_init", onenet_start_thread_entry, RT_NULL, 8192, 25, 10);
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
    }
    return 0;
}
//INIT_APP_EXPORT(onenet_start_thread_init);

/**
 * 用于OneNET平台测试
 * @return
 */
int stp_param(void)
{
    ef_set_env_blob("dev_name1", "panpan", strlen("panpan"));
    ef_set_env_blob("sn1", "sn999", strlen("sn999"));
    ef_set_env_blob("regist_url1", "http://api.heclouds.com/register_de?register_code=", strlen("http://api.heclouds.com/register_de?register_code="));
    ef_set_env_blob("regist_code1", "SvpKWmGZzilJ3YXv", strlen("SvpKWmGZzilJ3YXv"));
    ef_set_env_blob("regist_mkey1", "jydOZXTaxHRu0g1sa3LYuVx7hrFdtJQqfK4CxtbmWiM=", strlen("jydOZXTaxHRu0g1sa3LYuVx7hrFdtJQqfK4CxtbmWiM="));

    ef_set_env_blob("is_registed1", "false", strlen("false"));
    ef_set_env_blob("server_url1", "tcp://183.230.40.39", strlen("tcp://183.230.40.39"));
    ef_set_env_blob("server_port1", "6002", strlen("6002"));
    ef_set_env_blob("pro_id1", "360534", strlen("360534"));
    return 0;
}
MSH_CMD_EXPORT(stp_param, set OneNET defaults param);

/**
 * 用于蛋蛋平台测试
 * @return
 */
int std_param(void)
{
    ef_set_env_blob("dev_name1", "dandan", strlen("dandan"));
    ef_set_env_blob("sn1", "sn888", strlen("sn888"));
    ef_set_env_blob("regist_url1", "http://swustal.zicp.net/register_de?register_code=", strlen("http://swustal.zicp.net/register_de?register_code="));
    ef_set_env_blob("regist_code1", "SvpKWmGZzilJ3YXv", strlen("SvpKWmGZzilJ3YXv"));
    ef_set_env_blob("regist_mkey1", "jydOZXTaxHRu0g1sa3LYuVx7hrFdtJQqfK4CxtbmWiM=", strlen("jydOZXTaxHRu0g1sa3LYuVx7hrFdtJQqfK4CxtbmWiM="));

    ef_set_env_blob("is_registed1", "false", strlen("false"));
    ef_set_env_blob("server_url1", "tcp://103.46.128.41", strlen("tcp://103.46.128.41"));
    ef_set_env_blob("server_port1", "55421", strlen("55421"));
    ef_set_env_blob("pro_id1", "20190156", strlen("20190156"));
    return 0;
}
MSH_CMD_EXPORT(std_param, set Local param);
