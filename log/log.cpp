#include "log.h"

#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
using namespace std;

Log::Log() {
    count_ = 0;
    is_async_ = false;  // 默认是同步的
}

Log::~Log() {
    if (fp_ != nullptr) {
        fclose(fp_);  // Question C++中的文件操作
    }
}

bool Log::init(const char *file_name, int close_log, int log_buf_size,
               int split_lines, int max_queue_size) {
    // 根据队列元素的大小判断同步还是异步
    if (max_queue_size >= 1) {  // 如果阻塞队列有元素即为async
        is_async_ = true;       // 异步模式 Question C++同步和异步
        log_queue_ = new block_queue<string>(max_queue_size);
        pthread_t tid;
        pthread_create(
            &tid, nullptr, flush_log_thread,
            nullptr);  // flush_log_thread为回调函数,这里表示创建线程异步写日志
    }

    // 初始化对应参数
    close_log_ = close_log;
    log_buf_size_ = log_buf_size;
    buf_ = new char[log_buf_size_];
    memset(buf_, '\0',
           log_buf_size_);  // 初始化buffer为空字符 \0为字符串结尾标识
    split_lines_ = split_lines;

    // Question C++中时间的模块
    time_t t =
        time(nullptr);  // time函数返回系统的当前日历时间，自 1970 年 1 月 1
                        // 日以来经过的秒数。如果系统没有时间，则返回 -1
    struct tm *sys_tm =
        localtime(&t);  // 该函数返回一个指向表示本地时间的 tm 结构的指针。
    struct tm my_tm = *sys_tm;

    const char *p = strrchr(file_name, '/');  // 查找最后一个/的位置
    char log_full_name[256] = {0};            // 日志名称

    // 自定义日志名 如果输入的日志没有/，则将文件命名成时间+文件名
    if (p == nullptr) {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900,
                 my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    } else {
        strcpy(log_name, p + 1);  // 复制/后面的内容
        strncpy(dir_name, file_name,
                p - file_name +
                    1);  // Query: p - file_name + 1是文件所在路径文件夹的长度
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name,
                 my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                 log_name);
    }

    today_ = my_tm.tm_mday;  // 获取day

    fp_ = fopen(
        log_full_name,
        "a");  // mode
               // a追加到一个文件。写操作向文件末尾追加数据。如果文件不存在，则创建文件。
    if (fp_ == nullptr) {
        return false;
    }

    return true;
}

void Log::write_log(int level, const char *format, ...) {
    struct timeval now = {0, 0};  // 包含秒数/微秒
    gettimeofday(&now, nullptr);  // 获取当前时间
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};

    // 根据level判断类型
    switch (level) {
        case 0:
            strcpy(s, "[debug]:");
            break;
        case 1:
            strcpy(s, "[info]:");
            break;
        case 2:
            strcpy(s, "[warn]:");
            break;
        case 3:
            strcpy(s, "[erro]:");
            break;
        default:
            strcpy(s, "[info]:");
            break;
    }

    // 写入log
    mutex_.lock();
    count_++;  // 更新行数

    if (today_ != my_tm.tm_mday ||
        count_ % split_lines_ == 0)  // 判断天不是今天 or 行数是最大行数的倍数
    {
        char new_log[256] = {0};
        fflush(fp_);
        fclose(fp_);
        char tail[16] = {0};
        // 格式化文件名时间部分
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900,
                 my_tm.tm_mon + 1, my_tm.tm_mday);

        if (today_ !=
            my_tm.tm_mday)  // 如果不是今天 则创建今天的日志，更新today和count
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            today_ = my_tm.tm_mday;
            count_ = 0;
        } else {  // 超过了最大行，在之前的日志名基础上加后缀
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name,
                     count_ / split_lines_);
        }
        fp_ = fopen(new_log, "a");
    }

    mutex_.unlock();

    va_list valst;
    va_start(
        valst,
        format);  // 初始化valst变量,
                  // format是最后一个传递给函数的已知的固定参数，即省略号之前的参数。

    string log_str;
    mutex_.lock();

    // 写入的具体时间内容格式
    // 时间格式化，snprintf成功返回写字符的总数，其中不包括结尾的null字符
    int n = snprintf(buf_, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    // 内容格式化，用于向字符串中打印数据、数据格式用户自定义，返回写入到字符数组str中的字符个数(不包含终止符)
    // 使用参数列表发送格式化输出到字符串
    int m = vsnprintf(buf_ + n, log_buf_size_ - n - 1, format, valst);
    buf_[n + m] = '\n';
    buf_[n + m + 1] = '\0';
    log_str = buf_;

    mutex_.unlock();

    if (is_async_ && !log_queue_->full())  // 如果是异步模式且阻塞队列不满
    {
        log_queue_->push(log_str);
    } else  // 否则直接写入
    {
        mutex_.lock();
        fputs(log_str.c_str(), fp_);
        mutex_.unlock();
    }

    va_end(valst);  // 与va_start搭配使用
}

void Log::flush(void) {
    mutex_.lock();
    fflush(fp_);  // fflush()会强迫将缓冲区内的数据写回参数stream
                  // 指定的文件中，如果参数stream
                  // 为NULL，fflush()会将所有打开的文件数据更新。
    mutex_.unlock();
}