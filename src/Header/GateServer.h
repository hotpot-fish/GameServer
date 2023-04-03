#ifndef _GATESERVER_H
#define _GATESERVER_H

#include "FuncServer.h"

class GateServer : public FuncServer
{
private:
    /* 记录userid和fd的映射 */
    std::map<std::string, int> user_fd_record;                      // client
    std::map<int, std::map<std::string, int>> room_user_fd_records; //
    std::map<std::string, int> user_room_record;                    // username roomid
    std::map<int, std::string> userid_username_record;
    std::map<std::string, int> username_userid_record; // 记录username 和userid对应
    std::vector<std::string> logout_clients;           // 记录断开客户端
    int cur_room_size = 0;
    int max_map_size = MAX_MAP_SIZE;

private:
    /* 尝试连接其他类型功能服务器 */
    virtual void TryToConnectAvailabeServer();

    void DisconnectAllClients();

protected:
    virtual void OnMsgBodyAnalysised(Header head,
                                     const uint8_t *body,
                                     uint32_t length,
                                     int fd);

    /*
     * @brief 成功连接到center server后触发
     */
    virtual void OnConnectToCenterServer();

public:
    explicit GateServer();

    virtual ~GateServer();

    virtual void CloseClientSocket(int fd);
};

#endif