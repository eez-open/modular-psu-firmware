/*
 * EEZ Middleware
 * Copyright (C) 2015-present, Envox d.o.o.
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

#include "texture.h"

Texture::Texture() {
    // Initialize
    mTexture = NULL;
    mWidth = 0;
    mHeight = 0;
}

Texture::~Texture() {
    // Deallocate
    free();
}

bool Texture::loadFromImageBuffer(unsigned char *image_buffer, int width, int height,
                                  SDL_Renderer *renderer) {
    // Get rid of preexisting texture
    free();

    SDL_Surface *rgbSurface =
        SDL_CreateRGBSurfaceFrom(image_buffer, width, height, 32, 4 * width, 0, 0, 0, 0);
    if (rgbSurface != NULL) {
        // Create texture from surface pixels
        mTexture = SDL_CreateTextureFromSurface(renderer, rgbSurface);
        if (mTexture == NULL) {
            printf("Unable to create texture from image buffer! SDL Error: %s\n", SDL_GetError());
        } else {
            // Get image dimensions
            mWidth = rgbSurface->w;
            mHeight = rgbSurface->h;
        }

        // Get rid of old surface
        SDL_FreeSurface(rgbSurface);
    } else {
        printf("Unable to render text surface! SDL Error: %s\n", SDL_GetError());
    }

    // Return success
    return mTexture != NULL;
}

void Texture::free() {
    // Free texture if it exists
    if (mTexture != NULL) {
        SDL_DestroyTexture(mTexture);
        mTexture = NULL;
        mWidth = 0;
        mHeight = 0;
    }
}

void Texture::render(SDL_Renderer *renderer, int x, int y, int w, int h) {
    // Set rendering space and render to screen
    SDL_Rect src_rect = { 0, 0, mWidth, mHeight };
    SDL_Rect dst_rect = { x, y, w, h };

    // Render to screen
    SDL_RenderCopyEx(renderer, mTexture, &src_rect, &dst_rect, 0.0, NULL, SDL_FLIP_NONE);
}
