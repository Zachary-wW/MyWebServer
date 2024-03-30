#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

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
#include <unistd.h>

#include <map>

#include "../CGImysql/sql_connection_pool.h"
#include "../lock/locker.h"
#include "../log/log.h"
#include "../timer/lst_timer.h"

class http_conn {
   public:
    // 读取文件名称大小
    static const int FILENAME_LEN = 200;
    // 读缓冲区大小
    static const int READ_BUFFER_SIZE = 2048;
    // 写缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024;
    // 报文请求方法
    enum METHOD {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    // 主状态机的状态
    enum CHECK_STATE {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    // 报文解析结果
    enum HTTP_CODE {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    // 从状态机状态
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

   public:
    http_conn() {}
    ~http_conn() {}

   public:
    // 初始化套接字， 内部调用私有方法
    void init(int sockfd, const sockaddr_in &addr, char *, int, int,
              string user, string passwd, string sqlname);
    // 关闭http连接
    void close_conn(bool real_close = true);
    void process();
    // 读取全部数据
    bool read_once();
    // 响应报文写函数
    bool write();
    sockaddr_in *get_address() { return &address_; }
    // 同步线程初始化数据库读取表
    void initmysql_result(connection_pool *connPool);
    int timer_flag;
    int improv;

   private:
    void init();
    // 从读缓冲区读取数据并处理
    HTTP_CODE process_read();
    // 向写缓冲区写入数据
    bool process_write(HTTP_CODE ret);
    // 主状态机解析报文的请求行数据
    HTTP_CODE parse_request_line(char *text);
    // 请求头数据
    HTTP_CODE parse_headers(char *text);
    // 请求内容
    HTTP_CODE parse_content(char *text);
    // 生成相应报文
    HTTP_CODE do_request();
    // 指向未处理的字符
    char *get_line() { return read_buf_ + start_line_; };
    // 从状态机读取一行
    LINE_STATUS parse_line();
    void unmap();

    // 根据报文响应格式生成的部分
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

   public:
    static int epollfd_;
    static int user_count_;
    MYSQL *mysql;
    int state_;  // 读为0, 写为1

   private:
    int sockfd_;
    sockaddr_in address_;
    // 读取缓冲区
    char read_buf_[READ_BUFFER_SIZE];
    // 缓冲区最后一个字节的下一个位置
    long read_idx_;
    long checked_idx_;
    // 已经解析的字符个数
    int start_line_;

    // 写缓冲区
    char write_buf_[WRITE_BUFFER_SIZE];
    // buffer长度
    int write_idx_;

    // 主状态机状态
    CHECK_STATE check_state_;
    // 请求方法
    METHOD method_;

    // 解析请求报文对应的变量
    char real_file_[FILENAME_LEN];
    char *url_;
    char *version_;
    char *host_;
    long content_length_;
    bool linger_;

    // 读取服务器上的文件地址
    char *file_address_;
    struct stat file_stat_;
    // IO向量机制
    struct iovec iv_[2];
    int iv_count_;
    // 是否启用的POST
    int cgi;
    // 存储请求头数据
    char *string_;
    // 剩余发送字节
    int bytes_to_send;
    // 已经发送字节
    int bytes_have_send;
    char *doc_root;

    map<string, string> users_;
    int TRIGMode_;
    int close_log_;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif