#ifndef FONT_H
#define FONT_H


#include "../external/stb_truetype.h"
#include "mesh_core.h"
#include "image_core.h"
#include <stdbool.h>


// Font structure for holding baked ASCII data
typedef struct Font
{
	char name[256];

	// The texture atlas (NULL until it's uploaded)
	Texture* texture_atlas;

	// Baked character data for ASCII 32 -> 126
	stbtt_bakedchar cdata[96];

	float size;
	int atlas_width;
	int atlas_height;
	float ascent;
	float line_height;
} Font;





// Bakes ASCII 32-126 into an RGBA atlas.
bool Font_Bake(Font* font, const unsigned char* ttf_data, int ttf_size, float pixel_height, const char* name, ImageData* out_atlas);

// Loads a TTF from disk and bakes it. Caller must Image_Free the atlas.
bool Font_BakeFromFile(Font* font, const char* filepath, float pixel_height, ImageData* out_atlas);

// Advances xpos/ypos along the baseline and returns a screen-space glyph quad. Scale is relative to the baked pixel size (1 = baked size).
bool Font_GetGlyphQuad(const Font* font, int codepoint, float* xpos, float* ypos, float scale, float* x0, float* y0, float* x1, float* y1, float* u0, float* v0, float* u1, float* v1);

// Measures unwrapped text. If wrap_width > 0, wraps on spaces and reports wrapped size.
void Font_MeasureText(const Font* font, const char* text, float font_size, float wrap_width, float* out_width, float* out_height);



#endif // FONT_H