/**
 * Class to output to the screen
 *
 * @author Ernesto Martínez García <me@ecomaikgolf.com>
 */

#pragma once

#include "colors.h"
#include "framebuffer.h"
#include <stdarg.h>
#include <stdint.h>

namespace screen {

/**
 * Interface to output to the screen
 */
class fast_renderer_i
{
  public:
    fast_renderer_i() = default;
    void fmt(const char *, ...);
    void println(const char *);
    void printcharln(char letter)
    {
        char aux[] = { letter, '\0' };
        this->println(aux);
    }
    void print(const char *, int64_t n = -1);
    void printchar(char letter)
    {
        char aux[] = { letter, '\0' };
        this->print(aux);
    };
    void newline()
    {
        this->println("");
    };
    void clear();
    void scroll();
    virtual void draw(const char) = 0;
    void put(const char);
    void setColor(color_e);
    color_e getColor();
    unsigned int get_x();
    unsigned int get_y();
    void set_x(unsigned int);
    void set_y(unsigned int);

  protected:
    void update_video();
    void update_line();
    /** Video Memory */
    framebuffer video_memory;
    /** Cache (double buffer) */
    cache_framebuffer video_cache;
    /** x PIXEL offset of next glyph */
    unsigned int x_offset;
    /** y PIXEL offset of next glyph */
    unsigned int y_offset;
    /** renderer glyph x size */
    virtual unsigned int glyph_x() = 0;
    /** renderer glyph y size */
    virtual unsigned int glyph_y() = 0;
    /** color of next glyph */
    color_e color;
};

} // namespace screen
