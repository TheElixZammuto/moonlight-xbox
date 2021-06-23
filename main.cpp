#include <SDL.h>
#include <wrl.h>

extern "C" {
    #include <libgamestream/client.h>
    #include "sdl-state.h"
}

int red = 0;
int green = 0;
int blue = 0;
bool run = false;
SERVER_DATA server;
STREAM_CONFIGURATION config;

int init_stream(void *data) {
    int status = 0;
    status = gs_init(&server, "10.1.0.2", SDL_GetPrefPath("Moonlight","Xbox"), 0, 0);
    blue = 127;
    printf("Init got: " + status);
    if (!server.paired) {
        char pin[5];
        sprintf(pin, "%d%d%d%d", 1,2,3,4);
        printf("Please enter the following PIN on the target PC: %s\n", pin);
        blue = 255;
        if ((status = gs_pair(&server, &pin[0])) != 0) {
            fprintf(stderr, "Failed to pair to server");
            blue = 0;
            red = 255;
        }
        else {
            printf("Succesfully paired\n");
            blue = 0;
            red = 0;
            green = 255;
        }
    }
    else {
        printf("Succesfully paired\n");
        blue = 0;
        red = 0;
        green = 255;
    }
    PAPP_LIST list;
    gs_applist(&server, &list);
    while (strncmp(list->name, "Desktop",0) == -1) {
        list = list->next;
    }
    config.width = 1920;
    config.height = 1080;
    config.bitrate = 8000;
    config.fps = 60;
    config.packetSize = 1024;
    config.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
    int a = gs_start_app(&server, &config, list->id, false, true, 0);
    CONNECTION_LISTENER_CALLBACKS callbacks;
    DECODER_RENDERER_CALLBACKS rCallbacks = get_video_callback();
    LiStartConnection(&server.serverInfo, &config,NULL, &rCallbacks, NULL, NULL, 0, NULL, 0);
    return 0;
}

int main(int argc, char** argv) {
    SDL_CreateThread(init_stream,"",NULL);
    SDL_DisplayMode mode; SDL_Window* window = NULL; SDL_Renderer* renderer = NULL; SDL_Event evt;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return 1;
    }

    if (SDL_GetCurrentDisplayMode(0, &mode) != 0) {
        return 1;
    }
    sdl_init(mode.w, mode.h, true);
    sdl_loop();
}