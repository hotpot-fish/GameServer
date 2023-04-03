
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

    switch (type)
    {
    case BODYTYPE::LoginData:

        loginData = ProtoUtil::ParseBodyToLoginData(body, length, parseRet);

        // false 的情况已在util的函数里处理
        if (parseRet)
        {
            user_fd_record[loginData.username()] = fd;
            std::cout<<"fd:"<<fd<<std::endl;
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
            else if (loginResp.result()==0 ||loginResp.result()==-1)
            {
                // 登录失败 移除record
            }else {
                
                //记录username userid关系
                userid_username_record[loginResp.userid()] = loginResp.username();
                username_userid_record[loginResp.username()] = loginResp.userid();

                room_user_fd_records[cur_room_size][loginResp.username()] = user_fd_record[loginResp.username()]; //添加到房间
                user_room_record[loginResp.username()] = cur_room_size;

                playerOptData.set_itemid(0);
                playerOptData.set_userid(loginResp.userid());
                playerOptData.set_roomid(cur_room_size);
                playerOptData.set_opt(Net::PlayerOpt::User_Login);
                tmplength =  playerOptData.ByteSizeLong();
                tmpbody = new uint8_t[tmplength];

                if (playerOptData.SerializeToArray(tmpbody, tmplength))
                {
                    SendMsg(BODYTYPE::PlayerOptData, tmplength, tmpbody, logic_server_client);
                }

                std::cout<<"注册时"<<std::endl;
                std::cout<<"user_fd_record.size"<<user_fd_record.size()<<std::endl;
                std::cout<<"room_user_fd_records.size"<<room_user_fd_records.size()<<std::endl;  
                std::cout<<"user_room_record.size"<<user_room_record.size()<<std::endl;
                std::cout<<"userid_username_record.size"<<userid_username_record.size()<<std::endl;
                std::cout<<"username_userid_record.size"<<username_userid_record.size()<<std::endl;
                std::cout<<"logout_clients.size"<<logout_clients.size()<<std::endl;
                std::cout<<"cur_room_size:"<<cur_room_size<<std::endl;


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
            }
        }

        break;
    case BODYTYPE::Frame:
        frame = ProtoUtil::ParseBodyToFrame(body, length, parseRet);
        // false 的情况已在util的函数里处理
        if (parseRet)
        {
            if(frame.sendcase()==Net::Frame_Send_All){
                std::cout<<"frame.roomid"<<frame.roomid()<<std::endl;
                for (auto iter = room_user_fd_records[frame.roomid()].begin(); iter != room_user_fd_records[frame.roomid()].end(); ++iter) {
                    if(!SendMsg(BODYTYPE::Frame, length, body, iter->second)){
                        logout_clients.push_back(iter->first);
                    }
                }
                if(logout_clients.size()!=0){
                    for(int i = 0 ; i < logout_clients.size() ;i++){
                        room_user_fd_records[user_room_record[logout_clients[i]]].erase(logout_clients[i]);

                        if(room_user_fd_records[user_room_record[logout_clients[i]]].size()==0){
                            playerOptData.set_itemid(-1);
                            playerOptData.set_userid(-1);
                            playerOptData.set_roomid(user_room_record[logout_clients[i]]);
                            playerOptData.set_opt(Net::PlayerOpt::Nothing);
                            tmplength =  playerOptData.ByteSizeLong();
                            tmpbody = new uint8_t[tmplength];

                            if (playerOptData.SerializeToArray(tmpbody, tmplength))
                            {
                                SendMsg(BODYTYPE::PlayerOptData, tmplength, tmpbody, logic_server_client);
                            }
                            user_room_record.erase(logout_clients[i]);
                        }else{
                            playerOptData.set_itemid(-1);
                            playerOptData.set_userid(username_userid_record[logout_clients[i]]);
                            playerOptData.set_roomid(user_room_record[logout_clients[i]]);
                            playerOptData.set_opt(Net::PlayerOpt::User_Logout);
                            tmplength =  playerOptData.ByteSizeLong();
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
            }else{
                SendMsg(BODYTYPE::Frame, length, body, user_fd_record[userid_username_record[frame.userid()]]);
            }
            // 转发给db server处理
            //SendMsg(BODYTYPE::Frame, length, body, db_server_client);
        }

        break;
    case BODYTYPE::PlayerOptData:
        playerOptData = ProtoUtil::ParseBodyToPlayerOptData(body, length, parseRet);
        playerOptData.set_roomid(user_room_record[userid_username_record[playerOptData.userid()]]);
        // false 的情况已在util的函数里处理
        tmplength =  playerOptData.ByteSizeLong();
        tmpbody = new uint8_t[tmplength];

        if (playerOptData.SerializeToArray(tmpbody, tmplength))
        {
            SendMsg(BODYTYPE::PlayerOptData, tmplength, tmpbody, logic_server_client);
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

    printf("Start Gate Center Server ing...\n");
    gateServer.BootServer(port);

    return 0;
}
