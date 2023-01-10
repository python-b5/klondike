/* wrapper
 * by python-b5
 *
 * A small wrapper around SDL2, providing basic sprite and text drawing
 * capabilities.
 */


// project includes
#include "wrapper.hpp"

// standard libraries
#include <string>

// external libraries
#include <SDL2/SDL.h>

// using declarations
using namespace wrapper;


// global variables
bool initialized = false;

SDL_Window *window;
SDL_Renderer *renderer;
bool refreshed;

std::vector<SDL_Texture *> textures;

unsigned int frame_time;
uint32_t last_frame = 0;

int mouse_x;
int mouse_y;
bool lmb_state;
bool lmb_state_last = false;



/* Color implementation:
 * A 32-bit color in the RGBA format.
 */

Color::Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_) :
    r(r_), g(g_), b(b_),
    a(a_)
{}


/* Sprite implementation:
 * A wrapper around an SDL texture.
 */

Sprite::Sprite() {}

Sprite::Sprite(std::string file) {
    // create surface and convert to texture (keying out the background)
    // (all the bitmaps I'm using in this project use #FF00FF as the background
    // color, so I'm hardcoding it in because I'm lazy)
    // (also I know I should have used PNGs but I'm also too lazy to install
    // SDL_image)
    SDL_Surface *temp = SDL_LoadBMP(file.c_str());
    SDL_SetColorKey(temp, SDL_TRUE, SDL_MapRGB(temp->format, 255, 0, 255));
    texture = SDL_CreateTextureFromSurface(renderer, temp);
    SDL_FreeSurface(temp);

    // query size
    SDL_QueryTexture(texture, NULL, NULL, &width, &height);

    // add texture to vector so it can be freed later
    textures.push_back(texture);
}

/* Draws the Sprite at a given position. */
void Sprite::draw(int x, int y) {
    // create rect
    SDL_Rect rect;

    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;

    // draw texture
    SDL_RenderCopy(renderer, texture, NULL, &rect);
}

/* Returns the width of the Sprite. */
int Sprite::get_width() const {
    return width;
}

/* Returns the height of the Sprite. */
int Sprite::get_height() const {
    return height;
}


/* BBox implementation:
 * A box used to check collisions.
 */

BBox::BBox() {}

BBox::BBox(int x1_, int y1_, int x2_, int y2_):
    x1(x1_), y1(y1_),
    x2(x2_), y2(y2_)
{}

/* Checks if another BBox overlaps this one. */
bool BBox::collision(const BBox &other) {
    return other.x1 <= x2 && other.x2 >= x1
        && other.y1 <= y2 && other.y2 >= y1;
}

/* Checks if a point is inside the BBox. */
bool BBox::collision(int point_x, int point_y) {
    return point_x >= x1 && point_y >= y1
        && point_x <= x2 && point_y <= y2;
}


/* functions */

/* Initializes SDL and creates necessary resources.
 * Returns whether it was successful.
 */
bool wrapper::initialize(
    int width, int height, int fps,
    std::string title, std::string icon
) {
    if (!initialized) {
        // initialize SDL
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
            return false;
        }

        // create window
        window = SDL_CreateWindow(
            title.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height,
            SDL_WINDOW_HIDDEN
        );

        if (window == NULL) {
            return false;
        }

        // set icon
        SDL_Surface *temp = SDL_LoadBMP(icon.c_str());
        SDL_SetWindowIcon(window, temp);
        SDL_FreeSurface(temp);

        // create renderer
        renderer = SDL_CreateRenderer(
            window,
            -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
        );

        if (renderer == NULL) {
            return false;
        }

        // set screen refresh values
        refreshed = false;
        frame_time = 1000 / fps;

        // get initial mouse state
        lmb_state = SDL_GetMouseState(&mouse_x, &mouse_y);

        initialized = true;

        return true;
    } else {
        return false;
    }
}

/* Frees memory and quits SDL. */
void wrapper::quit() {
    if (initialized) {
        // destroy sprites' textures
        for (SDL_Texture *texture : textures) {
            SDL_DestroyTexture(texture);
        }

        // free remaining resources
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);

        // quit SDL
        SDL_Quit();
        initialized = false;
    }
}

/* Waits for the next frame, refreshes the screen, and handles events.
 * Returns whether the window was closed.
 */
bool wrapper::update() {
    if (refreshed) {
        // wait until next frame
        uint32_t current = SDL_GetTicks();

        while (current - last_frame < frame_time) {
            current = SDL_GetTicks();
        }

        last_frame = current;
    } else {
        // show window if this is the first refresh
        SDL_ShowWindow(window);
        refreshed = true;
    }

    // present renderer
    SDL_RenderPresent(renderer);

    // handle events
    SDL_Event event;

    while ((SDL_PollEvent(&event)) != 0) {
        if (event.type == SDL_QUIT) {
            // hide the window if it was closed
            SDL_HideWindow(window);
            return true;
        }
    }

    // get mouse state
    lmb_state_last = lmb_state;
    lmb_state = SDL_GetMouseState(&mouse_x, &mouse_y);

    // window was not closed
    return false;
}

/* Clears the screen, optionally filling it with a color. */
void wrapper::clear(const Color &color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
}

/* Returns whether the left mouse button is down. */
bool wrapper::mouse_down() {
    return lmb_state;
}

/* Returns whether the left mouse button was clicked this frame. */
bool wrapper::mouse_clicked() {
    return lmb_state && !lmb_state_last;
}

/* Returns the X position of the mouse cursor. */
int wrapper::get_mouse_x() {
    return mouse_x;
}

/* Returns the Y position of the mouse cursor. */
int wrapper::get_mouse_y() {
    return mouse_y;
}