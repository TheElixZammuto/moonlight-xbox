/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */
#include "sdl-state.h"
#include <SDL_ttf.h>
#include <Limelight.h>

static bool done;
static int fullscreen_flags;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *bmp;
static TTF_Font* font;
char* message = "Ciao";
static SDL_Color white = {
   255,255,255
};
SDL_mutex *mutex;

int sdlCurrentFrame, sdlNextFrame;


void sdl_init(int width, int height, bool fullscreen) {
    char* path = SDL_GetBasePath();
    strcat(path, "font.ttf\0");
    TTF_Init();
  font = TTF_OpenFont(path, 16);
  sdlCurrentFrame = sdlNextFrame = 0;
  window = SDL_CreateWindow("Moonlight", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_FULLSCREEN);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  bmp = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_NV12, SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!bmp) {
    fprintf(stderr, "SDL: could not create texture - exiting\n");
    exit(1);
  }

  mutex = SDL_CreateMutex();
  if (!mutex) {
    fprintf(stderr, "Couldn't create mutex\n");
    exit(1);
  }
}

void set_text(char* msg) {
    message = msg;
    SDL_Event event;
    event.type = SDL_USEREVENT;
    event.user.code = SDL_CODE_TEXT;
    SDL_PushEvent(&event);
}

void sdl_loop() {
  SDL_Event event;
  while(!done && SDL_WaitEvent(&event)) {
      sdlinput_handle_event(&event);
      if (event.type == SDL_QUIT)
        done = true;
      else if (event.type == SDL_USEREVENT) {
        if (event.user.code == SDL_CODE_FRAME || event.user.code == SDL_CODE_TEXT) {
          if (++sdlCurrentFrame <= sdlNextFrame - SDL_BUFFER_FRAMES) {
            //Skip frame
          } else if (SDL_LockMutex(mutex) == 0) {
            if (event.user.code == SDL_CODE_FRAME) {
                Uint8** data = ((Uint8**)event.user.data1);
                int* linesize = ((int*)event.user.data2);
                SDL_UpdateNVTexture(bmp, NULL, data[0], linesize[0], data[1], linesize[1]);
                //SDL_UpdateYUVTexture(bmp, NULL, data[0], linesize[0], data[1], linesize[1], data[2], linesize[2]);
            }
            SDL_UnlockMutex(mutex);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, bmp, NULL, NULL);
            if (strlen(message) > 0) {
                SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, message, white);
                SDL_Texture* MessageSurface = SDL_CreateTextureFromSurface(renderer, surfaceMessage);
                SDL_Rect Message_rect; //create a rect
                Message_rect.x = 250;  //controls the rect's x coordinate 
                Message_rect.y = 64; // controls the rect's y coordinte
                TTF_SizeText(font, message, &(Message_rect.w), &(Message_rect.h));
                SDL_RenderCopy(renderer, MessageSurface, NULL, &Message_rect);
            }
            SDL_RenderPresent(renderer);
          } else
            fprintf(stderr, "Couldn't lock mutex\n");
        }
      }
    }
    SDL_DestroyWindow(window);
}
