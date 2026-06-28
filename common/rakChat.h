//#pragma once

#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"
#include "RakString.h"
#include "RakNetTypes.h"  // MessageID


#define INVALID_CHANNEL_ID  65535

#if !defined RKC_MAX_CLIENTS
    #define RKC_MAX_CLIENTS (64);
#endif


enum GameMessages
{
    ID_SYSTEM_MESSAGE=ID_USER_PACKET_ENUM+1,
    ID_REGISTER_ME,
    // + uchar 'Y' - registration successful.
    // + uchar 'O' - Name in use - Registration failed.
    // + uchar 'N' - Registration failed.
    ID_CHAT_MESSAGE,
    ID_VOICE_DATA,
    ID_QUERY,
    // + uchar 'P' - peerlist
    // // + uchar 'C' - channellist
    ID_CHANNEL_ACTION,
    ID_COMMAND,
    ID_USER_UPDATE
    // uchar 'Q' USER QUITTED
    // uchar 'J' JOIN
};