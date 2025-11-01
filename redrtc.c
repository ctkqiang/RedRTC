/**
 * REd RTC 信令服务器 - 主入口文件
 * 
 * 高性能、内存高效的 WebRTC 信令服务器
 * 每个房间最多支持 6 个参与者
 * 
 * 编译: gcc -O2 -Wall -Wextra main.c server.c client.c room.c messages.c utils.c -lwebsockets -ljansson -o red_rtc_server
 * 运行: ./red_rtc_server --port 8080 --clients 1024 --rooms 256
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <inttypes.h>


#include "./include/server.h"
#include "./include/utilities.h"


/* 全局上下文用于信号处理 */
static server_context_t *global_server_ctx = NULL;

/**
 * @brief 信号处理函数，用于优雅关闭服务器
 * @param sig 信号编号
 */
static void signal_handler(int sig) {
    if (global_server_ctx) {
        printf("\n收到信号 %d，正在优雅关闭服务器...\n", sig);
        server_stop(global_server_ctx);
    }
}

/**
 * @brief 打印使用说明
 * @param program_name 可执行文件名称
 */
static void print_usage(const char *program_name) {
    printf("REd RTC 信令服务器 - 高性能、内存高效\n");
    printf("版本: 1.0.0\n");
    printf("编译时间: %s %s\n\n", __DATE__, __TIME__);
    
    printf("用法: %s [选项]\n\n", program_name);
    printf("选项:\n");
    printf("  -p, --port 端口          服务器端口 (默认: 8080)\n");
    printf("  -i, --interface 接口     绑定的网络接口 (默认: 所有)\n");
    printf("  -c, --clients 数量       最大并发客户端数 (默认: 1024)\n");
    printf("  -r, --rooms 数量         最大活跃房间数 (默认: 256)\n");
    printf("  -t, --timeout 秒数       客户端超时时间 (默认: 300)\n");
    printf("  -d, --daemon             以守护进程模式运行\n");
    printf("  -v, --verbose            启用详细日志\n");
    printf("  -h, --help               显示此帮助信息\n");
    printf("\n示例:\n");
    printf("  %s -p 8080 -c 2048 -r 512\n", program_name);
    printf("  %s --port 9000 --interface 0.0.0.0 --timeout 600\n", program_name);
    printf("  %s --daemon --verbose --clients 512 --rooms 128\n", program_name);
}

/**
 * @brief 打印服务器横幅和构建信息
 */
static void print_banner(void) {
    printf("=================================================\n");
    printf("            REd RTC 信令服务器\n");
    printf("            高性能、内存高效\n");
    printf("            每个房间最多支持 %d 个参与者\n", MAX_PARTICIPANTS);
    printf("=================================================\n");
    printf("编译: %s %s\n", __DATE__, __TIME__);
    printf("进程ID: %d\n", getpid());
    printf("=================================================\n");
}

/**
 * @brief 打印服务器配置
 * @param config 服务器配置
 */
static void print_config(const server_config_t *config) {
    printf("服务器配置:\n");
    printf("  端口:             %d\n", config->port);
    printf("  网络接口:         %s\n", config->interface ? config->interface : "所有");
    printf("  最大客户端数:     %zu\n", config->max_clients);
    printf("  最大房间数:       %zu\n", config->max_rooms);
    printf("  客户端超时:       %u 秒\n", config->client_timeout_sec);
    printf("  详细日志:         %s\n", config->enable_stats ? "启用" : "禁用");
    printf("=================================================\n");
}

/**
 * @brief 验证配置参数
 * @param config 要验证的服务器配置
 * @return 0 表示有效，-1 表示无效
 */
static int validate_config(const server_config_t *config) {
    if (config->port < 1 || config->port > 65535) {
        fprintf(stderr, "错误: 端口必须在 1 到 65535 之间\n");
        return -1;
    }
    
    if (config->max_clients < 1 || config->max_clients > 65536) {
        fprintf(stderr, "错误: 最大客户端数必须在 1 到 65536 之间\n");
        return -1;
    }
    
    if (config->max_rooms < 1 || config->max_rooms > 10000) {
        fprintf(stderr, "错误: 最大房间数必须在 1 到 10000 之间\n");
        return -1;
    }
    
    if (config->client_timeout_sec < 30) {
        fprintf(stderr, "错误: 客户端超时时间必须至少 30 秒\n");
        return -1;
    }
    
    return 0;
}

/**
 * @brief 设置信号处理器用于优雅关闭
 */
static void setup_signal_handlers(void) {
    struct sigaction sa;
    
    /* 设置信号处理器 */
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    /* 注册信号 */
    sigaction(SIGINT, &sa, NULL);  /* Ctrl-C */
    sigaction(SIGTERM, &sa, NULL); /* kill 命令 */
    sigaction(SIGHUP, &sa, NULL);  /* 终端挂断 */
    
    /* 忽略 SIGPIPE 避免在管道破裂时终止 */
    signal(SIGPIPE, SIG_IGN);
}

/**
 * @brief 将服务器进程守护进程化（可选）
 * @return 成功返回 0，失败返回 -1
 */
static int daemonize_server(void) {
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    
    if (pid > 0) {
        /* 父进程 - 退出 */
        printf("服务器已在后台启动，进程ID: %d\n", pid);
        exit(0);
    }
    
    /* 子进程继续执行 */
    
    /* 创建新会话 */
    if (setsid() < 0) {
        perror("setsid");
        return -1;
    }
    
    /* 更改工作目录到根目录 */
    if (chdir("/") < 0) {
        perror("chdir");
        return -1;
    }
    
    /* 关闭标准文件描述符 */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    /* 将标准输入输出重定向到 /dev/null */
    fopen("/dev/null", "r"); /* 标准输入 */
    fopen("/dev/null", "w"); /* 标准输出 */
    fopen("/dev/null", "w"); /* 标准错误 */
    
    return 0;
}

/**
 * @brief 显示运行时统计信息
 * @param server 服务器上下文
 */
static void display_runtime_stats(const server_context_t *server) {
    if (!server->config.enable_stats) return;
    
    static time_t last_stats_time = 0;
    time_t now = time(NULL);
    
    /* 每30秒显示一次统计信息 */
    if (now - last_stats_time >= 30) {
        printf("[统计] 客户端: %zu/%zu, 房间: %zu/%zu, 消息: %" PRIu64 ", 错误: %" PRIu64 "\n",
               client_registry_get_active_count(&server->clients),
               server->clients.max_clients,
               room_registry_get_active_count(&server->rooms),
               server->rooms.max_rooms,
               server->total_messages,
               server->total_errors);
        last_stats_time = now;
    }
}

/**
 * @brief 主入口点
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 退出代码 (0 = 成功, 非零 = 错误)
 */
int main(int argc, char *argv[]) {
    server_config_t config = {
        .port = 8080,
        .max_clients = 1024,
        .max_rooms = 256,
        .client_timeout_sec = 300, /* 5 分钟 */
        .enable_stats = false,
        .interface = NULL
    };
    
    int daemon_mode = 0;
    int verbose_mode = 0;
    
    /* 命令行选项 */
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"interface", required_argument, 0, 'i'},
        {"clients", required_argument, 0, 'c'},
        {"rooms", required_argument, 0, 'r'},
        {"timeout", required_argument, 0, 't'},
        {"daemon", no_argument, 0, 'd'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* 解析命令行参数 */
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "p:i:c:r:t:dvh", 
                             long_options, &option_index)) != -1) {
        switch (opt) {
            case 'p':
                config.port = atoi(optarg);
                if (config.port <= 0) {
                    fprintf(stderr, "错误: 无效的端口号: %s\n", optarg);
                    return 1;
                }
                break;
                
            case 'i':
                config.interface = optarg;
                break;
                
            case 'c':
                config.max_clients = atoi(optarg);
                if (config.max_clients <= 0) {
                    fprintf(stderr, "错误: 无效的客户端数量: %s\n", optarg);
                    return 1;
                }
                break;
                
            case 'r':
                config.max_rooms = atoi(optarg);
                if (config.max_rooms <= 0) {
                    fprintf(stderr, "错误: 无效的房间数量: %s\n", optarg);
                    return 1;
                }
                break;
                
            case 't':
                config.client_timeout_sec = atoi(optarg);
                if (config.client_timeout_sec < 30) {
                    fprintf(stderr, "错误: 超时时间必须至少 30 秒\n");
                    return 1;
                }
                break;
                
            case 'd':
                daemon_mode = 1;
                break;
                
            case 'v':
                verbose_mode = 1;
                config.enable_stats = true;
                break;
                
            case 'h':
                print_usage(argv[0]);
                return 0;
                
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* 验证配置 */
    if (validate_config(&config) != 0) {
        return 1;
    }
    
    /* 如果请求了守护进程模式 */
    if (daemon_mode) {
        if (daemonize_server() != 0) {
            fprintf(stderr, "守护进程化失败\n");
            return 1;
        }
    } else {
        /* 在前台模式下打印横幅 */
        print_banner();
        print_config(&config);
    }
    
    /* 设置信号处理器 */
    setup_signal_handlers();
    
    /* 初始化服务器 */
    server_context_t server;
    int ret = server_init(&server, &config);
    if (ret != 0) {
        fprintf(stderr, "服务器初始化失败: 错误代码 %d\n", ret);
        
        if (ret == -2) {
            fprintf(stderr, "  客户端注册表初始化失败\n");
        } else if (ret == -3) {
            fprintf(stderr, "  房间注册表初始化失败\n");
        } else if (ret == -4) {
            fprintf(stderr, "  消息队列初始化失败\n");
        } else if (ret == -5) {
            fprintf(stderr, "  WebSocket 上下文创建失败\n");
            fprintf(stderr, "  请检查端口 %d 是否可用\n", config.port);
        }
        
        return 1;
    }
    
    /* 为信号处理器设置全局上下文 */
    global_server_ctx = &server;
    
    if (!daemon_mode) {
        printf("服务器正在端口 %d 上启动...\n", config.port);
        printf("按 Ctrl-C 停止服务器\n");
        printf("服务器运行中...\n");
    }
    
    /* 记录开始时间 */
    time_t start_time = time(NULL);
    time_t last_activity_check = start_time;
    
    /* 运行服务器主循环 */
    ret = server_run(&server);
    
    /* 计算运行时间 */
    time_t uptime = time(NULL) - start_time;
    
    /* 清理 */
    server_cleanup(&server);
    global_server_ctx = NULL;
    
    if (!daemon_mode) {
        printf("\n=================================================\n");
        printf("服务器关闭完成\n");
        printf("运行时间: %ld 秒\n", uptime);
        
        if (config.enable_stats) {
            printf("统计信息:\n");
            printf("  总连接数: %" PRIu64 "\n", server.clients.total_connections);
            printf("  总创建房间数: %" PRIu64 "\n", server.rooms.total_rooms_created);
            printf("  总处理消息数: %" PRIu64 "\n", server.total_messages);
            printf("  总错误数: %" PRIu64 "\n", server.total_errors);
        }
        printf("=================================================\n");
    }
    
    return ret;
}

/**
 * @brief 程序退出时调用的清理函数
 */
static void cleanup(void) {
    if (global_server_ctx) {
        printf("紧急清理...\n");
        server_stop(global_server_ctx);
        
        /* 等待一小段时间让服务器停止 */
        sleep(1);
    }
}

/* 注册清理函数 */
__attribute__((constructor)) static void init(void) {
    atexit(cleanup);
    
    /* 设置本地化（如果需要中文支持） */
    setlocale(LC_ALL, "zh_CN.UTF-8");
}

/**
 * @brief 检查系统资源
 * @return 0 表示资源充足，-1 表示资源不足
 */
static int check_system_resources(void) {
    /* 检查文件描述符限制 */
    struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
        if (rlim.rlim_cur < 8192) {
            fprintf(stderr, "警告: 文件描述符限制较低 (%lu)，建议至少 8192\n", 
                   (unsigned long)rlim.rlim_cur);
        }
    }
    
    return 0;
}