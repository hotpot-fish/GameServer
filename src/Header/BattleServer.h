#ifndef _DBSERVER_H
#define _DBSERVER_H

#include <mysql/mysql.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <chrono>
#include <thread>
#include "FuncServer.h"

class BattleServer : public FuncServer
{
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

    virtual void OnMsgBodyAnalysised(Header head,
                                     const uint8_t *body,
                                     uint32_t length,
                                     int fd);

public:
    Net::Frame tmpframe;
    std::map<int, std::vector<Net::Frame>> room_all_frames;   // 房间所有帧
    std::map<int, Net::Frame> room_frames;                    // 房间当前帧
    std::map<int, int> room_frame_indexs;                     // 房间当前帧和索引
    std::map<int, int> room_playernum;                        // 房间人数
    std::map<int, bool> room_state;                           // 房间状态
    std::map<int, std::vector<Net::Frame>> room_login_frames; // 追帧
    int frameindex = 1;

    explicit BattleServer();

    void run();

    void startThread();

    virtual ~BattleServer();

    bool ConnectToMysqlAndRedis();
};

#endif