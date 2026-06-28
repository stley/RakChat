#include <iostream>
#include <thread>
#include <string>
#include <queue>
#include <mutex>
#include <rakChat.h>
#include "SpeakEngine.hpp"
#include "SoundDevice.hpp"
#include <atomic>

extern std::atomic<bool> clientInit;


using namespace RakNet;


struct ConnectionConfig
{
    std::string serverIp;
    unsigned short serverPort = 65535;
    std::string userName;
};

enum MessageTypes : uint8_t
{
    SPK_MESSAGE_UNDEFINED = 0x00,
    SPK_SYSTEM_MESSAGE,
    SPK_CHAT_MESSAGE
};

struct ChatMessage
{
    uint8_t message_type = SPK_MESSAGE_UNDEFINED;
    std::string messageAuthor;
    std::string messageContent;
};

enum ClientState : uint8_t
{
    Init = 0x00,
    Connecting,
    Registering,
    RegistrationFailed,
    FailedConnection,
    Connected
};


class RakChatClient
{
private:
    std::queue<ChatMessage> MessageQueue;
protected:
    ConnectionConfig config_;
    RakNet::RakPeerInterface *peer;
    SocketDescriptor sd;
    std::thread workerThread;
    std::atomic<bool> running_ = false;
    std::atomic<bool> connected_ = false;
    std::mutex queueMutex;
    SystemAddress serverAddress = UNASSIGNED_SYSTEM_ADDRESS;

    ClientState state_ = Init;

    //Voice
    SpeakeasyEngine* voiceEngine = nullptr;

    virtual void ClientThread();
    virtual void ProcessSlashCommand(const std::string& cmdtext);

public:
    RakChatClient();
    ~RakChatClient();
    ClientState GetClientState() { return state_; }
    virtual void SetClientState(ClientState newState) { state_ = newState; }
    virtual void ClientMain();
};