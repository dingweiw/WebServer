#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"

class util_timer;

struct client_data
{
    sockaddr_in address; // 客户端socket地址
    int sockfd;         // socket文件描述符
    util_timer *timer;  // 定时器
};

class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL)   {}
    time_t expires;  // 定时器超时时间 = 浏览器和服务器连接时刻 + 固定时间

    void (*cb_func)(client_data *d); // 回调函数，通过函数指向调用的函数
    client_data *user_data;  // 连接资源
    util_timer *prev;        // 前驱定时器
    util_timer *next;       // 后继计时器
};

// 计时器容器类
class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer *timer); //将目标定时器添加到链表中，将定时器按超时时间升序插入
    void adjust_timer(util_timer *timer); // 任务发生变化时，调整该定时器在链表中的位置
    void del_timer(util_timer *timer); // 删除定时器
    void tick();  // 定时任务处理函数，处理链表容器中到期的定时器
private:
    void add_timer(util_timer *time, util_timer *lst_head);// 调整节点位置

    util_timer *head; // 头结点
    util_timer *tail; // 尾结点
};

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    // 对文件描述符设置非阻塞
    int setnonblocking(int fd);

    // 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    // 信号处理函数
    static void sig_handler(int sig);

    //设置信号函数，设置需要捕捉的信号
    void addsig(int sig, void(handler)(int), bool restart = true);

    // 定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();
    // 向客户端发送错误信号
    void show_error(int connfd, const char *info);
public:
    static int *u_pipefd;    // 管道文件描述符
    sort_timer_lst m_timer_lst; // 定时器链表
    static int u_epollfd; // 内核中的EPOLL事件表文件描述符
    int m_TIMESLOT; // 超时时间
};

void cb_func(client_data *user_data);

#endif
