#include "RakChatUser.hpp"
#include "BitStream.h"
#include "rakChat.h"
#include <mutex>

using namespace RakNet;


uint16_t RakChatUserPool::insert(const RakChatUser &user)
{
    uint16_t id = 0;

    if (!freeIds.empty())
    {
        id = freeIds.front();
        freeIds.erase(freeIds.begin());
    }
    else
    {
        id = nextId++;
    }

    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);
        connectionList_.emplace(id, std::make_unique<RakChatUser>(user));
    }
    return id;
}

bool RakChatUserPool::remove(uint16_t userid)
{
    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);
        if (auto it = connectionList_.find(userid); it != connectionList_.end())
        {
            connectionList_.erase(userid);
            freeIds.push_back(userid);
            return true;
        }
    }
    return false;
}
bool RakChatUserPool::exists(uint16_t userid)
{   
    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);
        if (auto it = connectionList_.find(userid); it != connectionList_.end())
            return true;
    }
    return false;
}
uint16_t RakChatUserPool::getId(const RakNet::RakNetGUID &guid)
{
    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);
        for (const auto& [id, user] : connectionList_)
        {
            if(user->userGUID == guid)
            {
                return id;
            }
        }
    }
    return 0;
}
const std::string& RakChatUserPool::getName(const RakNet::RakNetGUID &guid) const
{
    static const std::string ret = "null";
    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);
        for (const auto& [id, user] : connectionList_)
        {
            if(user->userGUID == guid)
            {
                return user->Name;
            }
        }
    }
    return ret;
}

RakChatUser* RakChatUserPool::get(uint16_t userid)
{
    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);
        if (auto it = connectionList_.find(userid); it != connectionList_.end())
        {
            return it->second.get();
        }
    }
    return nullptr;
}

RakChatUser* RakChatUserPool::get(const RakNet::RakNetGUID& guid)
{
    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);
        for (const auto& [id, user] : connectionList_)
        {
            if (user->userGUID == guid)
            return user.get();
        }
    }
    return nullptr;
}

RakChatUser* RakChatUserPool::get(const RakNet::SystemAddress& systemAddress)
{
    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);
        for (auto& [id, user] : connectionList_)
        {
            if(user->userAddr == systemAddress)
            return user.get();
        }
    
    }
    return nullptr;
}

RakChatUser* RakChatUserPool::get(const std::string& userName)
{
    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);

        for (auto& [id, user] : connectionList_)
        {
            if(user->Name == userName)
            return user.get();
        }
    }
    return nullptr;
}

const RakChatUser* RakChatUserPool::get(uint16_t userid) const
{
    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);
        if (auto it = connectionList_.find(userid); it != connectionList_.end())
        {
            return it->second.get();
        }
    }
    return nullptr;
}

const RakChatUser* RakChatUserPool::get(const RakNet::RakNetGUID& guid) const
{
    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);
        for (auto& [id, user] : connectionList_)
        {
            if (user->userGUID == guid)
                return user.get();
        }
    }
    return nullptr;
}

const RakChatUser* RakChatUserPool::get(const RakNet::SystemAddress& systemAddress) const
{
    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);
        for (auto& [id, user] : connectionList_)
        {
            if(user->userAddr == systemAddress)
                return user.get();
        }
    }
    return nullptr;
}

const RakChatUser* RakChatUserPool::get(const std::string& userName) const
{
    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);
        for (auto& [id, user] : connectionList_)
        {
            if(user->Name == userName)
                return user.get();
        }
    }
    return nullptr;
}


void RakChatUserPool::BroadcastSystemMessage(const char* message, const RakNetGUID& exclude)
{
    std::vector<uint16_t> peers;
    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);
        for (const auto& [id, _] : connectionList_)
        {
            peers.push_back(id);
        }
    }
    
    for (uint16_t id : peers)
    {
        RakChatUser* user = this->get(id);
        if (user == nullptr) continue;
        if (user->userGUID != exclude)
            user->PushSystemMessage(message);
    }
}

void RakChatUserPool::BroadcastBitStream(const BitStream* bs, const RakNetGUID& exclude)
{
    std::vector<uint16_t> peers;
    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);
        for (const auto& [id, _] : connectionList_)
        {
            peers.push_back(id);
        }
    }
    
    for (uint16_t id : peers)
    {
        RakChatUser* user = this->get(id);
        if (user == nullptr) continue;
        if (user->userGUID != exclude)
            user->SendBitStream(bs);
    }
}

void RakChatUser::RPCCall(const char* identifier, const BitStream* bs) const
{
    remote->Signal(identifier, const_cast<BitStream*>(bs), HIGH_PRIORITY, RELIABLE_ORDERED, 0, this->userAddr, false, false);
}
void RakChatUser::RPCCall(const char* identifier, const BitStream* bs, PacketPriority priority, PacketReliability reliability, char orderingChannel, RakChatUser* exclude) const
{
    if (this->userGUID == exclude->userGUID) return;
    remote->Signal(identifier, const_cast<BitStream*>(bs), priority, reliability, orderingChannel, this->userAddr, false, false);
}

void RakChatUserPool::BroadcastRPCCall(const char* identifier, const BitStream* bs, const RakNetGUID& exclude)
{
    std::vector<uint16_t> peers;
    {
        std::lock_guard<std::mutex> lock(connectionList_mutex);
        for (const auto& [id, _] : connectionList_)
        {
            peers.push_back(id);
        }
    }
    
    for (uint16_t id : peers)
    {
        RakChatUser* user = this->get(id);
        if (user == nullptr) continue;
        if (user->userGUID != exclude)
            user->RPCCall(identifier, bs);
    }
}

RakChatUser::RakChatUser(RakPeerInterface* peerInstance, RPC4* RPCinstance)
{
    printf("peer: %p", peerInstance);
    printf("rpc4: %p", RPCinstance);
    peer = peerInstance;
    remote = RPCinstance;
}

void RakChatUser::SendBitStream(const BitStream* bs) const
{
    if (!peer) return;
    peer->Send(bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, this->userAddr, false);
}
void RakChatUser::SendBitStream(const BitStream* bs, PacketPriority priority, PacketReliability reliability, char orderingChannel, RakChatUser* exclude) const
{
    if (this->userGUID == exclude->userGUID) return;
    if (!peer) return;
    peer->Send(bs, priority, reliability, orderingChannel, this->userAddr, false);
}


void RakChatUser::PushSystemMessage(const char* message) const
{
    BitStream bs = BitStream();
    RakString msg(message);
    bs.Write(static_cast<RakNet::MessageID>(ID_SYSTEM_MESSAGE));
    bs.Write(msg);
    peer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, this->userAddr, false);
}