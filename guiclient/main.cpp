#include "GUIClient.hpp"
static GUIClient visual_;

#ifdef _WIN32
    #include <windows.h>

    LONG WINAPI oopsieHandler(EXCEPTION_POINTERS* info)
    {
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

    visual_.Initialize();
    return 0;
}