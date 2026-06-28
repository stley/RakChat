#include "RakChatChannel.hpp"
#include "RakChatUser.hpp"
#include <iostream>
#include <algorithm>


using namespace RakNet;



RakChatChannel::RakChatChannel(const char* name, RakChatChannel* parent, const char* password, uint32_t capacity)
{
    name_ = name;
    capacity_ = capacity;
    if (parent)
        channel_parent = parent;
    if (password)
        password_ = password;
}


RakChatChannel::~RakChatChannel()
{
    if (!users_.empty())
    {
        users_.clear();
    }
}


bool RakChatChannel::JoinChannel(RakChatUser* user)
{
    printf("Patching %s to channel %s.\n", user->Name.c_str(), this->Name().c_str());
    if (!IsUserInChannel(user))
    {
        {
            std::lock_guard<std::mutex> lock(pSetMutex);
            users_.insert(user);
        }
        std::string msg = "You joined channel \"";
        msg += name_.c_str();
        msg += "\".";
        user->PushSystemMessage(msg.c_str());
        return true;
    }
    return false;
}

bool RakChatChannel::LeaveChannel(RakChatUser* user, uint8_t type)
{
    printf("Dropping %s from channel %s.\n", user->Name.c_str(), this->Name().c_str());
    if (IsUserInChannel(user))
    {
        {
            std::lock_guard<std::mutex> lock(pSetMutex);
            users_.erase(user);
        }

        if ( type == LEAVE_DROP )
            return true;

        std::string msg = "You left channel \"";
        msg += name_.c_str();
        msg += "\".";
        user->PushSystemMessage(msg.c_str());
        return true;
    }
    return false;
}

void RakChatChannel::Broadcast(const BitStream* bs) const
{
    std::unordered_set<RakChatUser*> snapshot;
    {
        std::lock_guard<std::mutex> lock(pSetMutex);
        snapshot = users_;
    }
    for (const auto usr : snapshot )
    {
        usr->SendBitStream(bs);
    }
}

void RakChatChannel::Broadcast(const BitStream* bs, PacketPriority priority, PacketReliability reliability, char orderingChannel, RakChatUser* from) const
{
    std::unordered_set<RakChatUser*> snapshot;
    {
        std::lock_guard<std::mutex> lock(pSetMutex);
        snapshot = users_;
    }
    for (const auto usr : snapshot )
    {
        usr->SendBitStream(bs, priority, reliability, orderingChannel, from);
    }
}

uint16_t ChannelPool::CreateChannel(const char* name, RakChatChannel* parent, const char* password, uint32_t capacity)
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

    channels_.emplace(std::piecewise_construct, std::forward_as_tuple(id), std::forward_as_tuple(name, parent, password, capacity));

    return id;
}

bool ChannelPool::DeleteChannel(uint16_t channel_id)
{

    if (auto it = channels_.find(channel_id); it != channels_.end())
    {
        channels_.erase(channel_id);
        freeIds.push_back(channel_id);
        return true;
    }
    return false;
}


RakChatChannel* ChannelPool::GetChannel(uint16_t channel_id)
{
    if (auto it = channels_.find(channel_id); it != channels_.end())
    {
        return &it->second;
    }
    return nullptr;
}

const RakChatChannel* ChannelPool::GetChannel(uint16_t channel_id) const
{
    if (auto it = channels_.find(channel_id); it != channels_.end())
    {
        return &it->second;
    }
    return nullptr;
}
