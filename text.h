#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H

typedef unsigned char u8;

FT_Library  library;
FT_Face     face;

#define PIXEL_BUFFER_CAP 1000000
u8 pixel_buffer[PIXEL_BUFFER_CAP];
long pixel_buffer_count;

#define GLYPH_COUNT 256
struct Glyph {
	u8 *pixels;
	int width;
	int height;
};
struct Glyph glyphs[GLYPH_COUNT];
typedef struct Glyph *Glyph;

void load_glyph(Glyph glyph, FT_Bitmap *glyph_bitmap) {
	int width = glyph_bitmap->width;
	int height = glyph_bitmap->rows;
	glyph->width = width;
	glyph->height = height;

	glyph->pixels = &pixel_buffer[pixel_buffer_count];
	pixel_buffer_count += width * height;
	if (pixel_buffer_count > PIXEL_BUFFER_CAP) {
		fprintf(stderr, "ERROR: reached end of pixel buffer\n");
		exit(1);
	}

	u8 *in_row = glyph_bitmap->buffer;
	u8 *out_row = glyph->pixels;
	for (int i = 0; i < height; i++) {
		memcpy(out_row, in_row, width);
		in_row += glyph_bitmap->pitch;
		out_row += width;
	}
}

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

	for (int i = 0; i < GLYPH_COUNT; i++) {
		/* load glyph image into the slot (erase previous one) */
		error = FT_Load_Char(face, i, FT_LOAD_RENDER);
		if (error)
			continue;  /* ignore errors */

		/* now, draw to our target surface */
		load_glyph(&glyphs[i], &face->glyph->bitmap);
	}
}

void test_text() {
	Glyph glyph = &glyphs['&'];

	FILE *out = fopen("text.pgm", "w");

	fprintf(out, "P2 %d %d %d\n", glyph->width, glyph->height, 255);

	u8 *row = glyph->pixels;
	for (int j = 0; j < glyph->height; j++) {
		for (int i = 0; i < glyph->width; i++) {
			fprintf(out, "%d ", 255 - row[i]);
		}
		fprintf(out, "\n");
		row += glyph->width;
	}

	fclose(out);
}
