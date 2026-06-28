#pragma once
#include "RakNetTypes.h"
#include "PacketPriority.h"
#include "RPC4Plugin.h"
#include <utility>
#include <mutex>
#include <unordered_map>
#include <vector> 
#include <string>
using namespace RakNet;

struct RakChatUser
{
    RakChatUser(RakPeerInterface* peer, RPC4* RPCinstance);
    std::string Name;
    RakNet::SystemAddress userAddr;
    RakNet::RakNetGUID userGUID;
    bool operator==(const RakChatUser& user) const
    {
        if (this->Name != user.Name) return false;
        if (this->userAddr != user.userAddr) return false;
        if (this->userGUID != user.userGUID) return false;
        return true;
    }
    void PushSystemMessage(const char* message) const;
    void SendBitStream(const BitStream* bs) const;
    void SendBitStream(const BitStream* bs, PacketPriority priority, PacketReliability reliability, char orderingChannel, RakChatUser* exclude) const;
    void RPCCall(const char* identifier, const BitStream* bs) const;
    void RPCCall(const char* identifier, const BitStream* bs, PacketPriority priority, PacketReliability reliability, char orderingChannel, RakChatUser* exclude) const;
private:
    RakPeerInterface* peer = nullptr;
    RPC4* remote = nullptr;
};


class RakChatUserPool
{
public:
    RakChatUserPool() = default;
    ~RakChatUserPool() = default;
    uint16_t insert(const RakChatUser& user);
    bool remove(uint16_t userid);
    bool exists(uint16_t userid);
    uint16_t getId(const RakNet::RakNetGUID& guid);

    const std::string& getName(const RakNet::RakNetGUID& guid) const;
    const RakChatUser* get(const std::string& userName) const;
    const RakChatUser* get(uint16_t userid) const;
    const RakChatUser* get(const RakNet::RakNetGUID& guid) const;
    const RakChatUser* get(const RakNet::SystemAddress& systemAddress) const;
    RakChatUser* get(const std::string& userName);
    RakChatUser* get(uint16_t userid);
    RakChatUser* get(const RakNet::RakNetGUID& guid);
    RakChatUser* get(const RakNet::SystemAddress& systemAddress);
    void BroadcastSystemMessage(const char* message, const RakNetGUID& exclude = UNASSIGNED_RAKNET_GUID);
    void BroadcastBitStream(const BitStream* bs, const RakNetGUID& exclude = UNASSIGNED_RAKNET_GUID);
    void BroadcastRPCCall(const char* identifier, const BitStream* bs, const RakNetGUID& exclude);
    
    [[nodiscard]] std::vector<std::pair<uint16_t, RakChatUser*>> GetPeerList() const 
    {
      std::lock_guard<std::mutex> lock(connectionList_mutex);
      std::vector<std::pair<uint16_t, RakChatUser*>> list;
      list.reserve(connectionList_.size());
      for (const auto& [id, user] : connectionList_)
          list.emplace_back(id, user.get());
      return list;
    }
private:
    uint16_t nextId = 1;
    std::vector<uint16_t> freeIds;
    std::unordered_map<uint16_t, std::unique_ptr<RakChatUser>> connectionList_;
    mutable std::mutex connectionList_mutex;
};
