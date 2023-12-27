# 轻量级Web服务器

Linux环境下基于C++语言实现的轻量级web服务器：

- 使用 **线程池 + 非阻塞socket +I/O多路复用 epoll(ET和LT均实现) + 事件处理(Reactor和模拟Proactor均实现)** 的并发模型

- 使用**有限状态机**解析HTTP请求报文，支持解析**GET和POST**请求

- 访问服务器数据库实现web端用户**注册、登录**功能，可以请求服务器**图片和视频文件**

- 实现**同步/异步日志系统**，记录服务器运行状态

- 为节省连接资源，使用 **信号量统一事件管理+升序双向链表** 定期检测并删除超时HTTP连接

- 经Webbench压力测试可以实现**上万的并发连接**数据交换

  

# 项目结构

```c++
|--MyWebServer // 项目根目录
  |--commend   // 工具函数如信号量注册，文件描述符非阻塞设置，添加删除epoll事件等...
  |--http	   // 管理http连接资源
  |--lock      // 线程同步封装类，封装互斥锁，信号量，条件变量三种常用同步资源
  |--log       // 包括日志管理资源，以及用于异步写的阻塞队列
  |--pool      // 包括线程池和数据库连接池两类资源
  |--root	   // 文件资源根目录，包括http请求所需的html文件，图片和视频
  |--timer     // 定时器资源，管理每条连接的超时时间
  |--webserver // 将上述资源进行整合，进行事件监听和响应
|--config      // 参数配置
|--main		   // 主函数
```

# 项目整体框架

<img src=".\root\frame.jpg" style="zoom: 25%;" />

# 测试结果

## Reactor模式
<img src=".\root\Reactor.jpg" style="zoom: 100%;" />

## Proactor模式

<img src=".\root\Proactor.jpg" style="zoom: 100%;" />