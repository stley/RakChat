#include "server.hpp"
#include <algorithm>
#include <sstream>


RakChatServer::RakChatServer() {}
int RakChatServer::Init()
{
    peer = RakPeerInterface::GetInstance();
    this->sd = SocketDescriptor(50000, 0);
    auto res = peer->Startup(RKC_MAX_CLIENTS, &sd, 1);
    peer->SetMaximumIncomingConnections(RKC_MAX_CLIENTS);
    std::cout << "RakChat Server started on port 60000." << "(" << res << ")" << "\n";
    assert(res == RAKNET_STARTED);
    isServerRunning = true;
    workerThread = std::thread(&RakChatServer::MainThread, this);

    peer->AttachPlugin(&rpc4);

    uint16_t rootId = channelPool.CreateChannel("Root", nullptr, nullptr, 0);
    rootPtr = channelPool.GetChannel(rootId);

    uint16_t testId = channelPool.CreateChannel("Test0", nullptr, nullptr, 0);
    testId = channelPool.CreateChannel("Test1", nullptr, nullptr, 0);
    testId = channelPool.CreateChannel("Test2", nullptr, nullptr, 0);
    testId = channelPool.CreateChannel("SubRoot", rootPtr, nullptr, 0);
    while (isServerRunning)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return 1;
}

void RakChatServer::Stop()
{
    isServerRunning = false;
    if(workerThread.joinable())
    {
        workerThread.join();
    }
}

RakChatServer::~RakChatServer()
{
    Stop();
    RakNet::RakPeerInterface::DestroyInstance(peer);
}


bool RakChatServer::isNameAvailable(const char* name, size_t len)
{   
    std::string getName(name, len);
    if (userPool.get(getName) != nullptr) return false;
    return true;
}

bool RakChatServer::isGuidRegistered(RakNetGUID guid_)
{
    RakChatUser* usr = const_cast<RakChatUser*>(userPool.get(guid_));
    if (usr != nullptr) return true;
    return false;
}

void RakChatServer::DropUser(RakChatUser* user)
{
    RakChatChannel* oldChannel = channelPool.IsUserInAnyChannel(user);
    if (oldChannel)
        oldChannel->LeaveChannel(user, LEAVE_DROP);

    uint16_t quitter = userPool.getId(user->userGUID);

    userPool.remove(quitter);

    BitStream bs;
    bs.Write((RakNet::MessageID)ID_USER_UPDATE);
    bs.Write((unsigned char)'Q');
    bs.Write(quitter);
    userPool.BroadcastBitStream(&bs);
}

void RakChatServer::HandlePacket(Packet *packet)
{
    switch(packet->data[0])
    {
        case ID_REGISTER_ME:
        {
            BitStream reg_bs = BitStream(packet->data, packet->length, false);
            reg_bs.IgnoreBytes(sizeof(RakNet::MessageID));
            RakString name;
            reg_bs.Read(name);
            BitStream response = BitStream();
            if(!isNameAvailable(name.C_String(), name.GetLength()))
            {
                
                response.Write((RakNet::MessageID)ID_REGISTER_ME);
                response.Write((unsigned char)'O');
                peer->Send(&response, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
            }
            else
            {
                RakChatUser theUser(peer, &rpc4);
                theUser.userAddr = packet->systemAddress;
                theUser.userGUID = packet->guid;
                theUser.Name = name.C_String();
                uint16_t newId = userPool.insert(theUser);
                
                response.Write((RakNet::MessageID)ID_REGISTER_ME);
                response.Write((unsigned char)'Y');
                response.Write(newId);
                peer->Send(&response, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

                char buf[9];
                std::sprintf(buf, "(%d) ", newId);
                std::string system_message = buf;
                system_message+= name.C_String();
                system_message+= " connected to the server.";

                userPool.BroadcastSystemMessage(system_message.c_str(), packet->guid);

                rootPtr->JoinChannel(const_cast<RakChatUser*>(userPool.get(newId)));

                
                printf("Building tree for client...\n");
                for (const auto& [cid, rcchan] : channelPool.GetList())
                {
                    BitStream bs;
                    bs.Write(cid);
                    bs.Write((rcchan.GetParent() != nullptr)); //has parent
                    bs.Write(channelPool.toID(rcchan.GetParent()));
                    RakString chanName(rcchan.Name().c_str());
                    bs.Write(chanName);
                    printf("%d | %d | %s\n", cid, channelPool.toID(rcchan.GetParent()), rcchan.Name().c_str());
                    rpc4.Signal("ChannelInfo", &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false, false);
                    bs.Reset();
                }
                for (const auto& [uid, rcuser] : userPool.GetPeerList())
                {
                    BitStream bs;
                    uint16_t userchannel = channelPool.toID(channelPool.IsUserInAnyChannel(rcuser));
                    bs.Write(uid); 
                    bs.Write(userchannel);
                    RakString nome(rcuser->Name.c_str());
                    bs.Write(nome);
                    rpc4.Signal("UserInfo", &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false, false);
                    bs.Reset();
                }
                BitStream bs;
                bs.Write(newId);
                bs.Write(static_cast<uint16_t>(0));
                bs.Write(RakString(theUser.Name.c_str()));
                printf("Sending user: %d %d %s\n", newId, 0, theUser.Name.c_str());
                userPool.BroadcastRPCCall("UserInfo", &bs, packet->guid);
                
            }
            break;
        }
            
		case ID_NEW_INCOMING_CONNECTION:
        {
            printf("A connection is incoming.\n");
            break;
        }
            
		case ID_DISCONNECTION_NOTIFICATION:
        {
            printf("A client has disconnected.\n");
            RakChatUser* user = const_cast<RakChatUser*>(userPool.get(packet->guid));
            if (user != nullptr)
            {
                //BitStream announce = BitStream();
                //announce.Write((RakNet::MessageID)ID_SYSTEM_MESSAGE);
                char buf[9];
                std::sprintf(buf, "(%d) ", userPool.getId(packet->guid));
                std::string system_message = buf;
                system_message += user->Name.c_str();
                system_message += " disconnected from the server.";
                //announce.Write(system_message.c_str());
                /*userPool.BroadcastSystemMessage(system_message.c_str(), packet->guid);
                //peer->Send(&announce, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_SYSTEM_ADDRESS, true);
                userPool.remove( userPool.getId(packet->guid) );*/
                this->DropUser(user);
                userPool.BroadcastSystemMessage(system_message.c_str(), packet->guid);
            }
        }
			break;
		case ID_CONNECTION_LOST:
        {
			printf("A client lost connection.\n");
            if (!isGuidRegistered(packet->guid)) break;
            
            RakChatUser* user = const_cast<RakChatUser*>(userPool.get(packet->guid));
            if (user != nullptr)
            {
                //BitStream announce = BitStream();
                //announce.Write((RakNet::MessageID)ID_SYSTEM_MESSAGE);
                char buf[9];
                std::sprintf(buf, "(%d) ", userPool.getId(packet->guid));
                std::string system_message = buf;
                system_message+= user->Name.c_str();
                system_message+= " lost connection to the server.";
                //announce.Write(system_message.c_str());
                RakChatChannel* oldChannel = channelPool.IsUserInAnyChannel(user);
                

                if (oldChannel)
                    oldChannel->LeaveChannel(user, LEAVE_DROP);

                userPool.remove( userPool.getId(packet->guid) );
                userPool.BroadcastSystemMessage(system_message.c_str(), packet->guid);
                //peer->Send(&announce, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_SYSTEM_ADDRESS, true);
                
                
            }
            break;
        }
			
        case ID_CHAT_MESSAGE:
        {
            if (!isGuidRegistered(packet->guid)) break;
		    RakChatChannel* chan = channelPool.IsUserInAnyChannel(userPool.get(packet->guid));
            if (!chan) break;

            BitStream bsIn = BitStream(packet->data, packet->length, false);
            bsIn.IgnoreBytes(sizeof(RakNet::MessageID));
            RakString message;
            bsIn.Read(message);

            BitStream bsOut = BitStream();
            bsOut.Write((RakNet::MessageID)ID_CHAT_MESSAGE);
            RakString author(userPool.getName(packet->guid).c_str());
            bsOut.Write(author);
            bsOut.Write(message);
            
            chan->Broadcast(&bsOut);

            //peer->Send(&bsOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_SYSTEM_ADDRESS, true);
            break;
        }
            

        case ID_NO_FREE_INCOMING_CONNECTIONS:
		    printf("The server is full.\n");
			break;

        case ID_VOICE_DATA:
        {
            if (!isGuidRegistered(packet->guid)) break;
            RakChatUser* theUser = const_cast<RakChatUser*>(userPool.get(packet->guid));
            RakChatChannel* chan = channelPool.IsUserInAnyChannel(theUser);
            uint16_t len = 0;
            BitStream bsIn = BitStream(packet->data, packet->length, false);
            bsIn.IgnoreBytes(sizeof(RakNet::MessageID));
            bsIn.Read(len);
            if(len)
            {
                uint8_t __data[512];
                bsIn.Read(reinterpret_cast<char*>(__data), static_cast<unsigned int>(len));

                /*
                ID_VOICE_DATA
                USER ID, UNSIGNED 16 BIT INT
                LENGTH OF BUFFER
                NAME OF SPEAKER
                ACTUAL VOICE DATA
                */

                BitStream bsOut = BitStream();
                bsOut.Write((RakNet::MessageID)ID_VOICE_DATA);
                uint16_t uID = userPool.getId(packet->guid);
                bsOut.Write(uID);
                bsOut.Write(len);
                RakString rsName(theUser->Name.c_str());
                bsOut.Write( rsName );
                bsOut.Write(reinterpret_cast<const char*>(__data), (uint16_t)len);
                chan->Broadcast(&bsOut, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 1, theUser);
                //peer->Send(&bsOut, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 1, packet->systemAddress, true);
            }
            break;
        }

        case ID_QUERY:
        {
            break;
            RakChatUser* theUser = const_cast<RakChatUser*>(userPool.get(packet->guid));
            if (!theUser)
                break;

            BitStream bsIn = BitStream(packet->data, packet->length, false);
            bsIn.IgnoreBytes(sizeof(RakNet::MessageID));
            unsigned char result;
            break;
        }

        case ID_COMMAND:
        {
            RakChatUser* theUser = const_cast<RakChatUser*>(userPool.get(packet->guid));
            if (!theUser)
                break;
            BitStream bsIn = BitStream(packet->data, packet->length, false);
            bsIn.IgnoreBytes(sizeof(RakNet::MessageID));
            RakString rsCMD;
            bsIn.Read(rsCMD);
            std::string cmdtext = rsCMD.C_String();
            ProcessSlashCommand(cmdtext, theUser);
            break;
        }
        case ID_CHANNEL_ACTION:
        {
            RakChatUser* theUser = const_cast<RakChatUser*>(userPool.get(packet->guid));
            if (!theUser)
                break;
            BitStream bsIn = BitStream(packet->data, packet->length, false);
            bsIn.IgnoreBytes(sizeof(RakNet::MessageID));
            unsigned char option;
            bsIn.Read(option);
            switch (option)
            {
                case 'J':
                {
                    uint16_t IdToJoin = 0;
                    bsIn.Read(IdToJoin);

                    RakChatChannel* oldChan = channelPool.IsUserInAnyChannel(theUser);
                    RakChatChannel* newChan = channelPool.GetChannel(IdToJoin);

                    if (!newChan)
                    {
                        theUser->PushSystemMessage("Invalid channel ID.");
                        return;
                    }
                    if (oldChan)
                        oldChan->LeaveChannel(theUser, LEAVE_GRACEFULLY);
                    newChan->JoinChannel(theUser);
                    BitStream announce;
                    announce.Write((RakNet::MessageID)ID_USER_UPDATE);
                    announce.Write((unsigned char)'J');
                    announce.Write(userPool.getId(theUser->userGUID));
                    announce.Write(IdToJoin);

                    userPool.BroadcastBitStream(&announce, UNASSIGNED_RAKNET_GUID);
                        
                    break;
                }
            }
            

        }
        break;
    }
}

void RakChatServer::ProcessSlashCommand(const std::string& cmdtext, RakChatUser* issuer)
{
    if (cmdtext.find("/peerlist") == 0) 
    {
        BitStream peerBS = BitStream();
        peerBS.Write((RakNet::MessageID)ID_SYSTEM_MESSAGE);
        RakString rs = RakString("List of connected peers:");
        peerBS.Write(rs);
        issuer->SendBitStream(&peerBS);
        
        for (const auto& [ uid, chatUser ] : userPool.GetPeerList())
        {
            peerBS.Reset();
            peerBS.Write((RakNet::MessageID)ID_SYSTEM_MESSAGE);
            rs = RakString("(%d) - %s", uid, chatUser->Name.c_str());
            peerBS.Write(rs);
            issuer->SendBitStream(&peerBS);
        }         
    }

    else if (cmdtext.find("/channellist") == 0)
    {
        BitStream peerBS = BitStream();
        peerBS.Write((RakNet::MessageID)ID_SYSTEM_MESSAGE);
        RakString rs = RakString("List of available channels:");
        peerBS.Write(rs);
        issuer->SendBitStream(&peerBS);
        for (const auto& [chanid, chan] : channelPool.GetList())
        {
            peerBS.Reset();
            peerBS.Write((RakNet::MessageID)ID_SYSTEM_MESSAGE);
            rs = RakString("(%d) - %s", chanid, chan.Name().c_str());
            peerBS.Write(rs);
            issuer->SendBitStream(&peerBS);
        }
    }

    else if (cmdtext.find("/joinchannel") == 0)
    {
        std::istringstream iss(cmdtext);

        std::string command;
        uint16_t IdToJoin;
        
        iss >> command >> IdToJoin;

        if (iss.fail())
        {
            issuer->PushSystemMessage("Usage: /joinchannel <Channel ID>");
            return;
        }

        IdToJoin = std::clamp(IdToJoin, static_cast<uint16_t>(0), static_cast<uint16_t>(65535));


        RakChatChannel* oldChan = channelPool.IsUserInAnyChannel(issuer);
        RakChatChannel* newChan = channelPool.GetChannel(IdToJoin);

        if (newChan)
        {   if (oldChan)
                oldChan->LeaveChannel(issuer, LEAVE_GRACEFULLY);
            newChan->JoinChannel(issuer);
        }
        else
            issuer->PushSystemMessage("Invalid channel ID.");

        return;
    }
}

void RakChatServer::MainThread()
{   
    printf("Thread is on\n");
    while(isServerRunning)
    {
        bool working = false;
        for (auto* packet = peer->Receive(); packet; peer->DeallocatePacket(packet), packet=peer->Receive())
		{
            working = true;
            HandlePacket(packet);
		}
        if (!working)
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
    }
}

#ifdef _WIN32
    #include <windows.h>
    #include <dbghelp.h>
    #include <minidumpapiset.h>
    #pragma comment(lib, "Dbghelp.lib")

    LONG WINAPI oopsieHandler(EXCEPTION_POINTERS* info)
    {
        HANDLE file = CreateFileA("server.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (file != INVALID_HANDLE_VALUE)
        {
            MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
            dumpInfo.ThreadId = GetCurrentThreadId();
            dumpInfo.ExceptionPointers = info;
            dumpInfo.ClientPointers = FALSE;

            MiniDumpWriteDump(
                GetCurrentProcess(),
                GetCurrentProcessId(),
                file,
                MiniDumpWithFullMemory,
                &dumpInfo,
                NULL,
                NULL
            );

            CloseHandle(file);
        }

        printf("Crash\n");
        printf("Exception 0x%X\n", info->ExceptionRecord->ExceptionCode);
        printf("Address: %p\n", info->ExceptionRecord->ExceptionAddress);

        return EXCEPTION_EXECUTE_HANDLER;
    }
#endif



int main()
{
    #ifdef _WIN32
        SetUnhandledExceptionFilter(oopsieHandler);
    #endif
    RakChatServer theServer;
    return theServer.Init();
}