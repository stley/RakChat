#include <iostream>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>

#define RKC_MAX_CLIENTS (10)

#include <rakChat.h>
#include "RakSleep.h"
#include "RakChatUser.hpp"
#include "RakChatChannel.hpp"
#include "RPC4Plugin.h"

using namespace RakNet;

class RakChatServer
{
private:
    RakPeerInterface* peer;
    SocketDescriptor sd;
    std::thread workerThread;
    std::atomic<bool> isServerRunning = false;

    RPC4 rpc4;
    
    //RakNet::Packet *packet;
    
    
    std::mutex listMutex;

    void MainThread();

    void HandlePacket(Packet *data);

    bool isNameAvailable(const char* name, size_t len);

    bool isGuidRegistered(RakNetGUID guid_);

    void DropUser(RakChatUser* usr);
    

    RakChatUserPool userPool;
    ChannelPool channelPool;
    RakChatChannel* rootPtr = nullptr;

    void ProcessSlashCommand(const std::string& cmdtext, RakChatUser* issuer);
public:

    RakChatServer();
    ~RakChatServer();
    int Init();
    void Stop();
};

