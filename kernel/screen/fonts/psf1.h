/**
 * Font definitions
 *
 * @author Ernesto Martínez García <me@ecomaikgolf.com>
 */

#pragma once

#include "../colors.h"
#include "../framebuffer.h"
#include "../renderer.h"
#include "screen/fonts/psf1_definition.h"

namespace screen {

/**
 * Renderer with PSF1 fonts
 */
class psf1_renderer : public renderer_i
{
  public:
    psf1_renderer(framebuffer,
                  fonts::psf1,
                  unsigned int x_offset = 0,
                  unsigned int y_offset = 0,
                  color_e color         = color_e::WHITE);
    psf1_renderer()        = default;
    psf1_renderer &operator=(psf1_renderer &&);

    void draw(const char);

  private:
    ///** renderer glyph x size */
    unsigned int glyph_x()
    {
        return fonts::psf1::glyph_x;
    }
    ///** renderer glyph y size */
    unsigned int glyph_y()
    {
        return fonts::psf1::glyph_y;
    }
    /** PSF1 font to use */
    fonts::psf1 font;
};

} // namespace screen
