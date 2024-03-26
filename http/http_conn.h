#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class http_conn{

public:
    static int epollfd_;
    static int user_count_;
    MYSQL *mysql;
    int state_;  //读为0, 写为1
};


#endif