#include "sql_connection_pool.h"

#include <mysql/mysql.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <list>
#include <string>

connection_pool::connection_pool() {
    CurConn_ = 0;
    FreeConn_ = 0;
}

// RAII机制销毁pool
connection_pool::~connection_pool() { DestroyPool(); }

connection_pool *connection_pool::GetInstance() {
    static connection_pool connPool;
    return &connPool;
}

void connection_pool::init(string url, string User, string PassWord,
                           string DBName, int Port, int MaxConn,
                           int close_log) {
    url_ = url;
    Port_ = Port;
    User_ = User;
    PassWord_ = PassWord;
    DatabaseName_ = DBName;
    close_log_ = close_log;

    // 创建最大maxconn条连接
    for (int i = 0; i < MaxConn; i++) {
        MYSQL *con = nullptr;
        con = mysql_init(con);

        if (con == nullptr) {
            LOG_ERROR("MySQL Error");
            exit(1);
        }

        // 尝试与运行在主机上的MySQL数据库引擎建立连接
        con =
            mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(),
                               DBName.c_str(), Port, nullptr, 0);

        if (con == nullptr) {
            LOG_ERROR("MySQL Error");
            exit(1);
        }
        connList_.push_back(con);  // 将创建的连接加入进去
        ++FreeConn_;               // 创建新的自然空闲多了
    }

    reserve_ = sem(FreeConn_);  // 信号量的数量就是连接的数量

    MaxConn_ = FreeConn_;
}

// 当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection() {
    MYSQL *con = nullptr;

    if (0 == connList_.size())  // 没有连接
        return nullptr;

    reserve_.wait();  // 取出连接，-1，如果为0则block

    lock_.lock();

    con = connList_.front();  // 取出连接
    connList_.pop_front();

    --FreeConn_;
    ++CurConn_;

    lock_.unlock();
    return con;
}

// 释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con) {
    if (con == nullptr) return false;

    lock_.lock();

    connList_.push_back(con);
    ++FreeConn_;
    --CurConn_;

    lock_.unlock();

    reserve_.post();  // 资源+1
    return true;
}

// 销毁数据库连接池
void connection_pool::DestroyPool() {
    lock_.lock();
    if (connList_.size() > 0) {
        list<MYSQL *>::iterator it;
        // 通过迭代器遍历销毁连接
        for (it = connList_.begin(); it != connList_.end(); ++it) {
            MYSQL *con = *it;
            mysql_close(con);
        }
        CurConn_ = 0;
        FreeConn_ = 0;
        connList_.clear();
    }

    lock_.unlock();
}

// 当前空闲的连接数
int connection_pool::GetFreeConn() { return this->FreeConn_; }

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool) {
    *SQL = connPool->GetConnection();

    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII() { poolRAII->ReleaseConnection(conRAII); }