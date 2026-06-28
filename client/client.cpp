#include "Client.hpp"
#ifdef _WIN32
    #include <conio.h>
#else
    #include <Kbhit.h>
    #define _kbhit kbhit
    #define _getch getch
#endif

#include <algorithm>
#include <sstream>

std::atomic<bool> clientInit{ true };

RakChatClient::RakChatClient()
{
    peer = RakNet::RakPeerInterface::GetInstance();
    peer->Startup(1, &sd, 1);
    printf("RakChat Client started.\n");
    
}
RakChatClient::~RakChatClient()
{
    running_ = false;
    connected_ = false;
    peer->Shutdown(300);
    if(workerThread.joinable())
    {
        workerThread.join();
    }
    
    RakNet::RakPeerInterface::DestroyInstance(peer);
}


void RakChatClient::ClientThread()
{
    //printf("Packet thread: It's on! :)\n");
    while(running_)
    {
        for (auto* packet = peer->Receive(); packet; peer->DeallocatePacket(packet), packet=peer->Receive())
		{
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
                        printf("You registered as %s (%d).", config_.userName.c_str(), myId);
                        printf(" Now you can send messages.\n");
                        voiceEngine = new SpeakeasyEngine(peer);
                        this->connected_ = true;
                    }
                    else if (result == 'O')
                    {
                        printf("Registration failed because name is in use.\n");
                        clientInit = true;
                        return;
                    }
                    else
                    {
                        printf("Registration failed.");
                        clientInit = true;
                        return;
                    }
                    break;
                }
                        
                case ID_CONNECTION_REQUEST_ACCEPTED:
				{
					printf("You are connected! Registering you...\n");
                    
                    serverAddress = peer->GetSystemAddressFromIndex(0);
                    BitStream bs = BitStream();
                    bs.Write(static_cast<RakNet::MessageID>(ID_REGISTER_ME));
                    bs.Write(config_.userName.c_str());
                    peer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, serverAddress, false);
                    break;
				}

                case ID_CONNECTION_LOST:
                {
                    printf("Connection lost.\n");
                    clientInit = true;
                    return;
                }

				case ID_CONNECTION_ATTEMPT_FAILED:
                {
                    printf("Failed to connect to server.\n");
                    clientInit = true;
                    return;
                }

                case ID_DISCONNECTION_NOTIFICATION:
                {
                    printf("Connection dropped, probably by the server.\n");
                    clientInit = true;
                    return;
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
                        MessageQueue.push(message);
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
                    message.messageAuthor = "\033[33m[SYSTEM]\033[37m - ";
                    message.messageContent = rs_msg.C_String();
                    {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        MessageQueue.push(message);
                    }
                    break;
                }
            
                case ID_NO_FREE_INCOMING_CONNECTIONS:
                {
                    printf("The server is full.\n");
			        break;
                }
    		        
                case ID_VOICE_DATA: {
                    if(voiceEngine)
                    {
                        if (voiceEngine->GetState() != ENGINE_OK)
                            break;
                    }
                    else break;
                    BitStream bsIn = BitStream(packet->data, packet->length, false);
                    uint16_t userid;
                    uint16_t size;
                    bsIn.IgnoreBytes(sizeof(RakNet::MessageID));
                    
                    if (!bsIn.Read(userid)) break;
                    if (!bsIn.Read(size)) break;
                    if (size == 0 || size > 512)
                    {
                        printf("Invalid Buffer size: %d\n", size);
                        break;
                    }
                    if(bsIn.GetNumberOfUnreadBits() < size * 8)
                    {
                        printf("Malformed voice packet.\n");
                        break;
                    }
                    RakString rsName;
                    bsIn.Read(rsName);
                    uint8_t buf[512];
                    bsIn.Read(reinterpret_cast<char*>(buf), size);
                    voiceEngine->OnNetworkVoice(userid, buf, size, rsName.C_String());
                    break;
                }
			    default:
			        break;

            }
        } 
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void RakChatClient::ProcessSlashCommand(const std::string& cmdtext)
{
    if(cmdtext.find("/myname") == 0)
    {
        std::cout << "You are registered as " << config_.userName << ".\n";
    }

    else if(cmdtext.find("/exit") == 0)
    {
        std::cout << "Disconnecting from server..." << "\n";
        clientInit = true;
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
        printf("%s\n", (state) ? "Deafen." : "Undeafen.");
    }

    else if(cmdtext.find("/mute") == 0)
    {
        if(!voiceEngine) return;
        bool state = voiceEngine->GetDevice()->Mute();
        printf("%s\n", (state) ? "Muted." : "Unmuted.");
    }

    else if (cmdtext.find("/loopback") == 0)
    {
        if(!voiceEngine) return;
        bool state = voiceEngine->GetDevice()->ToggleLoopback();
        printf("%s\n", (state) ? "Now you can hear yourself." : "Now you can't hear yourself.");
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
            std::cout << "Usage: /mastervolume <0.0 - 100.0> | Current Volume: " << (voiceEngine->GetMasterVolume()*100.f) << "\n";
            return;
        }

        char* endPtr = nullptr;

        float volume = std::strtof(valueStr.c_str(), &endPtr);

        if (endPtr == valueStr.c_str())
        {
            std::cout << "Invalid number.\n";
        }

        volume = std::clamp(volume, 0.0f, 100.0f);

        voiceEngine->SetMasterVolume(volume);

        std::cout << "Master volume set to " << volume << "\n";
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
            std::cout << "Usage: /peervolume <Peer ID> <0.0 - 100.0>\n";
            return;
        }
        RemoteVoice* selectedPeerVoice = voiceEngine->GetRemoteVoice(peerId);
        volume = std::clamp(volume, 0.0f, 100.0f);
        
        if (selectedPeerVoice)
        {
            selectedPeerVoice->SetPeerVolume(volume);
            std::cout << "Updated volume for " << selectedPeerVoice->rvUsername << " to " << volume << ".\n"; 
        }
        else
        {
            std::cout << "Invalid Peer ID. (Usage: /peervolume <Peer ID> <0.0 - 100.0>)\n";
            return;
        }

    }

    else
    {
        BitStream cmdStream = BitStream();
        RakString rsCMD(cmdtext.c_str());
        cmdStream.Write(static_cast<RakNet::MessageID>(ID_COMMAND));
        cmdStream.Write(rsCMD);
        peer->Send(&cmdStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, serverAddress, false);    
    }

    
}

void RakChatClient::ClientMain()
{
    while (config_.serverIp.empty())
    {
        printf("Input server IP Address: ");
        std::getline(std::cin, config_.serverIp);
    }

    while (true)
    {
        printf("Input server port: ");
        std::string portString;
        std::getline(std::cin, portString);
        if(portString.empty())
            continue;

        try
        {
            int port = std::stoi(portString);
            if (port >= 1 && port <= 65535)
            {
                config_.serverPort = static_cast<unsigned short>(port);
                break;
            } 
            else
            {
                std::cout << "Port must be between 1 and 65535.\n";
            }
        }
        catch (...)
        {
            std::cout << "Invalid port. Please enter a number.\n";
        }
    }

    while (config_.userName.empty())
    {
        printf("Please enter your username: ");
        std::getline(std::cin, config_.userName);
    }
    printf("Done! We will patch you up to %s:%d, %s...\n", config_.serverIp.c_str(), config_.serverPort, config_.userName.c_str());
    
    peer->Connect(config_.serverIp.c_str(), config_.serverPort, nullptr, 0);
    running_ = true;
    workerThread = std::thread(&RakChatClient::ClientThread, this);
    

    std::string writeBuffer;

    while (!connected_ && running_)
    {
        if (clientInit == true) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::cout << "> " << std::flush;
    while (connected_) 
    { 
        std::queue<ChatMessage> localQueue;

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            std::swap(localQueue, MessageQueue);
        }

        while (!localQueue.empty())
        {
            ChatMessage m = localQueue.front();
            std::cout << "\r\33[2K";
            localQueue.pop();
            switch (m.message_type)
            {
                case SPK_CHAT_MESSAGE:
                {
                    std::cout << m.messageAuthor << ": " 
                        << m.messageContent << "\n";
                        std::cout << "> " << writeBuffer << std::flush;
                    break;
                }
                case SPK_SYSTEM_MESSAGE:
                {
                    std::cout
                        << m.messageAuthor
                        << m.messageContent << "\n";
                    std::cout << "> " << writeBuffer << std::flush;
                    break;
                }
                default:
                    break;
            }    
        }
        
        if (_kbhit())
        {
            int ch = _getch();
            
            if(ch == 8)
            {
                if(!writeBuffer.empty())
                {
                    writeBuffer.pop_back();
                    std::cout << "\b \b";
                }

            }

            else if (ch == '\r')
            {
                if (writeBuffer.empty()) continue;
                if (writeBuffer[0] == '/')
                {
                    std::cout << "\n";
                    ProcessSlashCommand(writeBuffer);
                }
                else
                {
                    BitStream bsOut = BitStream();
                    bsOut.Write(static_cast<RakNet::MessageID>(ID_CHAT_MESSAGE));
                    bsOut.Write(writeBuffer.c_str());
                    peer->Send(&bsOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, serverAddress, false);    
                }
                writeBuffer.clear();
                std::cout << "\r\33[2K> " << std::flush;
            }
            else
            {
                if (writeBuffer.length() <= 144)
                {
                    writeBuffer.push_back(ch);
                    std::cout << static_cast<char>(ch);
                }
            }

        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

}

