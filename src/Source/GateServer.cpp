
#include "../Header/GateServer.h"

GateServer::GateServer()
{
    server_type = SERVER_TYPE::GATE;
}

GateServer::~GateServer()
{
}

void GateServer::CloseClientSocket(int fd)
{
    ServerBase::CloseClientSocket(fd);

    // 如果和logic server断开连接 告知所有客户端并则尝试重连
    if (fd == logic_server_client)
    {
        // notice clients
        DisconnectAllClients();

        // retry
        logic_server_client = -1;
        TryToConnectAvailabeServer();
    }

    // 如果和db server断开连接 则尝试重连
    if (fd == db_server_client)
    {
        db_server_client = -1;
        TryToConnectAvailabeServer();
    }
}

void GateServer::OnConnectToCenterServer()
{
    TryToConnectAvailabeServer();
}

void GateServer::TryToConnectAvailabeServer()
{
    if (logic_server_client == -1)
    {
        ApplyServerByType(SERVER_TYPE::LOGIC);
    }

    if (db_server_client == -1)
    {
        ApplyServerByType(SERVER_TYPE::DATABASE);
    }
}

void GateServer::DisconnectAllClients()
{
    for (auto it = connections.begin(); it != connections.end();)
    {
        if (it->first == center_server_client)
        {
            ++it;
        }
        else if (it->first == logic_server_client)
        {
            ++it;
        }
        else if (it->first == db_server_client)
        {
            ++it;
        }
        else
        {
            auto cur = it;
            ++it;
            CloseClientSocket(cur->first);
        }
    }
}

void GateServer::OnMsgBodyAnalysised(Header head, const uint8_t *body, uint32_t length, int fd)
{
    bool parseRet = false;
    BODYTYPE type = head.type;
    int tmplength;
    uint8_t *tmpbody;
    Net::LoginData loginData;
    Net::LoginResponse loginResp;
    Net::ServerInfo server_info;
    Net::UserMoney userMoney;
    Net::Frame frame;
    Net::PlayerOptData playerOptData;
    Net::PlayerMessage playerMessage;

    switch (type)
    {
    case BODYTYPE::LoginData:

        loginData = ProtoUtil::ParseBodyToLoginData(body, length, parseRet);

        // false 的情况已在util的函数里处理
        if (parseRet)
        {
            user_fd_record[loginData.username()] = fd;
            std::cout << "fd:" << fd << std::endl;
            // 转发给db server处理
            SendMsg(BODYTYPE::LoginData, length, body, db_server_client);
        }

        break;
    case BODYTYPE::LoginResponse:
        loginResp = ProtoUtil::ParseBodyToLoginResponse(body, length, parseRet);

        // false 的情况已在util的函数里处理
        if (parseRet)
        {
            int client_fd = user_fd_record[loginResp.username()];
            SendMsg(BODYTYPE::LoginResponse, length, body, client_fd);

            if (loginResp.opt() == Net::LoginResponse_Operation_Register)
            {
                // 注册操作 移除临时record
            }
            else if (loginResp.result() == 0 || loginResp.result() == -1)
            {
                // 登录失败 移除record
            }
            else
            {

                // 记录username userid关系
                userid_username_record[loginResp.userid()] = loginResp.username();
                username_userid_record[loginResp.username()] = loginResp.userid();

                room_user_fd_records[0][loginResp.username()] = user_fd_record[loginResp.username()]; // 添加到房间
                user_room_record[loginResp.username()] = 0;

                /*
                playerOptData.set_itemid(0);
                playerOptData.set_userid(loginResp.userid());
                playerOptData.set_roomid(0);
                playerOptData.set_opt(Net::PlayerOpt::User_Login);
                tmplength =  playerOptData.ByteSizeLong();
                tmpbody = new uint8_t[tmplength];

                if (playerOptData.SerializeToArray(tmpbody, tmplength))
                {
                    SendMsg(BODYTYPE::PlayerOptData, tmplength, tmpbody, logic_server_client);
                }
                */

                /*
                std::cout<<"注册时"<<std::endl;
                std::cout<<"user_fd_record.size"<<user_fd_record.size()<<std::endl;
                std::cout<<"room_user_fd_records.size"<<room_user_fd_records.size()<<std::endl;
                std::cout<<"user_room_record.size"<<user_room_record.size()<<std::endl;
                std::cout<<"userid_username_record.size"<<userid_username_record.size()<<std::endl;
                std::cout<<"username_userid_record.size"<<username_userid_record.size()<<std::endl;
                std::cout<<"logout_clients.size"<<logout_clients.size()<<std::endl;
                std::cout<<"cur_room_size:"<<cur_room_size<<std::endl;
                */

                /*
                if( room_user_fd_records[cur_room_size].size() == MAX_MAP_SIZE){
                    Net::Frame loginframe;
                    loginframe.set_roomid(cur_room_size);
                    loginframe.set_userid(playerOptData.userid());
                    loginframe.set_index(0);
                    loginframe.set_sendcase(Net::Frame_Send_All);
                    int length =  loginframe.ByteSizeLong();
                    uint8_t *array = new uint8_t[length];
                    if (loginframe.SerializeToArray(array, length))
                    {
                        SendMsg(BODYTYPE::Frame, length, array, logic_server_client);
                    }
                    cur_room_size++;
                }
                */
            }
        }

        break;
    case BODYTYPE::Frame:
        frame = ProtoUtil::ParseBodyToFrame(body, length, parseRet);
        // false 的情况已在util的函数里处理
        if (parseRet)
        {
            if (frame.sendcase() == Net::Frame_Send_All)
            {
                //std::cout << "frame.roomid" << frame.roomid() << "roomsize:" << room_user_fd_records[frame.roomid()].size() << std::endl;
                for (auto iter = room_user_fd_records[frame.roomid()].begin(); iter != room_user_fd_records[frame.roomid()].end(); ++iter)
                {
                    ssize_t sendres =  SendMsg(BODYTYPE::Frame, length, body, iter->second);
                    std::cout<<"Send:"<<sendres<<std::endl;
                    if (!sendres)
                    {
                        logout_clients.push_back(iter->first);
                    }
                }
                if (logout_clients.size() != 0)
                {
                    for (int i = 0; i < logout_clients.size(); i++)
                    {
                        room_user_fd_records[user_room_record[logout_clients[i]]].erase(logout_clients[i]);

                        if (room_user_fd_records[user_room_record[logout_clients[i]]].size() == 0)
                        {
                            std::cout << "roomclosed" << std::endl;
                            playerOptData.set_itemid(-1);
                            playerOptData.set_userid(-1);
                            playerOptData.set_roomid(user_room_record[logout_clients[i]]);
                            playerOptData.set_opt(Net::PlayerOpt::Nothing);
                            tmplength = playerOptData.ByteSizeLong();
                            tmpbody = new uint8_t[tmplength];

                            if (playerOptData.SerializeToArray(tmpbody, tmplength))
                            {
                                SendMsg(BODYTYPE::PlayerOptData, tmplength, tmpbody, logic_server_client);
                            }
                            user_room_record.erase(logout_clients[i]);
                        }
                        else
                        {
                            std::cout << "what???" << std::endl;
                            playerOptData.set_itemid(-1);
                            playerOptData.set_userid(username_userid_record[logout_clients[i]]);
                            playerOptData.set_roomid(user_room_record[logout_clients[i]]);
                            playerOptData.set_opt(Net::PlayerOpt::Exit_Room);
                            tmplength = playerOptData.ByteSizeLong();
                            tmpbody = new uint8_t[tmplength];

                            if (playerOptData.SerializeToArray(tmpbody, tmplength))
                            {
                                SendMsg(BODYTYPE::PlayerOptData, tmplength, tmpbody, logic_server_client);
                            }
                        }
                        userid_username_record.erase(username_userid_record[logout_clients[i]]);
                        username_userid_record.erase(logout_clients[i]);
                        user_fd_record.erase(logout_clients[i]);
                    }
                    logout_clients.clear();
                }
            }
            else
            {
                SendMsg(BODYTYPE::Frame, length, body, user_fd_record[userid_username_record[frame.userid()]]);
            }
            // 转发给db server处理
            // SendMsg(BODYTYPE::Frame, length, body, db_server_client);
        }

        break;
    case BODYTYPE::PlayerOptData:
    {
        playerOptData = ProtoUtil::ParseBodyToPlayerOptData(body, length, parseRet);
        int curuserid = playerOptData.userid();
        int curroomsize = playerOptData.itemid();
        std::string curusername = userid_username_record[curuserid];
        if (playerOptData.opt() == Net::PlayerOpt::Join_Room)
        {
            std::cout << "Join!" << std::endl;
            if (!roomsize_roomid_record.count(curroomsize))
            {
                // 没有符合要求的房间，所以创建房间
                roomsize_roomid_record[curroomsize] = std::set<int>();
                roomsize_roomid_record[curroomsize].insert(cur_room_id);
                roomid_roomsize_record[cur_room_id] = curroomsize;
                room_user_fd_records[cur_room_id] = std::map<std::string, int>();
                room_user_fd_records[cur_room_id][curusername] = user_fd_record[curusername]; // 添加到房间
                user_room_record[curusername] = cur_room_id;
                cur_room_id++;
            }
            else
            {
                bool flag = false; // 是否有人不齐的房间
                for (auto it = roomsize_roomid_record[curroomsize].begin(); it != roomsize_roomid_record[curroomsize].end(); ++it)
                {
                    if (room_user_fd_records[*it].size() < playerOptData.itemid())
                    {
                        room_user_fd_records[*it][curusername] = user_fd_record[curusername];
                        user_room_record[curusername] = *it;

                        flag = true;
                        break;
                    }
                }
                if (!flag)
                { // 有房间但都满了，所以要创建
                    roomsize_roomid_record[curroomsize].insert(cur_room_id);
                    roomid_roomsize_record[cur_room_id] = curroomsize;
                    room_user_fd_records[cur_room_id][curusername] = user_fd_record[curusername]; // 添加到房间
                    user_room_record[curusername] = cur_room_id;
                    cur_room_id++;
                }
            }
            room_user_fd_records[0].erase(curusername);
        }
        playerOptData.set_roomid(user_room_record[curusername]);
        // false 的情况已在util的函数里处理
        tmplength = playerOptData.ByteSizeLong();
        tmpbody = new uint8_t[tmplength];

        if (playerOptData.SerializeToArray(tmpbody, tmplength))
        {
            SendMsg(BODYTYPE::PlayerOptData, tmplength, tmpbody, logic_server_client);
        }
        if (playerOptData.opt() == Net::PlayerOpt::Exit_Room)
        {
            frame.add_opts()->CopyFrom(playerOptData);
            frame.set_sendcase(Net::Frame_Send_All);
            frame.set_index(0);
            frame.set_roomid(user_room_record[curusername]);
            tmplength = frame.ByteSizeLong();
            tmpbody = new uint8_t[tmplength];
            if (frame.SerializeToArray(tmpbody, tmplength))
            {
                SendMsg(BODYTYPE::Frame, tmplength, tmpbody, user_fd_record[curusername]);
            }



            room_user_fd_records[0][curusername] = user_fd_record[curusername]; // 添加到0房间
            room_user_fd_records[user_room_record[curusername]].erase(curusername);

            if (room_user_fd_records[user_room_record[curusername]].size() == 0 && user_room_record[curusername]!=0)
            {
                playerOptData.set_itemid(-1);
                playerOptData.set_userid(-1);
                playerOptData.set_roomid(user_room_record[curusername]);
                playerOptData.set_opt(Net::PlayerOpt::Nothing);
                tmplength = playerOptData.ByteSizeLong();
                tmpbody = new uint8_t[tmplength];
                if (playerOptData.SerializeToArray(tmpbody, tmplength))
                {
                    SendMsg(BODYTYPE::PlayerOptData, tmplength, tmpbody, logic_server_client);
                }
            }
            user_room_record[curusername] = 0;
        }
        if (room_user_fd_records[user_room_record[curusername]].size() == curroomsize)
        {
            Net::Frame startframe;
            startframe.set_roomid(user_room_record[curusername]);
            startframe.set_userid(curuserid);
            startframe.set_index(0);
            startframe.set_sendcase(Net::Frame_Send_All);
            int length = startframe.ByteSizeLong();
            uint8_t *array = new uint8_t[length];
            if (startframe.SerializeToArray(array, length))
            {
                SendMsg(BODYTYPE::Frame, length, array, logic_server_client);
            }
        }
    }
    break;
    case BODYTYPE::PlayerMessage:
    {
        playerMessage = ProtoUtil::ParseBodyToPlayerMessage(body, length, parseRet);
        int curuserid = playerMessage.userid();
        std::string curusername = userid_username_record[curuserid];
        int curroomid = user_room_record[curusername];
        playerMessage.set_roomid(curroomid);
        tmplength = playerMessage.ByteSizeLong();
        tmpbody = new uint8_t[tmplength];
        if (playerMessage.SerializeToArray(tmpbody, tmplength))
        {
            SendMsg(BODYTYPE::PlayerMessage, tmplength, tmpbody, logic_server_client);
        }
    }
    break;
    default:

        break;
    }

    FuncServer::OnMsgBodyAnalysised(head, body, length, fd);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s port\n", argv[0]);
        return 1;
    }

    int port = std::atoi(argv[1]);

    GateServer gateServer;

    printf("Start Gate Server ing...\n");
    gateServer.BootServer(port);

    return 0;
}
