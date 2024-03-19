#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "log.h"
#include <pthread.h>
using namespace std;

Log::Log(){
    count_ = 0;
    is_async_ = false; // 默认是同步的
}

Log::~Log(){
    if(fp_ != nullptr){
        fclose(fp_); // Question C++中的文件操作
    }
}

bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size){
    if(max_queue_size >= 1){
        is_async_ = true; // 异步模式 Question C++同步和异步
        log_queue_ = new block_queue<string>(max_queue_size);
        pthread_t tid;
        //flush_log_thread为回调函数,这里表示创建线程异步写日志
        pthread_create(&tid, nullptr, flush_log_thread, nullptr);
    }

    // 赋值操作
    close_log_ = close_log;
    log_buf_size_ = log_buf_size;
    buf_ = new char[log_buf_size_];
    memset(buf_, '\0', log_buf_size_);
    split_lines_ = split_lines;

    time_t t = time(nullptr); // Question C++中时间的模块
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};
}


void Log::flush(void){
    mutex_.lock();
    fflush(fp_); // 强制刷新写入流缓冲区
    mutex_.unlock();
}