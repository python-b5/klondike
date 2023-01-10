/* wrapper
 * by python-b5
 *
 * A small wrapper around SDL2, providing basic sprite and text drawing
 * capabilities.
 */


// standard libraries
#include <string>
#include <vector>

// external libraries
#include <SDL2/SDL.h>


#ifndef WRAPPER
    #define WRAPPER

    namespace wrapper {
        // structs/classes
        struct Color {
            uint8_t r, g, b;
            uint8_t a;

            Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255);
        };

        class Sprite {
            SDL_Texture *texture;
            int width;
            int height;

            public:
                Sprite();
                Sprite(std::string file);
                Sprite(SDL_Surface *surface);

                void draw(int x, int y);

                int get_width() const;
                int get_height() const;
        };

        class BBox {
            public:
                int x1, y1;
                int x2, y2;

                BBox();
                BBox(int x1_, int y1_, int x2_, int y2_);

                bool collision(const BBox &other);
                bool collision(int point_x, int point_y);
        };


        // functions
        bool initialize(
            int width, int height, int fps,
            std::string title, std::string icon
        );

        void quit();
        bool update();

        void clear(const Color &color = Color(0, 0, 0, 0));

        bool mouse_down();
        bool mouse_clicked();
        int get_mouse_x();
        int get_mouse_y();
    }
#endif