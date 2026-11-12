#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "font_core.h"
#include "io_core.h"
#include "log_core.h"
#include <stdlib.h>
#include <string.h>





static bool Font_ExpandAtlas(const unsigned char* alpha, int width, int height, ImageData* out_atlas)
{
	unsigned char* rgba = (unsigned char*)malloc((size_t)width * (size_t)height * 4);
	if (!rgba)
		return false;

	for (int i = 0; i < width * height; i++)
	{
		rgba[i * 4 + 0] = 255;
		rgba[i * 4 + 1] = 255;
		rgba[i * 4 + 2] = 255;
		rgba[i * 4 + 3] = alpha[i];
	}

	out_atlas->pixels = rgba;
	out_atlas->width = width;
	out_atlas->height = height;
	out_atlas->channels = 4;

	return true;
}





bool Font_Bake(Font* font, const unsigned char* ttf_data, int ttf_size, float pixel_height, const char* name, ImageData* out_atlas)
{
	if (!font || !ttf_data || ttf_size <= 0 || pixel_height <= 0.0f || !out_atlas)
		return false;

	memset(font, 0, sizeof(Font));
	if (name)
		snprintf(font->name, MAX_NAME_LENGTH, "%s", name);

	font->size = pixel_height;

	int atlas_w = 512;
	int atlas_h = 512;
	unsigned char* bitmap = NULL;
	int bake_result = -1;

	for (int attempt = 0; attempt < 3; attempt++)
	{
		free(bitmap);
		bitmap = (unsigned char*)malloc((size_t)atlas_w * (size_t)atlas_h);
		if (!bitmap)
			return false;
		memset(bitmap, 0, (size_t)atlas_w * (size_t)atlas_h);

		bake_result = stbtt_BakeFontBitmap(ttf_data, 0, pixel_height, bitmap, atlas_w, atlas_h, 32, 96, font->cdata);
		if (bake_result > 0)
			break;

		atlas_w *= 2;
		atlas_h *= 2;
	}

	if (bake_result <= 0)
	{
		free(bitmap);
		Log_Error("ERROR: Failed to bake font atlas");
		return false;
	}

	if (!Font_ExpandAtlas(bitmap, atlas_w, atlas_h, out_atlas))
	{
		free(bitmap);
		return false;
	}
	free(bitmap);

	font->atlas_width = atlas_w;
	font->atlas_height = atlas_h;

	stbtt_fontinfo info;
	if (stbtt_InitFont(&info, ttf_data, 0))
	{
		int ascent = 0, descent = 0, line_gap = 0;
		stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
		float scale = stbtt_ScaleForPixelHeight(&info, pixel_height);
		font->ascent = (float)ascent * scale;
		font->line_height = (float)(ascent - descent + line_gap) * scale;
	}
	else
	{
		font->ascent = pixel_height * 0.8f;
		font->line_height = pixel_height;
	}

	(void)ttf_size;

	return true;
}





// Packs glyph rectangles into rows with a 1px gutter so filtering never pulls in a neighbouring glyph
static bool Font_PackSDFGlyphs(Font* font, const int* widths, const int* heights, int atlas_w, int atlas_h)
{
	int x = 1;
	int y = 1;
	int row_height = 0;

	for (int i = 0; i < 96; i++)
	{
		if (widths[i] <= 0 || heights[i] <= 0)
			continue;

		if (x + widths[i] + 1 > atlas_w)
		{
			x = 1;
			y += row_height + 1;
			row_height = 0;
		}
		
		if (widths[i] + 2 > atlas_w || y + heights[i] + 1 > atlas_h)
			return false;
		
		font->cdata[i].x0 = (unsigned short)x;
		font->cdata[i].y0 = (unsigned short)y;
		font->cdata[i].x1 = (unsigned short)(x + widths[i]);
		font->cdata[i].y1 = (unsigned short)(y + heights[i]);
		
		x += widths[i] + 1;
		if (heights[i] > row_height)
			row_height = heights[i];
	}

	return true;
}





bool Font_BakeSDF(Font* font, const unsigned char* ttf_data, int ttf_size, float pixel_height, const char* name, ImageData* out_atlas)
{
	if (!font || !ttf_data || ttf_size <= 0 || pixel_height <= 0.0f || !out_atlas)
		return false;

	stbtt_fontinfo info;
	if (!stbtt_InitFont(&info, ttf_data, stbtt_GetFontOffsetForIndex(ttf_data, 0)))
	{
		Log_Error("ERROR: Failed to read font data for SDF baking");
		return false;
	}
	
	memset(font, 0, sizeof(Font));
	if (name)
		snprintf(font->name, MAX_NAME_LENGTH, "%s", name);
	
	font->size = pixel_height;
	font->is_sdf = true;
	
	// Padding is how far outside the glyph edge the distance field reaches, which bounds how soft or thick an edge can get
	float scale = stbtt_ScaleForPixelHeight(&info, pixel_height);
	int padding = (int)(pixel_height * 0.125f + 0.5f);
	if (padding < 4)
		padding = 4;

	float pixel_dist_scale = (float)FONT_SDF_ONEDGE / (float)padding;
	
	unsigned char* glyphs[96] = {0};
	int widths[96] = {0};
	int heights[96] = {0};
	
	for (int i = 0; i < 96; i++)
	{
		int xoff = 0, yoff = 0;
		glyphs[i] = stbtt_GetCodepointSDF(&info, scale, 32 + i, padding, FONT_SDF_ONEDGE, pixel_dist_scale, &widths[i], &heights[i], &xoff, &yoff);
		if (!glyphs[i])
			widths[i] = heights[i] = 0;

		int advance = 0, left_bearing = 0;
		stbtt_GetCodepointHMetrics(&info, 32 + i, &advance, &left_bearing);
		
		font->cdata[i].xoff = (float)xoff;
		font->cdata[i].yoff = (float)yoff;
		font->cdata[i].xadvance = (float)advance * scale;
	}

	int atlas_w = 512;
	int atlas_h = 512;
	bool packed = false;
	for (int attempt = 0; attempt < 4 && !packed; attempt++)
	{
		packed = Font_PackSDFGlyphs(font, widths, heights, atlas_w, atlas_h);
		if (!packed)
		{
			atlas_w *= 2;
			atlas_h *= 2;
		}
	}

	unsigned char* bitmap = packed ? (unsigned char*)calloc((size_t)atlas_w * (size_t)atlas_h, 1) : NULL;
	if (bitmap)
	{
		for (int i = 0; i < 96; i++)
		{
			if (!glyphs[i])
				continue;

			for (int row = 0; row < heights[i]; row++)
				memcpy(bitmap + (size_t)(font->cdata[i].y0 + row) * atlas_w + font->cdata[i].x0, glyphs[i] + (size_t)row * widths[i], (size_t)widths[i]);
		}
	}

	for (int i = 0; i < 96; i++)
	{
		if (glyphs[i])
			stbtt_FreeSDF(glyphs[i], NULL);
	}
	
	if (!bitmap)
	{
		Log_Error("ERROR: Failed to build SDF font atlas");
		return false;
	}

	bool expanded = Font_ExpandAtlas(bitmap, atlas_w, atlas_h, out_atlas);
	free(bitmap);
	if (!expanded)
		return false;
	
	font->atlas_width = atlas_w;
	font->atlas_height = atlas_h;
	
	int ascent = 0, descent = 0, line_gap = 0;
	stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
	font->ascent = (float)ascent * scale;
	font->line_height = (float)(ascent - descent + line_gap) * scale;
	
	return true;
}





static bool Font_BakeFileWith(Font* font, const char* filepath, float pixel_height, ImageData* out_atlas, bool sdf)
{
	int size = 0;
	unsigned char* data = IO_ReadBinaryFile(filepath, &size);
	if (!data)
		return false;

	const char* name = filepath;
	const char* slash = strrchr(filepath, '/');
	const char* bslash = strrchr(filepath, '\\');
	if (bslash && (!slash || bslash > slash))
		slash = bslash;
	if (slash)
		name = slash + 1;

	bool ok = false;
	if (sdf)
		ok = Font_BakeSDF(font, data, size, pixel_height, name, out_atlas);
	else
		ok = Font_Bake(font, data, size, pixel_height, name, out_atlas);

	free(data);

	return ok;
}





bool Font_BakeFromFile(Font* font, const char* filepath, float pixel_height, ImageData* out_atlas)
{
	return Font_BakeFileWith(font, filepath, pixel_height, out_atlas, false);
}





bool Font_BakeSDFFromFile(Font* font, const char* filepath, float pixel_height, ImageData* out_atlas)
{
	return Font_BakeFileWith(font, filepath, pixel_height, out_atlas, true);
}





bool Font_GetGlyphQuad(const Font* font, int codepoint, float* xpos, float* ypos, float scale, float* x0, float* y0, float* x1, float* y1, float* u0, float* v0, float* u1, float* v1)
{
	if (!font || !xpos || !ypos || codepoint < 32 || codepoint > 126)
		return false;

	if (scale <= 0.0f)
		scale = 1.0f;

	float start_x = *xpos;
	float start_y = *ypos;
	float x = *xpos;
	float y = *ypos;
	stbtt_aligned_quad q;
	
	if (font->is_sdf)
	{
		// stbtt_GetBakedQuad snaps to whole pixels, which shows up as uneven spacing once SDF text is magnified
		const stbtt_bakedchar* glyph = &font->cdata[codepoint - 32];
		q.x0 = x + glyph->xoff;
		q.y0 = y + glyph->yoff;

		q.x1 = q.x0 + (float)(glyph->x1 - glyph->x0);
		q.y1 = q.y0 + (float)(glyph->y1 - glyph->y0);
		
		q.s0 = (float)glyph->x0 / (float)font->atlas_width;
		q.t0 = (float)glyph->y0 / (float)font->atlas_height;
		q.s1 = (float)glyph->x1 / (float)font->atlas_width;
		q.t1 = (float)glyph->y1 / (float)font->atlas_height;
		
		x += glyph->xadvance;
	}
	else
	{
		stbtt_GetBakedQuad(font->cdata, font->atlas_width, font->atlas_height, codepoint - 32, &x, &y, &q, 1);
	}

	*x0 = start_x + (q.x0 - start_x) * scale;
	*y0 = start_y + (q.y0 - start_y) * scale;
	*x1 = start_x + (q.x1 - start_x) * scale;
	*y1 = start_y + (q.y1 - start_y) * scale;
	*u0 = q.s0;
	*v0 = q.t0;
	*u1 = q.s1;
	*v1 = q.t1;
	*xpos = start_x + (x - start_x) * scale;
	*ypos = start_y + (y - start_y) * scale;

	return true;
}





static float Font_GlyphAdvance(const Font* font, int codepoint, float scale)
{
	float x = 0.0f;
	float y = 0.0f;
	float x0, y0, x1, y1, u0, v0, u1, v1;
	
	if (!Font_GetGlyphQuad(font, codepoint, &x, &y, scale, &x0, &y0, &x1, &y1, &u0, &v0, &u1, &v1))
		return 0.0f;
	
	return x;
}





void Font_MeasureText(const Font* font, const char* text, float font_size, float wrap_width, float* out_width, float* out_height)
{
	if (out_width)
		*out_width = 0.0f;
	if (out_height)
		*out_height = 0.0f;
	if (!font || !text)
		return;

	float scale = (font->size > 0.0f) ? (font_size / font->size) : 1.0f;
	float line_height = font->line_height * scale;
	if (line_height <= 0.0f)
		line_height = font_size;

	float line_width = 0.0f;
	float max_width = 0.0f;
	float height = line_height;
	float word_width = 0.0f;

	for (const char* p = text; *p; p++)
	{
		unsigned char c = (unsigned char)*p;
		if (c == '\n')
		{
			if (line_width > max_width)
				max_width = line_width;
			line_width = 0.0f;
			word_width = 0.0f;
			height += line_height;
			continue;
		}

		float advance = Font_GlyphAdvance(font, c, scale);
		if (wrap_width > 0.0f && c == ' ')
		{
			if (line_width + word_width + advance > wrap_width && line_width > 0.0f)
			{
				if (line_width > max_width)
					max_width = line_width;
				line_width = 0.0f;
				height += line_height;
			}
			line_width += word_width + advance;
			word_width = 0.0f;
			continue;
		}

		if (wrap_width > 0.0f)
		{
			if (line_width + word_width + advance > wrap_width && (line_width + word_width) > 0.0f)
			{
				if (line_width > max_width)
					max_width = line_width;
				line_width = 0.0f;
				height += line_height;
			}
			word_width += advance;
		}
		else
		{
			line_width += advance;
		}
	}

	line_width += word_width;
	if (line_width > max_width)
		max_width = line_width;

	if (out_width)
		*out_width = max_width;
	if (out_height)
		*out_height = height;
}






// Resets a text scope to empty white text with no font at a 32px font size
void TextScope_Init(TextScope* scope)
{
	if (!scope)
		return;

	scope->text[0] = '\0';
	scope->color = (Color){1.0f, 1.0f, 1.0f, 1.0f};
	scope->font = NULL;
	scope->font_size = 32.0f;
}





// Copies text into the scope, truncating anything past MAX_TEXT_LENGTH - 1 characters. NULL clears the text.
void TextScope_SetText(TextScope* scope, const char* text)
{
	if (!scope)
		return;

	if (!text)
	{
		scope->text[0] = '\0';
		return;
	}
	
	size_t length = strlen(text);
	if (length > MAX_TEXT_LENGTH - 1)
		length = MAX_TEXT_LENGTH - 1;
	
	// memmove so a substring of the scope's own buffer can be passed in
	memmove(scope->text, text, length);
	scope->text[length] = '\0';
}





// Builds one quad per visible glyph in the XY plane facing +Z, positioned around the origin by the alignment.
uint32_t Font_BuildTextMesh(const TextScope* scope, TextHorizontalAlign horizontal_align, TextVerticalAlign vertical_align, float units_per_pixel, Vertex3D* out_vertices, uint32_t* out_indices, uint32_t max_glyphs)
{
	if (!scope || !scope->font || !out_vertices || !out_indices || max_glyphs == 0)
		return 0;

	const Font* font = scope->font;
	float scale = (font->size > 0.0f) ? (scope->font_size / font->size) : 1.0f;
	float line_height = font->line_height * scale;
	if (line_height <= 0.0f)
		line_height = scope->font_size;
	
	uint32_t line_count = 1;
	for (const char* p = scope->text; *p; p++)
	{
		if (*p == '\n')
			line_count++;
	}

	// Layout happens in Y-down pixel space with the top of the block at y = 0
	float block_height = line_height * (float)line_count;
	float offset_y = 0.0f;
	if (vertical_align == TEXT_ALIGN_V_CENTER)
		offset_y = -block_height * 0.5f;
	else if (vertical_align == TEXT_ALIGN_V_BOTTOM)
		offset_y = -block_height;

	uint32_t glyph_count = 0;
	float baseline = font->ascent * scale + offset_y;
	const char* line_start = scope->text;
	
	while (glyph_count < max_glyphs)
	{
		const char* line_end = line_start;
		float line_width = 0.0f;

		while (*line_end && *line_end != '\n')
		{
			line_width += Font_GlyphAdvance(font, (unsigned char)*line_end, scale);
			line_end++;
		}
		
		float cursor_x = 0.0f;
		if (horizontal_align == TEXT_ALIGN_H_CENTER)
			cursor_x = -line_width * 0.5f;
		else if (horizontal_align == TEXT_ALIGN_H_RIGHT)
			cursor_x = -line_width;
		
		for (const char* c = line_start; c < line_end && glyph_count < max_glyphs; c++)
		{
			float x0, y0, x1, y1, u0, v0, u1, v1;
			float ypos = baseline;
			if (!Font_GetGlyphQuad(font, (unsigned char)*c, &cursor_x, &ypos, scale, &x0, &y0, &x1, &y1, &u0, &v0, &u1, &v1))
				continue;

			// Whitespace only advances the cursor
			if (x1 <= x0 || y1 <= y0)
				continue;
			
			// Pixel space is Y-down while world space is Y-up, so pixel y0 (glyph top) becomes the higher world y
			float left = x0 * units_per_pixel;
			float right = x1 * units_per_pixel;
			float top = -y0 * units_per_pixel;
			float bottom = -y1 * units_per_pixel;
			
			uint32_t base = glyph_count * 4;
			Vertex3D* v = &out_vertices[base];
			v[0] = (Vertex3D){ {left,  bottom, 0.0f}, {0.0f, 0.0f, 1.0f}, {u0, v1}, {1.0f, 0.0f, 0.0f} };
			v[1] = (Vertex3D){ {right, bottom, 0.0f}, {0.0f, 0.0f, 1.0f}, {u1, v1}, {1.0f, 0.0f, 0.0f} };
			v[2] = (Vertex3D){ {right, top,    0.0f}, {0.0f, 0.0f, 1.0f}, {u1, v0}, {1.0f, 0.0f, 0.0f} };
			v[3] = (Vertex3D){ {left,  top,    0.0f}, {0.0f, 0.0f, 1.0f}, {u0, v0}, {1.0f, 0.0f, 0.0f} };
			
			uint32_t* idx = &out_indices[glyph_count * 6];
			idx[0] = base;     idx[1] = base + 1; idx[2] = base + 2;
			idx[3] = base + 2; idx[4] = base + 3; idx[5] = base;

			glyph_count++;
		}

		if (*line_end == '\0')
			break;
		
		line_start = line_end + 1;
		baseline += line_height;
	}
	
	return glyph_count;
}