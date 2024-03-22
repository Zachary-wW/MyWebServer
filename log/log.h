/*
 * 日志： 由服务器创建，记录运行状态、错误信息和访问数据的文件
   同步日志：日志写入函数和工作线程串行，涉及IO操作，单条日志过大->阻塞
   异步日志：将日志的内容放入阻塞队列中，写线程从其中取出内容写入
*/

#ifndef LOG_H
#define LOG_H

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>

#include <iostream>
#include <string>

#include "block_queue.h"

using namespace std;

class Log {
   public:
    // 懒汉式 什么是懒汉式
    // Question 为什么要使用static static的作用
    // Question 设计模式 单例模式 类别 有什么区别
    static Log *get_instance() {  // Question 指针和引用的区别
        static Log instance;      // 使用的时候才去初始化
        return &instance;  // C++11之后使用局部静态变量实现懒汉模式不用加锁
    }                      // 提供唯一的接口

    // 公有异步日志写入方法 调用了私有方法
    static void *flush_log_thread(void *args) {
        Log::get_instance()->async_write_log();
    }

    // Question 函数参数默认值的限制
    // 日志文件 缓存区大小 最大行数 最长日志条队列
    bool init(const char *file_name, int close_log, int log_buf_size = 8192,
              int split_lines = 5000000, int max_queue_size = 0);
    // Question 可变参数 模板可变参数
    // 将输出内容按照format整理
    void write_log(int level, const char *format, ...);
    // 强制刷新缓冲区
    void flush(void);

   private:
    Log();
    virtual ~Log();  // Question 为什么构造函数不能virtual
    // 异步写方法
    void *async_write_log() {
        string single_log;
        // 利用pop从阻塞队列取出一条日志
        while (log_queue_->pop(single_log)) {
            mutex_.lock();
            fputs(single_log.c_str(), fp_);
            mutex_.unlock();
        }
    }

   private:
    char dir_name[128];  // 路径名
    char log_name[128];  // log文件名
    int split_lines_;    // 日志最大行数
    int log_buf_size_;   // 日志缓冲区大小
    long long count_;    // 日志行数记录
    int today_;          // 因为按天分类,记录当前时间是哪一天
    FILE *fp_;           // 打开log的文件指针
    char *buf_;          // 输出的内容
    block_queue<string> *log_queue_;  // 阻塞队列
    bool is_async_;                   // 是否同步标志位
    locker mutex_;
    int close_log_;  // 关闭日志
};

// 日志类中的方法不会被其他程序 直接 调用，一般通过可变宏参数调用
#define LOG_DEBUG(format, ...)                                    \
    if (0 == close_log_) {                                        \
        Log::get_instance()->write_log(0, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }
#define LOG_INFO(format, ...)                                     \
    if (0 == close_log_) {                                        \
        Log::get_instance()->write_log(1, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }
#define LOG_WARN(format, ...)                                     \
    if (0 == close_log_) {                                        \
        Log::get_instance()->write_log(2, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }
#define LOG_ERROR(format, ...)                                    \
    if (0 == close_log_) {                                        \
        Log::get_instance()->write_log(3, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }

// __VA_ARGS__宏前面加上##的作用在于，当可变参数的个数为0时，
// 这里printf参数列表中的的##会把前面多余的","去掉，否则会编译出错
#endif