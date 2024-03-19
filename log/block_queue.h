#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>

#include <iostream>

#include "../lock/locker.h"
using namespace std;

template <class T>
class block_queue {
   public:
    block_queue(int max_size = 1000) {
        if (max_size <= 0) exit(-1);
        maxSize_ = max_size;
        array_ = new T[max_size];
        size_ = 0;
        front_ = -1;
        back_ = -1;
    }

    ~block_queue() {
        mutex_.lock();
        if (array_ != nullptr) delete[] array_;  // delete array
        mutex_.unlock();
    }

    void clear() {
        mutex_.lock();
        size_ = 0;
        front_ = -1;
        back_ = -1;
        mutex_.unlock();
    }

    bool full() {
        mutex_.lock();
        if (size_ >= maxSize_) {
            mutex_.unlock();
            return true;
        }
        mutex_.unlock();
        return false;
    }

    bool empty() {
        mutex_.lock();
        if (size_ == 0) {
            mutex_.unlock();
            return true;
        }
        mutex_.unlock();
        return false;
    }

    bool front(T &val) {
        mutex_.lock();
        if (size_ == 0) {
            mutex_.unlock();
            return false;
        }
        val = array_[front_];
        mutex_.unlock();
        return true;
    }

    bool back(T &val) {
        mutex_.lock();
        if (size_ == 0) {
            mutex_.unlock();
            return false;
        }
        val = array_[back_];
        mutex_.unlock();
        return true;
    }

    int getSize() {
        int size = 0;
        mutex_.lock();
        size = size_;
        mutex_.unlock();
        return size;
    }

    int getMaxSize() {
        int maxsize = 0;
        mutex_.lock();
        maxsize = maxSize_;
        mutex_.unlock();
        return maxsize;
    }

    bool push(T &item) {
        mutex_.lock();
        if (size_ >= maxSize_) {
            cond_.broadcast();  // awake all thread
            mutex_.unlock();
            return false;
        }
        back_ = (back_ + 1) % maxSize_;
        array_[back_] = item;
        ++size_;
        cond_.broadcast();
        mutex_.unlock();
        return true;
    }

    bool pop(T &item) {
        mutex_.lock();

        while (size_ <= 0) {
            if (!cond_.wait(mutex_.get())) {
                mutex_.unlock();
                return false;
            }
        }
        front_ = (front_ + 1) % maxSize_;
        item = array_[front_];
        --size_;
        mutex_.unlock();

        return true;
    }

    // time out pop func
    bool pop(T &item, int ms_timeout) {
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, nullptr);

        mutex_.lock();
        if (size_ <= 0) {
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if (!cond_.timewait(mutex_.get(), t)) {
                mutex_.unlock();
                return false;
            }
        }
        if (size_ <= 0) {
            mutex_.unlock();
            return false;
        }
        front_ = (front_ + 1) % maxSize_;
        item = array_[front_];
        --size_;
        mutex_.unlock();

        return true;
    }

   private:
    locker mutex_;
    cond cond_;
    
    T *array_;
    int size_;
    int maxSize_;
    int front_;
    int back_;
};

#endif