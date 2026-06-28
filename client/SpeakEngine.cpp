#include <rakChat.h>
#include "SpeakEngine.hpp"
#include "BitStream.h"
#include <algorithm>


using namespace RakNet;

SpeakeasyEngine::SpeakeasyEngine(RakPeerInterface* p)
{
    peer = p;
    bool success = device_.Initialize(this);
    if (success)
    {
        engineState = ENGINE_OK;
        device_.Start();
        encoder_.Initialize(DEFAULT_SAMPLE_RATE, 1);
        running_ = true;
        spkThr = std::thread(&SpeakeasyEngine::spkThread, this);
    }
    else engineState = ENGINE_ERROR;
}
SpeakeasyEngine::~SpeakeasyEngine()
{
    this->Shutdown();
}

AudioDevice* SpeakeasyEngine::GetDevice()
{
    return &device_;
}

void SpeakeasyEngine::SetMasterVolume(float value)
{ 
    masterVolume = value / 100.0f;
}

void SpeakeasyEngine::OnAudioInput(int16_t* pcm, unsigned long frameCount)
{
    if (frameCount != DEFAULT_FRAMES_PER_BUFFER) return;

    VoiceBuffer toPush;
    memcpy(toPush.data_, pcm, sizeof(toPush.data_));
    {
        std::lock_guard<std::mutex> lock(captureMutex);
        captureQueue.push(toPush);
    }
}


void SpeakeasyEngine::spkThread()
{
    while( running_  )
    {
        VoiceBuffer buf;
        bool hasData = false;
        {
            std::lock_guard<std::mutex> lock(captureMutex);
            if (!captureQueue.empty())
            {
                buf = captureQueue.front();
                captureQueue.pop();
                hasData = true;
            }
        }
        if(hasData)
        {
            BitStream bs = BitStream();
            uint8_t encoded[512];
            int written = encoder_.Encode(buf.data_, DEFAULT_FRAMES_PER_BUFFER, encoded, sizeof(encoded));
            bs.Write((RakNet::MessageID)ID_VOICE_DATA);
            bs.Write((uint16_t)written);
            bs.Write(reinterpret_cast<const char*>(encoded), written);
            peer->Send(&bs, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 1, UNASSIGNED_SYSTEM_ADDRESS, true);
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

void SpeakeasyEngine::Shutdown()
{
    device_.Stop();
    device_.Shutdown();
    encoder_.Shutdown();

    running_ = false;
    if (spkThr.joinable())
        spkThr.join();
}

RemoteVoice* SpeakeasyEngine::GetRemoteVoice(uint16_t id)
{
    
    if (auto it = RemoteSpeakers.find(id); it != RemoteSpeakers.end())
    {
        return &it->second;
    }
    return nullptr;
}

void SpeakeasyEngine::OnNetworkVoice(uint16_t id, uint8_t* data, uint16_t size, const char* fromName)
{
    {
        std::lock_guard<std::mutex> lock(voicesMutex);
        auto it = RemoteSpeakers.find(id);
        if (it == RemoteSpeakers.end())
        {
            RemoteVoice rv;
            rv.rvUsername = fromName;
            rv.decoder.Initialize(DEFAULT_SAMPLE_RATE, 1);
                RemoteSpeakers.emplace(id, std::move(rv));
            it = RemoteSpeakers.find(id);
        }
        RemoteVoice& rV = it->second;
        std::array<int16_t, DEFAULT_FRAMES_PER_BUFFER> pcm;
        int samples = rV.decoder.Decode(data, size, pcm.data(), DEFAULT_FRAMES_PER_BUFFER);
        if (rV.speakerVolume != 1.0f)
        {
            for (int i = 0; i < samples; i++)
            {    
                float scaled = pcm[i] * rV.speakerVolume;
                pcm[i] = static_cast<int16_t>(std::clamp(scaled, -32768.0f, 32767.0f));
            }
        }
        if (masterVolume != 1.0f)
        {
            for (int i = 0; i < samples; i++)
            {    
                float scaled = pcm[i] * masterVolume;
                pcm[i] = static_cast<int16_t>(std::clamp(scaled, -32768.0f, 32767.0f));
            }
        }
        if (samples == DEFAULT_FRAMES_PER_BUFFER)
        {    
            rV.jitter.push(pcm);
        }
    }
}

void SpeakeasyEngine::MixOutput(int16_t* out)
{
    {
        std::lock_guard<std::mutex> lock(voicesMutex);
        memset(out, 0, DEFAULT_FRAMES_PER_BUFFER * sizeof(int16_t));
        for (auto& [gid, voice] : RemoteSpeakers)
        {
            if (!voice.jitter.empty())
            {
                if (!voice.started)
                {
                    if (voice.jitter.size() >= 3)
                    {
                        voice.started = true;
                    }
                    else continue;
                }
                auto frame = voice.jitter.front();
                voice.jitter.pop();

                for (int i = 0; i < DEFAULT_FRAMES_PER_BUFFER; i++)
                {
                    int32_t mix = out[i] + frame[i];
                    mix = std::clamp(mix, -32768, 32767);
                    out[i] = static_cast<int16_t>(mix);
                }
            }
        }
    }
}


