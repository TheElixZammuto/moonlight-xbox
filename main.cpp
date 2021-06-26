#include <SDL.h>
#include <wrl.h>
#include <SDL_ttf.h>

extern "C" {
    #include <libgamestream/client.h>
    #include "sdl-state.h"
    #include "input/sdlinput.h"
}

bool run = false;
SERVER_DATA server;
STREAM_CONFIGURATION config;
SDL_DisplayMode mode;


int init_stream(void *data) {
    char* folder = SDL_GetPrefPath("Moonlight", "Xbox");
    char ipAddressFile[2048];
    char mappingsFile[2048];
    sprintf(ipAddressFile,"%s%s", folder, "ip_address");
    sprintf(mappingsFile,"%s%s", SDL_GetBasePath(), "gamecontrollerdb.txt");
    char ipAddress[256];
    FILE *fp = fopen(ipAddressFile, "r");
    if (fp == NULL) {
        char errorMsg[2048];
        set_text("Error in opening ip_address in LocalState");
        return 1;
    }
    fgets(ipAddress, 256, fp);
    fclose(fp);
    int status = 0;
    char connectionMsg[2048];
    sprintf(connectionMsg, "Connecting to %s", ipAddress);
    set_text(connectionMsg);
    status = gs_init(&server, ipAddress, folder, 0, 0);
    set_text("Init complete");
    if (!server.paired) {
        char pin[5];
        sprintf(pin, "%d%d%d%d", rand() % 10,rand()%10,rand()%10,rand() % 10);
        char printText[1024];
        sprintf(printText, "PIN to Pair: %s", pin);
        set_text(printText);
        if ((status = gs_pair(&server, &pin[0])) != 0) {
            gs_unpair(&server);
            set_text("Failed to pair to server");
            return 1;
        }
        else {
            set_text("Succesfully paired\n");
        }
    }
    else {
        set_text("Succesfully paired\n");
    }
    PAPP_LIST list;
    gs_applist(&server, &list);
    while (strcmp(list->name, "Desktop") != 0) {
        list = list->next;
    }
    config.width = mode.w;
    config.height = mode.h;
    config.bitrate = 500;
    config.fps = 60;
    config.packetSize = 1024;
    config.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
    sdlinput_init(mappingsFile);
    int a = gs_start_app(&server, &config, list->id, false, true, 0);
    CONNECTION_LISTENER_CALLBACKS callbacks;
    DECODER_RENDERER_CALLBACKS rCallbacks = get_video_callback();
    set_text("Streaming Setup...");
    int e = LiStartConnection(&server.serverInfo, &config,NULL, &rCallbacks, NULL, NULL, 0, NULL, 0);
    if (e != 0) {
        set_text("Error!");
    }
    set_text("OK");
    return 0;
}

int main(int argc, char** argv) {
    SDL_Window* window = NULL; SDL_Renderer* renderer = NULL; SDL_Event evt;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return 1;
    }
    
    if (SDL_GetCurrentDisplayMode(0, &mode) != 0) {
        return 1;
    }
    SDL_SetHint(SDL_HINT_WINRT_HANDLE_BACK_BUTTON, "1");
    sdl_init(mode.w,mode.h, true);
    SDL_CreateThread(init_stream, "", NULL);
    sdl_loop();
}