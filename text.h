#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H

typedef unsigned char u8;

FT_Library  library;
FT_Face     face;

typedef struct {
    u8 b;
    u8 g;
    u8 r;
    u8 a;
} col;

col font_texture_fg;
col font_texture_bg;

#define FONT_TEXTURE_CAP 2500000
col font_texture[FONT_TEXTURE_CAP];
int font_texture_width;
int font_texture_height;
int font_texture_range;
int font_texture_offset;

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

void init_text(col fg, col bg) {
    font_texture_fg = fg;
    font_texture_bg = bg;

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

    error = FT_Set_Pixel_Sizes(face, 0, 16);
    if (error) {
        fprintf(stderr, "ERROR: failed to set char size\n");
        exit(1);
    }

    font_texture_width = 0;
    font_texture_height = 0;
    font_texture_range = 0;
    int max_overhang = 0;
    for (int i = 0; i < GLYPH_COUNT; i++) {
        error = FT_Load_Char(face, i, 0);
        if (error)
            continue;  /* ignore errors */

        FT_Glyph_Metrics *metrics = &face->glyph->metrics;
        glyphs[i].width = metrics->width >> 6;
        glyphs[i].height = metrics->height >> 6;
        glyphs[i].bearingx = metrics->horiBearingX >> 6;
        glyphs[i].bearingy = metrics->horiBearingY >> 6;
        glyphs[i].advance = metrics->horiAdvance >> 6;

        if (glyphs[i].width > font_texture_width) {
            font_texture_width = glyphs[i].width;
        }
        if (glyphs[i].height > font_texture_height) {
            font_texture_height = glyphs[i].height;
        }
        if (glyphs[i].height - glyphs[i].bearingy > max_overhang) {
            max_overhang = glyphs[i].height - glyphs[i].bearingy;
        }
        if (glyphs[i].bearingy > font_texture_offset) {
            font_texture_offset = glyphs[i].bearingy;
        }
    }
    font_texture_range = font_texture_offset + max_overhang;

    /* could really malloc */
    if (font_texture_width * font_texture_height * GLYPH_COUNT > FONT_TEXTURE_CAP) {
        fprintf(stderr, "ERROR: font texture not large enough\n");
        exit(1);
    }

    int glyph_col_count = font_texture_width * font_texture_height;

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

        col *out_row = &font_texture[glyph_col_count * i];
        memset(out_row, 0, glyph_col_count * sizeof(col));
        u8 *in_row = face->glyph->bitmap.buffer;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int a = in_row[x];
                int b = 255 - a;
                out_row[x].r = (a * fg.r + b * bg.r) / 255;
                out_row[x].g = (a * fg.g + b * bg.g) / 255;
                out_row[x].b = (a * fg.b + b * bg.b) / 255;
                out_row[x].a = (a * fg.a + b * bg.a) / 255;
            }
            in_row += width;
            out_row += font_texture_width;
        }
    }
}

