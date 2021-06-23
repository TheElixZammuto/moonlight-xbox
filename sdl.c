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

#include <Limelight.h>

static bool done;
static int fullscreen_flags;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *bmp;

SDL_mutex *mutex;

int sdlCurrentFrame, sdlNextFrame;

void sdl_init(int width, int height, bool fullscreen) {
  sdlCurrentFrame = sdlNextFrame = 0;
  if (SDL_CreateWindowAndRenderer(width, height, SDL_WINDOW_FULLSCREEN, &window, &renderer) != 0) {
      return 1;
  }
  bmp = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, width, height);
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

void sdl_loop() {
  SDL_Event event;
  while(!done && SDL_WaitEvent(&event)) {
      if (event.type == SDL_QUIT)
        done = true;
      else if (event.type == SDL_USEREVENT) {
        if (event.user.code == SDL_CODE_FRAME) {
          if (++sdlCurrentFrame <= sdlNextFrame - SDL_BUFFER_FRAMES) {
            //Skip frame
          } else if (SDL_LockMutex(mutex) == 0) {
            Uint8** data = ((Uint8**) event.user.data1);
            int* linesize = ((int*) event.user.data2);
            SDL_UpdateYUVTexture(bmp, NULL, data[0], linesize[0], data[1], linesize[1], data[2], linesize[2]);
            SDL_UnlockMutex(mutex);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, bmp, NULL, NULL);
            SDL_RenderPresent(renderer);
          } else
            fprintf(stderr, "Couldn't lock mutex\n");
        }
      }
    }
    SDL_DestroyWindow(window);
}
