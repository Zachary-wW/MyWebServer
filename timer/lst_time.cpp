#include "../http/http_conn.h"
#include "lst_timer.h"

sort_timer_lst::sort_timer_lst() {
    head_ = nullptr;
    tail_ = nullptr;
}
sort_timer_lst::~sort_timer_lst() {
    util_timer *tmp = head_;
    // 遍历删除list
    while (tmp) {
        head_ = tmp->next_;
        delete tmp;
        tmp = head_;
    }
}

void sort_timer_lst::add_timer(util_timer *timer) {
    if (!timer) {
        return;
    }
    if (!head_) {
        head_ = tail_ = timer;
        return;
    }
    // 如果新的定时器超时时间小于当前头部结点
    // 直接将当前定时器结点作为头部结点
    if (timer->expire_ < head_->expire_) {
        timer->next_ = head_;
        head_->prev_ = timer;
        head_ = timer;
        return;
    }
    add_timer(timer, head_);  // othrewise，调用private method
}

// 调整定时器 任务发生变化时，调整定时器在链表中的位置
void sort_timer_lst::adjust_timer(util_timer *timer) {
    if (!timer) {
        return;
    }
    util_timer *tmp = timer->next_;
    // 被调整的定时器在链表尾部
    // 定时器超时值仍然小于下一个定时器超时值，不调整
    if (!tmp || (timer->expire_ < tmp->expire_)) {
        return;
    }

    // 被调整定时器是链表头结点，将定时器取出，重新插入
    if (timer == head_) {
        head_ = head_->next_;
        head_->prev_ = nullptr;
        timer->next_ = nullptr;
        add_timer(timer, head_);
    } else  // 被调整定时器在内部，将定时器取出，重新插入
    {
        timer->prev_->next_ = timer->next_;
        timer->next_->prev_ = timer->prev_;
        add_timer(timer, timer->next_);
    }
}

void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head) {
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next_;
    while (tmp) {
        if (timer->expire_ < tmp->expire_) {
            prev->next_ = timer;
            timer->next_ = tmp;
            tmp->prev_ = timer;
            timer->prev_ = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next_;
    }
    // 当插入的节点在tail
    if (!tmp) {
        prev->next_ = timer;
        timer->prev_ = prev;
        timer->next_ = nullptr;
        tail_ = timer;
    }
}

void sort_timer_lst::del_timer(util_timer *timer) {
    if (!timer) {
        return;
    }
    if ((timer == head_) && (timer == tail_)) {
        delete timer;
        head_ = nullptr;
        tail_ = nullptr;
        return;
    }
    if (timer == head_) {
        head_ = head_->next_;
        head_->prev_ = nullptr;
        delete timer;
        return;
    }
    if (timer == tail_) {
        tail_ = tail_->prev_;
        tail_->next_ = nullptr;
        delete timer;
        return;
    }
    timer->prev_->next_ = timer->next_;
    timer->next_->prev_ = timer->prev_;
    delete timer;
}

void sort_timer_lst::tick() {
    if (!head_) {
        return;
    }
    // 获取当前时间
    time_t cur = time(nullptr);
    util_timer *tmp = head_;
    while (tmp) {
        // 当前时间小于定时器的超时时间，后面的定时器也没有到期
        if (cur < tmp->expire_) {
            break;
        }
        // expire -> call back
        tmp->cb_func(tmp->user_data_);

        // 删除节点
        // TODO
        head_ = tmp->next_;
        if (head_) {
            head_->prev_ = nullptr;
        }
        delete tmp;
        tmp = head_;
    }
}

void Utils::init(int timeslot) { TIMESLOT_ = timeslot; }

// 对文件描述符设置非阻塞
int Utils::setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot) event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 信号处理函数
void Utils::sig_handler(int sig) {
    // 为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

// 设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler() {
    timer_lst_.tick();
    alarm(TIMESLOT_);
}

void Utils::show_error(int connfd, const char *info) {
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
// 定时器回调函数
void cb_func(client_data *user_data) {
    // 删除非活跃连接在socket上的注册事件
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    // 关闭文件描述符
    close(user_data->sockfd);
    // 减少连接数量
    --http_conn::user_count_;
}