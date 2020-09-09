# OneNET

## 1、介绍
[OneNET](https://open.iot.10086.cn/) 平台是中国移动基于物联网产业打造的生态平台，具有高并发可用、多协议接入、丰富 API 支持、数据安全存储、快速应用孵化等特点，同时，OneNET 平台还提供全方位支撑，加速用户产品的开发速度。


OneNET 软件包是 RT-Thread 针对 OneNET 平台连接做的的适配，通过这个软件包，可以让设备在 RT-Thread 上非常方便的连接 OneNet 平台，完成数据的发送、接收、设备的注册和控制等功能。

目前很多公司物联网服务都采用了OneNET的协议，针对某些项目需要同时对多个MQTT服务器进行数据传输的需求，在RT-Thread OneNET包的基础上进行修改，支持多服务器数据进行传输。

更多介绍请查看RT-Thread OneNET包的介绍。

### 1.1 目录结构

略

### 1.2 许可证

OneNET package  遵循 GUN GPL 许可，详见 `LICENSE` 文件。

### 1.3 依赖

- RT_Thread 3.0+
- [paho-mqtt](https://github.com/RT-Thread-packages/paho-mqtt.git)
- [webclient](https://github.com/RT-Thread-packages/webclient.git)
- [cJSON](https://github.com/RT-Thread-packages/cJSON.git)



