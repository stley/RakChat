#pragma once
#include <unordered_set>
#include <vector>
#include <string>
#include <cstdint>
#include "RakNetTypes.h"
#include "PacketPriority.h"
#include "BitStream.h"
#include <mutex>

#include <unordered_map>

enum LeaveTypes : uint8_t
{
    LEAVE_UNDEFINED = 0x00,
    LEAVE_GRACEFULLY,
    LEAVE_KICKED,
    LEAVE_DROP
};

class RakChatUser;

using namespace RakNet;

class RakChatChannel
{
    
private:

    std::string name_;
    std::string password_ = "nil";
    std::unordered_set<RakChatUser*> users_;
    mutable std::mutex pSetMutex;

    uint32_t capacity_ = 0;
    RakChatChannel* channel_parent = nullptr; // nullptr means it is not a subchannel
public:
    RakChatChannel(const char* name, RakChatChannel* parent = nullptr, const char* password = nullptr, uint32_t capacity = 0);
    ~RakChatChannel();
    RakChatChannel* GetParent() const  { return channel_parent; }
    const std::string& Name() const { return name_; }
    //uint32_t PeerCount() { return users_.size(); }
    bool JoinChannel(RakChatUser* user);
    bool LeaveChannel(RakChatUser* user, uint8_t type);
    bool IsUserInChannel(const RakChatUser* user) const { return users_.find(const_cast<RakChatUser*>(user)) != users_.end(); }

    void Broadcast(const BitStream* bs) const;

    void Broadcast(const BitStream* bs, PacketPriority priority, PacketReliability reliability, char orderingChannel, RakChatUser* from) const;

    std::unordered_set<RakChatUser*>& GetChannelPeerList() { return users_; }
};


class ChannelPool
{
private:
    uint16_t nextId = 0;
    std::vector<uint16_t> freeIds;
    std::unordered_map<uint16_t, RakChatChannel> channels_;
public:
    [[nodiscard]] const std::unordered_map<uint16_t, RakChatChannel>& GetList() { return channels_; }
    uint16_t CreateChannel(const char* name, RakChatChannel* parent, const char* password, uint32_t capacity);
    bool DeleteChannel(uint16_t channel_id);
    uint16_t toID(RakChatChannel* chanPtr) const
    {
        if (!chanPtr) return 0;
        for (const auto& [cid, chan] : channels_)
        {
            if (chanPtr == &chan) return cid;
        }
        return 0;
    }

    RakChatChannel* IsUserInAnyChannel(const RakChatUser* user)
    {
        for (auto& [cid, chan] : channels_)
        {
            if (chan.IsUserInChannel(user)) return &chan;
        }
        return nullptr;
    }
    const RakChatChannel* IsUserInAnyChannel(const RakChatUser* user) const
    {
        for (const auto& [cid, chan] : channels_)
        {
            if (chan.IsUserInChannel(user)) return &chan;
        }
        return nullptr;
    }
    [[nodiscard]] const RakChatChannel* GetChannel(uint16_t channel_id) const;
    [[nodiscard]] RakChatChannel* GetChannel(uint16_t channel_id);
};




