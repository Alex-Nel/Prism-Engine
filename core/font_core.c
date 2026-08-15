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
		strncpy(font->name, name, MAX_NAME_LENGTH - 1);

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





bool Font_BakeFromFile(Font* font, const char* filepath, float pixel_height, ImageData* out_atlas)
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

	bool ok = Font_Bake(font, data, size, pixel_height, name, out_atlas);
	free(data);

	return ok;
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
	stbtt_GetBakedQuad(font->cdata, font->atlas_width, font->atlas_height, codepoint - 32, &x, &y, &q, 1);

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