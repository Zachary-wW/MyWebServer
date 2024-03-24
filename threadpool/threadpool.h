#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>

#include <cstdio>
#include <exception>
#include <list>

#include "../CGImysql/sql_connection_pool.h"
#include "../lock/locker.h"

/**
 * 模板类编程
 * 由于模板类的函数创建是在调用阶段（需要知道具体的类型）
 * 因此如果分文件书写会造成编译链接失败
 */

template <typename T>
class threadpool {
   public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int actor_model, connection_pool *connPool,
               int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T *request, int state);  // 向请求队列插入任务请求
    bool append_p(T *request);

   private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

   private:
    int thread_number_;  // 线程池中的线程数
    int max_requests_;   // 请求队列中允许的最大请求数
    pthread_t *threads_;  // 描述线程池的数组，其大小为thread_number_
    std::list<T *> workqueue_;   // 请求队列
    locker queuelocker_;         // 保护请求队列的互斥锁
    sem queuestat_;              // 是否有任务需要处理
    connection_pool *connPool_;  // 数据库
    int actor_model_;            // 模型切换
};

// 实现
template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool,
                          int thread_number, int max_requests)
    : actor_model_(actor_model),
      thread_number_(thread_number),
      max_requests_(max_requests),
      threads_(nullptr),
      connPool_(connPool) {
    // 参数异常情况
    if (thread_number <= 0 || max_requests <= 0) throw std::exception();
    // 创建线程池数组
    threads_ = new pthread_t[thread_number_];
    // 空指针说明创建失败
    if (!threads_) throw std::exception();
    // Question C++多线程编程
    for (int i = 0; i < thread_number; ++i) {
        // 创建线程
        if (pthread_create(threads_ + i, nullptr, worker, this) != 0) {
            delete[] threads_;
            throw std::exception();
        }
        // 分离线程
        if (pthread_detach(threads_[i])) {
            delete[] threads_;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool() {
    delete[] threads_;
}

// 向请求队列中添加请求
template <typename T>
bool threadpool<T>::append(T *request, int state) {
    queuelocker_.lock();
    if (workqueue_.size() >= max_requests_) {
        queuelocker_.unlock();
        return false;
    }
    // state包含事件类型（read/write）
    request->state_ = state;
    workqueue_.push_back(request);
    queuelocker_.unlock();
    queuestat_.post();
    return true;
}

template <typename T>
bool threadpool<T>::append_p(T *request) {
    queuelocker_.lock();
    if (workqueue_.size() >= max_requests_) {
        queuelocker_.unlock();
        return false;
    }
    workqueue_.push_back(request);
    queuelocker_.unlock();
    queuestat_.post();
    return true;
}

template <typename T>
void *threadpool<T>::worker(void *arg) {
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run() {
    while (true) {
        queuestat_.wait();
        queuelocker_.lock();
        if (workqueue_.empty()) {
            queuelocker_.unlock();
            continue;
        }
        T *request = workqueue_.front();
        workqueue_.pop_front();
        queuelocker_.unlock();
        if (!request) continue;
        // TODO 修改request成员
        if (1 == actor_model_) {
            // read
            if (0 == request->state_) {
                if (request->read_once()) {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, connPool_);
                    request->process();
                } else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            } else {  // write
                if (request->write()) {
                    request->improv = 1;
                } else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        } else {
            connectionRAII mysqlcon(&request->mysql, connPool_);
            request->process();
        }
    }
}

#endif