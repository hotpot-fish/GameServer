#ifndef _DBSERVER_H
#define _DBSERVER_H

#include <mysql/mysql.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include "FuncServer.h"

class DBServer : public FuncServer
{
private:
    // mysql config
    const char *mysql_ip = "127.0.0.1";
    int mysql_port = 3306;
    const char *mysql_user = "game";
    const char *mysql_password = "game123u";
    const char *db_name = "mydb";

    // redis config
    const char *redis_ip = "127.0.0.1";
    int redis_port = 6379;
    const char *redis_password = "114514";

private:
    MYSQL *mysql = nullptr;

    redisContext *redis = nullptr;

private:
    void HandleUserLogin(Net::LoginData &data, int fd);

    void HandleUserMoney(Net::UserMoney &data, int fd);

    /* 尝试连接其他类型功能服务器 */
    virtual void TryToConnectAvailabeServer();

protected:
    /*
     * @brief
     * search from redis first,
     * if not exist then find in mysql
     *
     * @return
     * -1 means somewhere error
     *  0 means user not exist
     *  1 means find user success
     */
    int QueryUser(std::string username, std::string password);

    int InsertUser(std::string username, std::string password);

    bool ChangeUserMoney(std::string username, int money, int&);

    virtual void OnMsgBodyAnalysised(Header head,
                                     const uint8_t *body,
                                     uint32_t length,
                                     int fd);

                                     
public:
    explicit DBServer();

    virtual ~DBServer();

    bool ConnectToMysqlAndRedis();
};

#endif