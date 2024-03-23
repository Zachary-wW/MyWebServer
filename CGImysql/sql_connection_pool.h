/**
 * 池类似一个container 里面放着许多资源，这些资源在服务器启动的时候
 * 就是创建初始化了，本质上是对资源的复用
 * 当服务器需要资源的时候，直接从pool里面获取，不需要临时动态分配
 * 当不需要的时候可以放回去，而不是直接释放，免得下次还要创建
 */

#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <error.h>
#include <mysql/mysql.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <list>
#include <string>

#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class connection_pool {
   public:
    MYSQL *GetConnection();               // 获取数据库连接
    bool ReleaseConnection(MYSQL *conn);  // 释放连接
    int GetFreeConn();                    // 获取连接
    void DestroyPool();                   // 销毁所有连接

    // 单例模式
    // 局部静态变量获取instance
    static connection_pool *GetInstance();

    void init(string url, string User, string PassWord, string DataBaseName,
              int Port, int MaxConn, int close_log);

   private:
    connection_pool();
    ~connection_pool();

    int MaxConn_;   // 最大连接数
    int CurConn_;   // 当前已使用的连接数
    int FreeConn_;  // 当前空闲的连接数
    locker lock_;
    list<MYSQL *> connList_;  // 连接池
    sem reserve_;

   public:
    string url_;           // 主机地址
    string Port_;          // 数据库端口号
    string User_;          // 登陆数据库用户名
    string PassWord_;      // 登陆数据库密码
    string DatabaseName_;  // 使用数据库名
    int close_log_;        // 日志开关
};

class connectionRAII {
   public:
   // 有参构造 由于mysql是指针类型，故若需要修改就应该使用双指针
    connectionRAII(MYSQL **con, connection_pool *connPool);
    ~connectionRAII();

   private:
    MYSQL *conRAII;
    connection_pool *poolRAII;
};

#endif