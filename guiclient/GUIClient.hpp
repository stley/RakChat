#include "CClient_impl.hpp"
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT  
#define NK_INCLUDE_FONT_BAKING            
#define SDL_MAIN_HANDLED
//#include <glad/glad.h>
#include <SDL.h>
extern "C"
{
    #include "render.h"
    #include "microui.h"
}
#include <unordered_map>
#include <SDL_opengl.h>
#include <queue>
#include <stdarg.h>
#include <mutex>






class GUIClient 
{
private:
    //SDL_Window* window_ = nullptr;
    //SDL_GLContext context_;
    std::atomic<bool> running_ = false;
    SDL_Event event;
    std::unique_ptr<ExtendedClient> under_;
    mu_Context *ctx = nullptr;

    void ProcessGUI();
public:
    void Resize(int width, int height);
    GUIClient(){ under_ = std::make_unique<ExtendedClient>(); }
    ~GUIClient() = default;

    void Initialize();

    void PromptInitWindow();
    void PromptMainWindow();
};