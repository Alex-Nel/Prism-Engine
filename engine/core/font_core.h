#ifndef FONT_H
#define FONT_H


#include "external/stb_truetype.h"
#include "mesh_core.h"
#include "image_core.h"
#include <stdbool.h>



// Atlas value of a glyph edge in SDF fonts. Shaders sampling SDF atlases must use the same threshold (128 / 255).
#define FONT_SDF_ONEDGE 128


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

	bool is_sdf;
} Font;





#define MAX_TEXT_LENGTH 256



// Horizontal placement of each text line relative to the origin
typedef enum TextHorizontalAlign
{
	TEXT_ALIGN_H_LEFT = 0,
	TEXT_ALIGN_H_CENTER = 1,
	TEXT_ALIGN_H_RIGHT = 2
} TextHorizontalAlign;



// Vertical placement of the whole text block relative to the origin
typedef enum TextVerticalAlign
{
	TEXT_ALIGN_V_TOP = 0,
	TEXT_ALIGN_V_CENTER = 1,
	TEXT_ALIGN_V_BOTTOM = 2
} TextVerticalAlign;



// Describes a piece of text and how it should look. Field order is mirrored by Prism::TextScope.
typedef struct TextScope
{
	char text[MAX_TEXT_LENGTH];
	Color color;
	Font* font;
	float font_size;
} TextScope;





// Bakes ASCII 32-126 into an RGBA atlas.
bool Font_Bake(Font* font, const unsigned char* ttf_data, int ttf_size, float pixel_height, const char* name, ImageData* out_atlas);

// Loads a TTF from disk and bakes it. Caller must Image_Free the atlas.
bool Font_BakeBitmapFromFile(Font* font, const char* filepath, float pixel_height, ImageData* out_atlas);

// Bakes ASCII 32-126 as signed distance fields, which stay sharp when magnified far past pixel_height.
bool Font_BakeSDF(Font* font, const unsigned char* ttf_data, int ttf_size, float pixel_height, const char* name, ImageData* out_atlas);

// Loads a TTF from disk and bakes it as signed distance fields. Caller must Image_Free the atlas.
bool Font_BakeSDFFromFile(Font* font, const char* filepath, float pixel_height, ImageData* out_atlas);

// Advances xpos/ypos along the baseline and returns a screen-space glyph quad. Scale is relative to the baked pixel size (1 = baked size).
bool Font_GetGlyphQuad(const Font* font, int codepoint, float* xpos, float* ypos, float scale, float* x0, float* y0, float* x1, float* y1, float* u0, float* v0, float* u1, float* v1);

// Measures unwrapped text. If wrap_width > 0, wraps on spaces and reports wrapped size.
void Font_MeasureText(const Font* font, const char* text, float font_size, float wrap_width, float* out_width, float* out_height);

// Resets a text scope to empty white text with no font at a 32px font size
void TextScope_Init(TextScope* scope);

// Copies text into the scope, truncating anything past MAX_TEXT_LENGTH - 1 characters. NULL clears the text.
void TextScope_SetText(TextScope* scope, const char* text);

// Builds one quad per visible glyph in the XY plane facing +Z, positioned around the origin by the alignment.
uint32_t Font_BuildTextMesh(const TextScope* scope, TextHorizontalAlign horizontal_align, TextVerticalAlign vertical_align, float units_per_pixel, Vertex3D* out_vertices, uint32_t* out_indices, uint32_t max_glyphs);





#endif // FONT_H