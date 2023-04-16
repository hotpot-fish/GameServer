#include <chrono>

#include "../Header/BattleServer.h"
#include "../Header/Encryption.hpp"

BattleServer::BattleServer()
{
    server_type = SERVER_TYPE::LOGIC;
    BattleServer::frameindex = 0;
}

BattleServer::~BattleServer()
{
}

void BattleServer::run()
{
    while (true)
    {
        // 获取当前时间
        for (auto iter = room_frames.begin(); iter != room_frames.end(); ++iter)
        {
            std::cout << "iter->first" << iter->first << "  room_state[iter->first]:  "<<room_state[iter->first]<<std::endl;
            if (room_state[iter->first])
            {
                if(iter->second.index()!=-1) iter->second.set_index(room_frame_indexs[iter->first]);
                iter->second.set_sendcase(Net::Frame_Send_All);
                iter->second.set_roomid(iter->first);
                room_all_frames[iter->first].push_back(iter->second);
                int length = iter->second.ByteSizeLong();
                uint8_t *array = new uint8_t[length];

                if (iter->second.SerializeToArray(array, length))
                {
                    for (const auto &pair : ServerBase::connections)
                    {
                        if (pair.first != FuncServer::center_server_client)
                        {
                            SendMsg(BODYTYPE::Frame, length, array, pair.first);
                            std::cout << "121212Send to conns :" << pair.first << "Frameindex:" << room_frame_indexs[iter->first] << " ProtoLength: " << length << std::endl;
                        }
                    }
                }
                iter->second.clear_opts();
                iter->second.clear_index();
                room_frame_indexs[iter->first]++;
            }
            else
            {
                iter->second.set_index(-1);
                iter->second.set_sendcase(Net::Frame_Send_All);
                iter->second.set_roomid(iter->first);
                if(iter->second.opts().size()!=0){
                    room_all_frames[iter->first].push_back(iter->second);
                    std::cout<<"Login Logout save"<<std::endl;
                }
                int length = iter->second.ByteSizeLong();
                uint8_t *array = new uint8_t[length];

                if (iter->second.SerializeToArray(array, length))
                {
                    for (const auto &pair : ServerBase::connections)
                    {
                        if (pair.first != FuncServer::center_server_client)
                        {
                            SendMsg(BODYTYPE::Frame, length, array, pair.first);
                        }
                    }
                }
                iter->second.clear_opts();
            }
        }
        // 等待一段时间
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
}

void BattleServer::startThread()
{
    std::thread t(&BattleServer::run, this);
    t.detach();
}

void BattleServer::TryToConnectAvailabeServer()
{
    // DBServer 无需连接其他功能服务器
}

void BattleServer::OnMsgBodyAnalysised(Header head, const uint8_t *body, uint32_t length, int fd)
{
    bool parseRet = false;
    BODYTYPE type = head.type;

    Net::PlayerOptData playerOptData;
    Net::LoginResponse loginResponse;
    Net::Frame frame;

    switch (type)
    {
    case BODYTYPE::PlayerOptData:
        /* code */
        playerOptData = ProtoUtil::ParseBodyToPlayerOptData(body, length, parseRet);

        if (playerOptData.itemid() == -1)
        { // 移除房间
            if (playerOptData.userid() == -1)
            {
                std::cout << "移除房间: " << playerOptData.roomid() << std::endl;
                room_frames.erase(playerOptData.roomid());
                room_all_frames.erase(playerOptData.roomid());
                room_login_frames.erase(playerOptData.roomid());
                room_frame_indexs.erase(playerOptData.roomid());
                room_playernum.erase(playerOptData.roomid());

                room_state.erase(playerOptData.roomid());
                if (!room_frames.count(playerOptData.roomid()))
                {
                    std::cout << "删了" << std::endl;
                }
            }
            else
            {
                if (playerOptData.opt() == Net::PlayerOpt::User_Logout)
                {
                    // 发送登出信息
                    room_frames[playerOptData.roomid()].set_index(-1);
                    room_frames[playerOptData.roomid()].add_opts()->CopyFrom(playerOptData);
                }
            }
        }
        else
        {
            if (!room_frames.count(playerOptData.roomid()))
            {
                room_frames[playerOptData.roomid()] = Net::Frame();
                room_all_frames[playerOptData.roomid()] = std::vector<Net::Frame>();
                room_login_frames[playerOptData.roomid()] = std::vector<Net::Frame>();
                room_frame_indexs[playerOptData.roomid()] = 0;
                room_playernum[playerOptData.roomid()] = 0;
                room_state[playerOptData.roomid()] = false;
            }

            if (playerOptData.opt() == Net::PlayerOpt::User_Login)
            {
                for (int i = 0; i < room_all_frames[playerOptData.roomid()].size(); i++)
                {
                    Net::Frame loginframe = room_all_frames[playerOptData.roomid()][i];
                    loginframe.set_userid(playerOptData.userid());
                    loginframe.set_sendcase(Net::Frame_Send_Single);

                    int length = loginframe.ByteSizeLong();
                    uint8_t *array = new uint8_t[length];
                    if (loginframe.SerializeToArray(array, length))
                    {
                        for (const auto &pair : ServerBase::connections)
                        {
                            if (pair.first != FuncServer::center_server_client)
                            {
                                SendMsg(BODYTYPE::Frame, length, array, pair.first);
                                std::cout << "追帧登录信息" << std::endl;
                                std::cout << "Send to conns :" << pair.first << " ProtoLength: " << length << "  UserLogin" << std::endl;
                            }
                        }
                    }
                }
                room_frames[playerOptData.roomid()].set_index(-1);
                room_playernum[playerOptData.roomid()] = room_playernum[playerOptData.roomid()] + 1;
            }
            room_frames[playerOptData.roomid()].add_opts()->CopyFrom(playerOptData);
        }
        // false 的情况已在util的函数里处理

        break;
    case BODYTYPE::Frame:
        frame = ProtoUtil::ParseBodyToFrame(body, length, parseRet);
        room_playernum[frame.roomid()] = MAX_MAP_SIZE;
        room_state[frame.roomid()] = true;
        break;

    default:

        break;
    }

    FuncServer::OnMsgBodyAnalysised(head, body, length, fd);
}

// ---------------------------------------------------------------------
// network msg handle
// ---------------------------------------------------------------------

// ---------------------------------------------------------------------

// ---------------------------------------------------------------------
// sql operation
// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s port\n", argv[0]);
        return 1;
    }

    int port = std::atoi(argv[1]);

    BattleServer battleServer;

    battleServer.startThread();

    battleServer.BootServer(port);
    return 0;
}