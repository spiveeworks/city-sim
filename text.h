#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H

typedef unsigned char u8;

FT_Library  library;
FT_Face     face;

#define FONT_TEXTURE_CAP 10000000
u8 font_texture[FONT_TEXTURE_CAP];
int font_texture_width;
int font_texture_height;

#define GLYPH_COUNT 256
struct Glyph {
    int width;
    int height;
    int bearingx;
    int bearingy;
    int advance;
};
struct Glyph glyphs[GLYPH_COUNT];
typedef struct Glyph *Glyph;

void init_text() {
    FT_Error error = FT_Init_FreeType(&library);
    if (error) {
        fprintf(stderr, "ERROR: failed to initialise FreeType library\n");
        exit(1);
    }

    error = FT_New_Face(
        library,
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
         0,
         &face
    );
    if (error == FT_Err_Unknown_File_Format) {
        fprintf(stderr, "ERROR: unknown font format\n");
        exit(1);
    } else if (error) {
        fprintf(stderr, "ERROR: failed to load font\n");
        exit(1);
    }

    error = FT_Set_Char_Size(
        face,    /* handle to face object           */
        0,       /* char_width in 1/64th of points  */
        16*64,   /* char_height in 1/64th of points */
        300,     /* horizontal device resolution    */
        300      /* vertical device resolution      */
    );
    if (error) {
        fprintf(stderr, "ERROR: failed to set char size\n");
        exit(1);
    }

    font_texture_width = 0;
    font_texture_height = 0;
    for (int i = 0; i < GLYPH_COUNT; i++) {
        error = FT_Load_Char(face, i, 0);
        if (error)
            continue;  /* ignore errors */

        FT_Glyph_Metrics *metrics = &face->glyph->metrics;
        glyphs[i].width = metrics->width >> 6;
        glyphs[i].height = metrics->height >> 6;
        glyphs[i].bearingx = metrics->horiBearingX;
        glyphs[i].bearingy = metrics->horiBearingY;
        glyphs[i].advance = metrics->horiAdvance;

        if (glyphs[i].width > font_texture_width) {
            font_texture_width = glyphs[i].width;
        }
        if (glyphs[i].height > font_texture_height) {
            font_texture_height = glyphs[i].height;
        }
    }

    /* could really malloc */
    if (font_texture_width * font_texture_height * GLYPH_COUNT > FONT_TEXTURE_CAP) {
        fprintf(stderr, "ERROR: font texture not large enough\n");
        exit(1);
    }

    int glyph_size = font_texture_width * font_texture_height;

    for (int i = 0; i < GLYPH_COUNT; i++) {
        /* load glyph image into the slot (erase previous one) */
        error = FT_Load_Char(face, i, FT_LOAD_RENDER);
        if (error)
            continue;  /* ignore errors */

        /* now, draw to our target surface */
        int width = glyphs[i].width;
        int height = glyphs[i].height;
        if (width != face->glyph->bitmap.width) {
            fprintf(stderr, "glyph width incorrect\n");
            exit(1);
        }
        if (height != face->glyph->bitmap.rows) {
            fprintf(stderr, "glyph height incorrect\n");
            exit(1);
        }

        u8 *start = &font_texture[glyph_size * i];
        memset(start, 0, glyph_size);
        u8 *in_row = face->glyph->bitmap.buffer;
        for (int j = 0; j < height; j++) {
            memcpy(start, in_row, width);
            in_row += width;
            start += font_texture_width;
        }
    }
}

