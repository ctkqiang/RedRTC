# RedRTC - WebRTC 信令服务器

## 概述

RedRTC 是一个基于 C 语言开发的高性能、内存高效的 WebRTC 信令服务器。它为实时通信应用提供可靠的信令服务，每个房间最多支持 6 个参与者。

## 功能特性

### 核心功能
- **WebRTC 信令**：完整的信令解决方案，支持 offer/answer 交换和 ICE 候选协商
- **房间管理**：支持多个并发房间，每个房间最多 6 个参与者
- **高性能**：优化的 C 语言实现，内存占用最小
- **内存高效**：固定大小分配和内存池，性能可预测
- **跨平台**：兼容 Linux、macOS 和其他类 Unix 系统

### 技术特性
- **可扩展架构**：支持数百个并发客户端和房间
- **健壮的连接处理**：自动重连和超时管理
- **生产就绪**：全面的错误处理和资源管理
- **标准兼容**：实现标准 WebRTC 信令协议

## 系统要求

### 最低要求
- Linux 内核 3.2+ 或 macOS 10.12+
- 512MB 内存
- 100MB 磁盘空间
- 支持 IPv4 的网络接口

### 依赖项
- libwebsockets 4.0+
- jansson 2.7+
- OpenSSL 1.1+

## 安装指南

### 先决条件

#### macOS
```bash
# 使用 Homebrew 安装依赖
brew install openssl libwebsockets jansson
```

#### Ubuntu/Debian
```bash
# 安装依赖
sudo apt-get update
sudo apt-get install libwebsockets-dev libjansson-dev libssl-dev build-essential
```

#### CentOS/RHEL
```bash
# 安装依赖
sudo yum install libwebsockets-devel jansson-devel openssl-devel gcc make
```

### 从源码构建

1. **克隆仓库**
```bash
git clone https://github.com/ctkqiang/redrtc.git
cd redrtc
```

2. **构建服务器**
```bash
make release
```

3. **系统级安装**（可选）
```bash
sudo make install
```

### 验证安装

通过检查服务器版本来验证安装：
```bash
./build/bin/redrtc --version
```

## 使用方法

### 基本操作

#### 启动服务器
```bash
# 使用默认配置启动（端口 8080）
./build/bin/redrtc

# 使用自定义端口启动
./build/bin/redrtc --port 9000

# 启用详细日志启动
./build/bin/redrtc --verbose
```

#### 守护进程模式
```bash
# 以守护进程模式运行，适用于生产环境
./build/bin/redrtc --daemon
```

### 命令行选项

| 选项 | 简写 | 默认值 | 描述 |
|------|------|--------|------|
| `--port` | `-p` | 8080 | 服务器端口号 |
| `--interface` | `-i` | all | 绑定的网络接口 |
| `--clients` | `-c` | 1024 | 最大并发客户端数 |
| `--rooms` | `-r` | 256 | 最大活跃房间数 |
| `--timeout` | `-t` | 300 | 客户端超时时间（秒） |
| `--daemon` | `-d` | false | 以守护进程运行 |
| `--verbose` | `-v` | false | 启用详细日志 |
| `--help` | `-h` | - | 显示帮助信息 |

### 配置示例

#### 开发环境配置
```bash
./build/bin/redrtc --port 8080 --clients 100 --rooms 50 --verbose
```

#### 生产环境配置
```bash
./build/bin/redrtc --port 443 --clients 2048 --rooms 512 --daemon
```

#### 自定义接口绑定
```bash
./build/bin/redrtc --interface 192.168.1.100 --port 8080
```

## WebRTC 信令协议

### 消息格式

所有消息都是 JSON 对象，结构如下：
```json
{
  "event": "事件类型",
  "data": { ... }
}
```

### 支持的事件

| 事件 | 方向 | 描述 |
|------|------|------|
| `client-id` | 服务器 → 客户端 | 分配唯一客户端标识符 |
| `join-room` | 客户端 → 服务器 | 加入或创建房间 |
| `leave-room` | 客户端 → 服务器 | 离开当前房间 |
| `offer` | 客户端 → 服务器 → 客户端 | WebRTC 会话描述 offer |
| `answer` | 客户端 → 服务器 → 客户端 | WebRTC 会话描述 answer |
| `ice-candidate` | 客户端 → 服务器 → 客户端 | ICE 候选交换 |
| `participants` | 服务器 → 客户端 | 房间参与者列表更新 |
| `error` | 服务器 → 客户端 | 错误通知 |

### 客户端集成示例

#### JavaScript 客户端
```javascript
class RedRTCClient {
    constructor(serverUrl) {
        this.ws = new WebSocket(serverUrl);
        this.clientId = null;
        this.room = null;
        
        this.ws.onmessage = (event) => {
            const message = JSON.parse(event.data);
            this.handleMessage(message);
        };
    }

    handleMessage(message) {
        switch (message.event) {
            case 'client-id':
                this.clientId = message.data.clientId;
                console.log('连接成功，客户端ID:', this.clientId);
                break;
                
            case 'participants':
                console.log('房间参与者:', message.data.participants);
                break;
                
            case 'offer':
                this.handleOffer(message.data);
                break;
                
            case 'answer':
                this.handleAnswer(message.data);
                break;
                
            case 'ice-candidate':
                this.handleIceCandidate(message.data);
                break;
        }
    }

    joinRoom(roomId, roomName) {
        const message = {
            event: 'join-room',
            data: {
                roomId: roomId,
                roomName: roomName
            }
        };
        this.ws.send(JSON.stringify(message));
    }

    sendOffer(targetClientId, offer) {
        const message = {
            event: 'offer',
            data: {
                targetClientId: targetClientId,
                offer: offer
            }
        };
        this.ws.send(JSON.stringify(message));
    }
}
```

## 系统架构

### 系统组件

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   WebSocket     │    │   客户端         │    │   房间          │
│   连接管理器    │◄──►│   注册表         │◄──►│   注册表        │
│                 │    │                  │    │                 │
└─────────────────┘    └──────────────────┘    └─────────────────┘
         │                       │                       │
         │                       │                       │
         ▼                       ▼                       ▼
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   消息          │    │   连接池         │    │   参与者        │
│   队列          │    │                  │    │   管理          │
└─────────────────┘    └──────────────────┘    └─────────────────┘
```

### 数据流程

1. **客户端连接**
   - 建立 WebSocket 握手
   - 分配唯一客户端 ID
   - 客户端添加到注册表

2. **房间管理**
   - 客户端请求加入/创建房间
   - 创建房间或加入现有房间
   - 向所有房间成员广播参与者列表

3. **信令处理**
   - 在客户端间中继 WebRTC offer/answer
   - 通过服务器交换 ICE 候选
   - 处理无效状态的错误处理

## 性能特征

### 资源使用
- **内存**：约 2MB 基础 + 每个连接客户端 4KB
- **CPU**：单线程事件循环，开销最小
- **网络**：高效的二进制 WebSocket 协议

### 扩展限制
| 资源 | 推荐限制 | 硬限制 |
|------|----------|--------|
| 并发客户端 | 1,024 | 65,536 |
| 活跃房间 | 256 | 10,000 |
| 每个房间参与者 | 6 | 6 |
| 每秒消息数 | 10,000 | 受网络限制 |

## 安全考虑

### 网络安全
- 在反向代理处使用 TLS/SSL 终止
- 为客户端连接实施速率限制
- 验证所有传入消息格式

### 应用安全
- 清理所有客户端提供的数据
- 实施适当的会话超时
- 定期更新依赖项的安全补丁

### 部署安全
```bash
# 使用非特权用户运行
sudo useradd -r -s /bin/false redrtc
sudo chown redrtc:redrtc /usr/local/bin/redrtc
```

## 监控和日志

### 日志级别
- **Error**：严重故障和系统错误
- **Warning**：非关键问题和警告
- **Info**：正常操作消息
- **Debug**：详细调试信息

### 健康监控
```bash
# 检查服务器状态
ps aux | grep redrtc

# 监控连接
netstat -an | grep 8080

# 检查系统资源
top -p $(pgrep redrtc)
```

## 故障排除

### 常见问题

#### 端口已被占用
```bash
# 检查端口使用情况
sudo lsof -i :8080

# 终止冲突进程
sudo kill -9 <PID>
```

#### 权限被拒绝
```bash
# 检查文件权限
ls -la build/bin/redrtc

# 如有需要，修复权限
chmod +x build/bin/redrtc
```

#### 库加载问题
```bash
# 在 macOS 上检查库路径
otool -L build/bin/redrtc

# 在 Linux 上检查库路径
ldd build/bin/redrtc
```

### 调试模式
```bash
# 使用调试符号构建
make debug

# 使用详细日志运行
./build/bin/redrtc --verbose
```

## 开发指南

### 从源码构建
```bash
# 克隆仓库
git clone https://github.com/ctkqiang/redrtc.git
cd redrtc

# 构建调试版本
make debug

# 运行测试
make test
```

### 代码结构
```
redrtc/
├── include/              # 头文件
│   ├── server.h         # 服务器核心功能
│   ├── client.h         # 客户端管理
│   ├── room.h           # 房间管理
│   ├── messages.h       # 消息处理
│   └── utils.h          # 工具函数
├── src/                 # 源文件
│   ├── server.c         # 服务器实现
│   ├── client.c         # 客户端管理
│   ├── room.c           # 房间操作
│   ├── messages.c       # 消息处理
│   └── utils.c          # 工具函数
├── test/               # 测试套件
└── examples/           # 使用示例
```

### 贡献指南
1. Fork 代码仓库
2. 创建特性分支
3. 实现更改并包含测试
4. 提交拉取请求

## 许可证

本项目采用 **木兰宽松许可证 (Mulan PSL)** 进行许可。    

[![License: Mulan PSL v2](https://img.shields.io/badge/License-Mulan%20PSL%202-blue.svg)](http://license.coscl.org.cn/MulanPSL2)


### 🌐 全球捐赠通道

#### 国内用户

<div align="center" style="margin: 40px 0">

<div align="center">
<table>
<tr>
<td align="center" width="300">
<img src="https://github.com/ctkqiang/ctkqiang/blob/main/assets/IMG_9863.jpg?raw=true" width="200" />
<br />
<strong>🔵 支付宝</strong>（小企鹅在收金币哟~）
</td>
<td align="center" width="300">
<img src="https://github.com/ctkqiang/ctkqiang/blob/main/assets/IMG_9859.JPG?raw=true" width="200" />
<br />
<strong>🟢 微信支付</strong>（小绿龙在收金币哟~）
</td>
</tr>
</table>
</div>
</div>

#### 国际用户

<div align="center" style="margin: 40px 0">
  <a href="https://qr.alipay.com/fkx19369scgxdrkv8mxso92" target="_blank">
    <img src="https://img.shields.io/badge/Alipay-全球支付-00A1E9?style=flat-square&logo=alipay&logoColor=white&labelColor=008CD7">
  </a>
  
  <a href="https://ko-fi.com/F1F5VCZJU" target="_blank">
    <img src="https://img.shields.io/badge/Ko--fi-买杯咖啡-FF5E5B?style=flat-square&logo=ko-fi&logoColor=white">
  </a>
  
  <a href="https://www.paypal.com/paypalme/ctkqiang" target="_blank">
    <img src="https://img.shields.io/badge/PayPal-安全支付-00457C?style=flat-square&logo=paypal&logoColor=white">
  </a>
  
  <a href="https://donate.stripe.com/00gg2nefu6TK1LqeUY" target="_blank">
    <img src="https://img.shields.io/badge/Stripe-企业级支付-626CD9?style=flat-square&logo=stripe&logoColor=white">
  </a>
</div>

---

### 📌 开发者社交图谱

#### 技术交流

<div align="center" style="margin: 20px 0">
  <a href="https://github.com/ctkqiang" target="_blank">
    <img src="https://img.shields.io/badge/GitHub-开源仓库-181717?style=for-the-badge&logo=github">
  </a>
  
  <a href="https://stackoverflow.com/users/10758321/%e9%92%9f%e6%99%ba%e5%bc%ba" target="_blank">
    <img src="https://img.shields.io/badge/Stack_Overflow-技术问答-F58025?style=for-the-badge&logo=stackoverflow">
  </a>
  
  <a href="https://www.linkedin.com/in/ctkqiang/" target="_blank">
    <img src="https://img.shields.io/badge/LinkedIn-职业网络-0A66C2?style=for-the-badge&logo=linkedin">
  </a>
</div>

#### 社交互动

<div align="center" style="margin: 20px 0">
  <a href="https://www.instagram.com/ctkqiang" target="_blank">
    <img src="https://img.shields.io/badge/Instagram-生活瞬间-E4405F?style=for-the-badge&logo=instagram">
  </a>
  
  <a href="https://twitch.tv/ctkqiang" target="_blank">
    <img src="https://img.shields.io/badge/Twitch-技术直播-9146FF?style=for-the-badge&logo=twitch">
  </a>
  
  <a href="https://github.com/ctkqiang/ctkqiang/blob/main/assets/IMG_9245.JPG?raw=true" target="_blank">
    <img src="https://img.shields.io/badge/微信公众号-钟智强-07C160?style=for-the-badge&logo=wechat">
  </a>
</div>

---

致极客与未来的你

> "世界由代码驱动，安全靠你我守护。"
