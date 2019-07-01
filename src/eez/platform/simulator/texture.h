/*
 * EEZ Generic Firmware
 * Copyright (C) 2018-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>

#include <SDL.h>
#include <SDL_image.h>

/// Image and font texture that can be rendered using SDL_Renderer.
class Texture {
  public:
    // Initializes variables
    Texture();

    // Deallocates memory
    ~Texture();

    // Creates image from image buffer
    bool loadFromImageBuffer(unsigned char *image_buffer, int width, int height,
                             SDL_Renderer *renderer);

    // Deallocates texture
    void free();

    // Renders texture at given point
    void render(SDL_Renderer *renderer, int x, int y, int w, int h);

  private:
    // The actual hardware texture
    SDL_Texture *mTexture;

    // Image dimensions
    int mWidth;
    int mHeight;
};
