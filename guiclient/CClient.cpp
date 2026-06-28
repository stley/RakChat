#include "CClient_impl.hpp"
#include <algorithm>
#include <sstream>

static ExtendedClient* clientPtr = nullptr;


ExtendedClient::ExtendedClient()
{
    peer = RakNet::RakPeerInterface::GetInstance();
    peer->Startup(1, &sd, 1);
    peer->AttachPlugin(&rpc4);
    clientPtr = this;
    rpcRegister();
}
ExtendedClient::~ExtendedClient()
{
    running_ = false;
    connected_ = false;
    
    if(workerThread.joinable())
    {
        workerThread.join();
    }
    peer->Shutdown(300);
    RakNet::RakPeerInterface::DestroyInstance(peer);
}

void rpcUserInfo(RakNet::BitStream* userData, RakNet::Packet* packet)
{
    if (!clientPtr)
    {
        printf("invalid client pointer"); 
        return;
    }

    uint16_t user_id;
    uint16_t user_channel;
    RakString user_name;
    userData->Read(user_id);
    userData->Read(user_channel);
    userData->Read(user_name);

    printf("PushUser: %d %d %s\n", user_id, user_channel, user_name.C_String());

    clientPtr->PushUser(user_id, user_channel, user_name.C_String());
}
void rpcChannelInfo(RakNet::BitStream* userData, RakNet::Packet* packet)
{
    if (!clientPtr)
    {
        printf("invalid client pointer"); 
        return;
    }

    uint16_t channel_id;
    bool hasParent = false;
    uint16_t channel_parent;
    RakString channel_name;
    userData->Read(channel_id);
    userData->Read(hasParent);
    userData->Read(channel_parent);
    userData->Read(channel_name);

    printf("PushChannel: %d %d %s\n", channel_id, channel_parent, channel_name.C_String());

    clientPtr->PushChannel(channel_id, hasParent, channel_parent, channel_name.C_String());
}

void ExtendedClient::rpcRegister()
{
    rpc4.RegisterSlot("ChannelInfo", &rpcChannelInfo, 1);
    rpc4.RegisterSlot("UserInfo", &rpcUserInfo, 1);
}

void ExtendedClient::ClientConfigure(const char* ip, unsigned short port, const char* username)
{
    config_.serverIp = ip;
    config_.serverPort = std::clamp(port, (unsigned short)0, (unsigned short)65534);
    config_.userName = username;
    std::cout << "INPUT: " << config_.serverIp << " " << config_.serverPort << " " << config_.userName << " " << "\n";
    return;
}
void ExtendedClient::ClientConnect()
{
    std::cout << "Connecting\n";
    SetClientState(Connecting);
    peer->Connect(config_.serverIp.c_str(), config_.serverPort, nullptr, 0);
    std::cout << "peer->Connect()\n";
    running_ = true;
    if (workerThread.joinable())
        workerThread.join();
    workerThread = std::thread(&ExtendedClient::ClientThread, this);
    return;
}

void ExtendedClient::ClientThread()
{
    std::cout << "Packet thread: It's on! :)\n";
    while(running_)
    {
        bool working = false;
        for (auto* packet=peer->Receive(); packet; peer->DeallocatePacket(packet), packet=peer->Receive())
		{
            working = true;   
			switch (packet->data[0])
			{
                case ID_REGISTER_ME:
                {
                    BitStream bs = BitStream(packet->data, packet->length, false);
                    bs.IgnoreBytes(sizeof(RakNet::MessageID));
                    unsigned char result;
                    bs.Read(result);
                    uint16_t myId = 0;
                    bs.Read(myId);
                    if (result == 'Y')
                    {
                        state_ = Connected;
                        ConsolePrint("You registered as %s (%d).", config_.userName.c_str(), myId);
                        ConsolePrint(" Now you can send messages.");
                        voiceEngine = new SpeakeasyEngine(peer);
                        this->connected_ = true;
                    }
                    else if (result == 'O')
                    {
                        state_ = RegistrationFailed;
                        ConsolePrint("Registration failed because name is in use.\n");
                        running_ = false;
    
                        break;
                    }
                    else
                    {
                        state_ = RegistrationFailed;
                        ConsolePrint("Registration failed.");
                        running_ = false;
    
                        break;
                    }
                    break;
                }
                        
                case ID_CONNECTION_REQUEST_ACCEPTED:
				{
					ConsolePrint("You are connected! Registering you...");
                    state_ = Registering;
                    serverAddress = peer->GetSystemAddressFromIndex(0);
                    BitStream bs = BitStream();
                    bs.Write(static_cast<RakNet::MessageID>(ID_REGISTER_ME));
                    bs.Write(config_.userName.c_str());
                    peer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, serverAddress, false);
                    break;
				}

                case ID_CONNECTION_LOST:
                {
                    ConsolePrint("Connection lost.\n");
                    running_ = false;

                    break;
                }

				case ID_CONNECTION_ATTEMPT_FAILED:
                {
                    state_ = FailedConnection;
                    ConsolePrint("Failed to connect to server.\n");
                    running_ = false;

                    break;
                }

                case ID_DISCONNECTION_NOTIFICATION:
                {
                    ConsolePrint("Connection dropped, probably by the server.\n");
                    running_ = false;

                    break;
                }
                    

                case ID_CHAT_MESSAGE:
                {
                    RakString rs_author;
                    RakString rs_msg;
                    BitStream bsIn = BitStream(packet->data, packet->length, false);
                    bsIn.IgnoreBytes(sizeof(RakNet::MessageID));
                    bsIn.Read(rs_author);
                    bsIn.Read(rs_msg);
                    ChatMessage message;
                    message.message_type = SPK_CHAT_MESSAGE;
                    message.messageAuthor = rs_author.C_String();
                    message.messageContent = rs_msg.C_String();

                    {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        if(MessageQueue.size() >= 500)
                            MessageQueue.pop_front();

                        MessageQueue.push_back(message);
                        consoleBufUpdated = true;
                    }
                    break;
                }
            
                case ID_SYSTEM_MESSAGE:
                {
                    RakString rs_msg;
                    BitStream bsIn = BitStream(packet->data, packet->length, false);
                    bsIn.IgnoreBytes(sizeof(RakNet::MessageID));
                    bsIn.Read(rs_msg);
                    ChatMessage message;
                    message.message_type = SPK_SYSTEM_MESSAGE;
                    message.messageAuthor = "[SYSTEM] - ";
                    message.messageContent = rs_msg.C_String();
                    {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        if(MessageQueue.size() >= 500)
                            MessageQueue.pop_front();
                        MessageQueue.push_back(message);
                    }
                    break;
                }
            
                case ID_NO_FREE_INCOMING_CONNECTIONS:
                {
                    ConsolePrint("The server is full.\n");
			        break;
                }
    		        
                case ID_VOICE_DATA:
                {
                    if(voiceEngine)
                    {
                        if (voiceEngine->GetState() != ENGINE_OK)
                            break;
                    }
                    else break;
                    BitStream bsIn = BitStream(packet->data, packet->length, false);
                    uint16_t userid;
                    uint16_t size;
                    RakString fromNome;
                    bsIn.IgnoreBytes(sizeof(RakNet::MessageID));
                    if (!bsIn.Read(userid)) break;
                    if (!bsIn.Read(size)) break;
                    if (size == 0 || size > 512)
                    {
                        ConsolePrint("Invalid Buffer size: %d\n", size);
                        break;
                    }
                    if(bsIn.GetNumberOfUnreadBits() < size * 8)
                    {
                        ConsolePrint("Malformed voice packet.\n");
                        break;
                    }
                    bsIn.Read(fromNome);

                    /*
                    ID_VOICE_DATA
                    USER ID, UNSIGNED 16 BIT INT
                    LENGTH OF BUFFER
                    NAME OF SPEAKER
                    ACTUAL VOICE DATA
                    */

                    uint8_t buf[512];
                    bsIn.Read(reinterpret_cast<char*>(buf), size);
                    voiceEngine->OnNetworkVoice(userid, buf, size, fromNome.C_String());
                    break;
                }
                case ID_USER_UPDATE:
                {
                    BitStream bsIn = BitStream(packet->data, packet->length, false);
                    bsIn.IgnoreBytes(sizeof(RakNet::MessageID));
                    unsigned char action = 'X';
                    uint16_t quitter = 0;
                    if (!bsIn.Read(action)) break;
                    if (!bsIn.Read(quitter) || quitter == 0) break;
                    switch (action)
                    {
                        case 'Q':
                        {
                            std::lock_guard<std::mutex> lock(pMapMutex_);
                            if (auto it = PeerMap.find(quitter); it != PeerMap.end())
                            {
                                PeerMap.erase(quitter);
                            }
                            break;
                        }
                        case 'J':
                        {
                            uint16_t newUserChannel;
                            if (!bsIn.Read(newUserChannel)) break;
                            std::lock_guard<std::mutex> lock(pMapMutex_);
                            if (auto it = PeerMap.find(quitter); it != PeerMap.end())
                            {
                                User n = it->second;
                                n.channelId = newUserChannel;
                                PeerMap.insert_or_assign(quitter, n);
                            }
                        }
                        break;
                    }

                }

			    default:
			        break;

            }
        }
        if(!working)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void ExtendedClient::ProcessSlashCommand(const std::string& cmdtext)
{
    if(cmdtext.find("/myname") == 0)
    {
        ConsolePrint("You are registered as %s.", config_.userName.c_str());
    }

    else if(cmdtext.find("/exit") == 0)
    {
        ConsolePrint("Disconnecting from server...");

        running_ = false;
        connected_ = false;
        if (voiceEngine)
            voiceEngine->Shutdown();
        peer->Shutdown(300);
        
        //peer->Shutdown(0);
    }

    else if(cmdtext.find("/deaf") == 0)
    {
        if(!voiceEngine) return;
        bool state = voiceEngine->GetDevice()->Deaf();
        ConsolePrint("%s", (state) ? "Deafen." : "Undeafen.");
    }

    else if(cmdtext.find("/mute") == 0)
    {
        if(!voiceEngine) return;
        bool state = voiceEngine->GetDevice()->Mute();
        ConsolePrint("%s", (state) ? "Muted." : "Unmuted.");
    }

    else if (cmdtext.find("/loopback") == 0)
    {
        if(!voiceEngine) return;
        bool state = voiceEngine->GetDevice()->ToggleLoopback();
        ConsolePrint("%s", (state) ? "Now you can hear yourself." : "Now you can't hear yourself.");
    }

    else if (cmdtext.find("/mastervolume") == 0)
    {
        if(!voiceEngine) return;
        std::istringstream iss(cmdtext);

        std::string command;
        std::string valueStr;

        iss >> command >> valueStr;

        if (valueStr.empty())
        {
            ConsolePrint("Usage: /mastervolume <0.0 - 100.0> | Current Volume: %0.1f", (voiceEngine->GetMasterVolume()*100.f));
            return;
        }

        char* endPtr = nullptr;

        float volume = std::strtof(valueStr.c_str(), &endPtr);

        if (endPtr == valueStr.c_str())
        {
            ConsolePrint("Invalid number.");
        }

        volume = std::clamp(volume, 0.0f, 100.0f);

        voiceEngine->SetMasterVolume(volume);

        ConsolePrint("Master volume set to %0.1f", volume);
    }

    else if (cmdtext.find("/peervolume") == 0)
    {
        std::istringstream iss(cmdtext);

        std::string command;
        uint16_t peerId = 0;
        float volume = 0.0;

        iss >> command >> peerId >> volume;

        
        if (iss.fail())
        {
            ConsolePrint("Usage: /peervolume <Peer ID> <0.0 - 100.0>");
            return;
        }
        RemoteVoice* selectedPeerVoice = voiceEngine->GetRemoteVoice(peerId);
        volume = std::clamp(volume, 0.0f, 100.0f);
        
        if (selectedPeerVoice)
        {
            selectedPeerVoice->SetPeerVolume(volume);
            ConsolePrint("Updated volume for %s to %0.1f.", selectedPeerVoice->rvUsername, volume); 
        }
        else
        {
            ConsolePrint("Invalid Peer ID. (Usage: /peervolume <Peer ID> <0.0 - 100.0>");
            return;
        }

    }

    else
    {
        BitStream cmdStream;
        RakString rsCMD(cmdtext.c_str());
        cmdStream.Write(static_cast<RakNet::MessageID>(ID_COMMAND));
        cmdStream.Write(rsCMD);
        peer->Send(&cmdStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, serverAddress, false);    
    }

    
}
void ExtendedClient::SendMessage(const char* message)
{
    if (message[0] == '/')
    {
        ProcessSlashCommand(std::string(message));
    }
    else
    {
        BitStream bsOut;
        bsOut.Write(static_cast<RakNet::MessageID>(ID_CHAT_MESSAGE));
        bsOut.Write(message);
        peer->Send(&bsOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, serverAddress, false);    
    }
}