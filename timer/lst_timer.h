/**
 *
 * 定时器设计，将连接资源和定时事件等封装起来，具体包括连接资源、超时时间和回调函数，这里的回调函数指向定时事件。

定时器容器设计，将多个定时器串联组织起来统一处理，具体包括升序链表设计。

定时任务处理函数，该函数封装在容器类中，具体的，函数遍历升序链表容器，根据超时时间，处理对应的定时器。

代码分析-使用定时器，通过代码分析，如何在项目中使用定时器。
*/

#ifndef LST_TIMER
#define LST_TIMER

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../log/log.h"

class util_timer;

struct client_data {
    sockaddr_in address; //用户socket地址
    int sockfd;
    util_timer *timer; // 定时器
};

class util_timer {
   public:
    util_timer() : prev_(nullptr), next_(nullptr) {}

   public:
    time_t expire_; // 超出时间

    void (*cb_func)(client_data *); // call back
    client_data *user_data_; // 用户资源
    util_timer *prev_; // 前向定时器
    util_timer *next_; // 后继定时器
};

// 双向升序链表作为定时器的container
class sort_timer_lst {
   public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer *timer);
    void adjust_timer(util_timer *timer);
    void del_timer(util_timer *timer);
    void tick();

   private:
    void add_timer(util_timer *timer, util_timer *lst_head);

    util_timer *head_;
    util_timer *tail_;
};

class Utils {
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

    // 设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    // 定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

   public:
    static int *u_pipefd;
    sort_timer_lst timer_lst_;
    static int u_epollfd;
    int TIMESLOT_;
};

void cb_func(client_data *user_data);

#endif