#include "GUIClient.hpp"
//#include "BitStream.h"
#include <algorithm>
#include <functional>

static int text_height(mu_Font font) {
  return r_get_text_height();
}
static int text_width(mu_Font font, const char *text, int len) {
  if (len == -1) { len = strlen(text); }
  return r_get_text_width(text, len);
}

static float bg[3] = { 90, 95, 100 };

std::array<char, 256> button_map = {};

std::array<char, 256> key_map = {};

static void init_maps()
{
    button_map[SDL_BUTTON_LEFT & 0xff] = MU_MOUSE_LEFT;
    button_map[SDL_BUTTON_RIGHT & 0xff] = MU_MOUSE_RIGHT;
    button_map[SDL_BUTTON_MIDDLE & 0xff] = MU_MOUSE_MIDDLE;

    key_map[SDLK_LSHIFT       & 0xff] = MU_KEY_SHIFT;
    key_map[SDLK_RSHIFT       & 0xff] = MU_KEY_SHIFT;
    key_map[SDLK_LCTRL        & 0xff] = MU_KEY_CTRL;
    key_map[SDLK_RCTRL        & 0xff] = MU_KEY_CTRL;
    key_map[SDLK_LALT         & 0xff] = MU_KEY_ALT;
    key_map[SDLK_RALT         & 0xff] = MU_KEY_ALT;
    key_map[SDLK_RETURN       & 0xff] = MU_KEY_RETURN;
    key_map[SDLK_BACKSPACE    & 0xff] = MU_KEY_BACKSPACE;
}



void GUIClient::Resize(int width, int height)
{
   r_resize(width, height);
}


void GUIClient::Initialize()
{
    init_maps();
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        std::cout << "SDL INIT ERROR: " << SDL_GetError() << "\n";
        return;
    }
    r_init();
    ctx = static_cast<mu_Context*>(malloc(sizeof(mu_Context)));

    mu_init(ctx);

    ctx->text_width = text_width;
    ctx->text_height = text_height;


    running_ = true;
    while (running_)
    {
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT: 
                {
                    exit(EXIT_SUCCESS);
                    delete under_.release();
                    break;
                }
                case SDL_MOUSEMOTION: mu_input_mousemove(ctx, event.motion.x, event.motion.y); break;
                case SDL_MOUSEWHEEL: mu_input_scroll(ctx, 0, event.wheel.y * -30); break;
                case SDL_TEXTINPUT: mu_input_text(ctx, event.text.text); break;
                            
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP: {
                    int b = button_map[event.button.button & 0xff];
                    if (b && event.type == SDL_MOUSEBUTTONDOWN) { mu_input_mousedown(ctx, event.button.x, event.button.y, b); }
                    if (b && event.type ==   SDL_MOUSEBUTTONUP) { mu_input_mouseup(ctx, event.button.x, event.button.y, b);   }
                    break;
                }
                
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                int c = key_map[event.key.keysym.sym & 0xff];
                if (c && event.type == SDL_KEYDOWN) { mu_input_keydown(ctx, c); }
                if (c && event.type ==   SDL_KEYUP) { mu_input_keyup(ctx, c);   }
                    break;
                }
            }    
        }


        ProcessGUI();

        


        /* render */
        r_clear(mu_color(bg[0], bg[1], bg[2], 255));
        mu_Command *cmd = NULL;
        while (mu_next_command(ctx, &cmd)) {
          switch (cmd->type) {
            case MU_COMMAND_TEXT: r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color); break;
            case MU_COMMAND_RECT: r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
            case MU_COMMAND_ICON: r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
            case MU_COMMAND_CLIP: r_set_clip_rect(cmd->clip.rect); break;
          }
        }
        r_present();
    }
    SDL_Quit();
}

void GUIClient::ProcessGUI()
{
    mu_begin(ctx);
    switch (under_->GetClientState())
    {
        
        case FailedConnection:
        {
            PromptInitWindow(); break;
        }
        case RegistrationFailed:
        {
            PromptInitWindow(); break;
        }
        case Init:
        case Connecting:
        case Registering:
            PromptInitWindow(); break;

        case Connected: PromptMainWindow(); break;
    }
    mu_end(ctx);
}
void GUIClient::PromptInitWindow()
{
    static std::string winTitle;

    switch (under_->GetClientState())
    {
        case Connecting: break;
        case Init: winTitle = "Speakeasy Client"; break;
        case FailedConnection:
        { 
            winTitle = "Connection failed!";
            break;
        } 
        case RegistrationFailed:
        { 
            winTitle = "Registration Failed.";
            break; 
        } 
        case Registering: winTitle = "Connection established. Registering..."; break;
    }

    Resize(300, 300);
    if (mu_begin_window_ex(ctx, winTitle.c_str(), mu_rect(0, 0, 300, 300), MU_OPT_NOCLOSE | MU_OPT_NORESIZE | MU_OPT_NOINTERACT | MU_OPT_NOSCROLL))
    {
        //int submit = 0;
        static char ipAddr[20] = "127.0.0.1";
        static char svPort[7] = "60000";
        static char myUsername[64] = "RakChatUser";
        int label_widths[] = {130, 100, 100};
        int widths[] = {100, 100, 100};

        mu_layout_row(ctx, 3, label_widths, 0);
        mu_label(ctx, "");
        mu_label(ctx, "IP:");
        mu_layout_row(ctx, 2, widths, 0);
        mu_label(ctx, "");
        if (mu_textbox(ctx, ipAddr, sizeof(ipAddr))) {}

        mu_layout_row(ctx, 2, label_widths, 0);
        mu_label(ctx, "");
        mu_label(ctx, "Port:");
        mu_layout_row(ctx, 2, widths, 0);
        mu_label(ctx, "");
        if (mu_textbox(ctx, svPort, sizeof(svPort))) {}
        mu_layout_row(ctx, 3, label_widths, 0);
        mu_label(ctx, "");
        mu_label(ctx, "Username:");
        mu_layout_row(ctx, 2, widths, 0);
        mu_label(ctx, "");
        if (mu_textbox(ctx, myUsername, sizeof(myUsername))) {}

        mu_layout_row(ctx, 2, widths, 0);
        mu_label(ctx, "");
        mu_label(ctx, "");
        mu_layout_row(ctx, 2, widths, 0);
        mu_label(ctx, "");
        if (mu_button(ctx, "Connect"))
        {
            unsigned short port = std::clamp(std::atoi(svPort), 0, 65535);
            if (port != 0)
            {
                under_->ClientConfigure(ipAddr, port, myUsername);
                under_->ClientConnect();
            }
            winTitle.clear();
            winTitle += "Connecting to ";
            winTitle += ipAddr;
            winTitle += ":";
            winTitle += svPort;
            winTitle += "...";
        }
    }
    mu_end_window(ctx);
}



void GUIClient::PromptMainWindow()
{
    Resize(800, 600);

    if (mu_begin_window_ex(ctx, "", mu_rect(0, 0, 800, 600), MU_OPT_NORESIZE | MU_OPT_NOINTERACT | MU_OPT_NOSCROLL | MU_OPT_NOTITLE))
    {
        mu_layout_row(ctx, 1, std::array<int, 1>{0}.data(), 0);
        if (mu_button(ctx, "Sound"))
        {
            mu_open_popup(ctx, "soundoptionspopup");

        }
        if (mu_begin_popup(ctx, "soundoptionspopup"))
        {
            if (mu_button(ctx, "Mute"))
            {
                bool state = under_->Mute();
                under_->ConsolePrint("%s", (state) ? "Muted." : "Unmuted.");
            }
            mu_end_popup(ctx);
        }
        mu_layout_row(ctx, 1, std::array<int, 1>{-1}.data(), -200);
        mu_begin_panel(ctx, "Channel list");
        std::unordered_map<uint16_t, Channel> localChannelMap;
        {
            std::lock_guard<std::mutex> lock(under_->cMapMutex_);
            localChannelMap = under_->GetChannels();
        }
        std::unordered_map<uint16_t, User> localUserMap;
        {
            std::lock_guard<std::mutex> lock(under_->pMapMutex_);
            localUserMap = under_->GetUsers();
        }

        std::function<void(uint16_t)> renderChannel;
        int depth = 0;
        std::function<void(uint16_t, uint16_t)> renderUser;
        renderUser = [&](uint16_t uid, uint16_t channel_id)
        {
            const User& usr = localUserMap.at(uid);
            mu_push_id(ctx, &uid, sizeof(uid));
            mu_push_id(ctx, &channel_id, sizeof(channel_id));
            mu_layout_row(ctx, 2, std::array<int, 2>{-180, 0}.data(), 0);
            //printf("renderUser: uid=%d name='%s'\n", uid, usr.UserName.c_str());
            mu_label(ctx, usr.UserName.c_str());
            if (mu_button(ctx, "Options"))
            {

            }
            mu_pop_id(ctx);
            mu_pop_id(ctx);
        };
        renderChannel = [&](uint16_t cid)
        {
            const Channel& child = localChannelMap[cid];
            mu_push_id(ctx, &cid, sizeof(cid));
            mu_layout_row(ctx, 2, std::array<int, 2>{-180, 0}.data(), 0);
            bool joinAction = false;
            //mu_label(ctx, child.channelName.c_str());
            mu_layout_begin_column(ctx);
            if (mu_begin_treenode_ex(ctx, child.channelName.c_str(), MU_OPT_EXPANDED))
            {
                for (const auto& [uid, user] : localUserMap)
                {
                    if (user.channelId == cid)
                        renderUser(uid, cid);
                }

                for (auto& [childId, child] : localChannelMap)
                {
                    if (childId == cid || !child.hasParent) continue;  
                    else if (child.channelParent == cid)
                    {
                        renderChannel(childId);
                    }
                }
                mu_end_treenode(ctx);
            }
            mu_layout_end_column(ctx);
            mu_layout_begin_column(ctx);

            mu_layout_row(ctx, 2, std::array<int, 2>{0, 0}.data(), 0);
            
            if (mu_button(ctx, "Join"))
                joinAction = true;

            if (mu_button(ctx, "Options"))
                mu_open_popup(ctx, "channeloptionspopup");

            if (mu_begin_popup(ctx, "channeloptionspopup"))
            {
                if (mu_button(ctx, "Join Channel"))
                    joinAction = true;
                mu_end_popup(ctx);
            }
            if (joinAction)
                under_->JoinChannel(cid);
            mu_layout_end_column(ctx);
            mu_pop_id(ctx);
        };  
        for (const auto& [cid, chan] : localChannelMap)
        {
            if (!chan.hasParent)
                renderChannel(cid);
            continue;
        }
        mu_end_panel(ctx);
        mu_layout_row(ctx, 1, std::array<int, 1>{-1}.data(), -30);
        mu_begin_panel(ctx, "Console Output");
        mu_Container* panel = mu_get_current_container(ctx);
        mu_layout_row(ctx, 1, std::array<int, 1>{-1}.data(), -1);
        mu_text(ctx, under_->FetchBuffer());
        if (under_->PollBuff())
        {
            panel->scroll.y = panel->content_size.y;
        }
        mu_end_panel(ctx);
        static char writeBuffer[256];
        int send = 0;
        mu_layout_row(ctx, 2, std::array<int, 2>{-70, -1}.data(), 0);
        if (mu_textbox(ctx, writeBuffer, sizeof(writeBuffer)) & MU_RES_SUBMIT) 
        {
            mu_set_focus(ctx, ctx->last_id);
            send = 1;
        }
        if (mu_button(ctx, "Send"))
            send = 1;

        if (send )
        {
            if (writeBuffer[0] != '\0')
            {
                under_->SendMessage(writeBuffer);
                writeBuffer[0] = '\0';
            }
        }
    }
    mu_end_window(ctx);
}