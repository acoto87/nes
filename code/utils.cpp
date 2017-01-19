#include "utils.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

stbtt_fontinfo LoadFontFile(char *fontFilePath)
{
    LoadedFile fontFile = LoadEntireFile(fontFilePath);

    stbtt_fontinfo fontInfo;

    s32 offset = stbtt_GetFontOffsetForIndex(fontFile.contents, 0);
    stbtt_InitFont(&fontInfo, fontFile.contents, offset);

    return fontInfo;
}

LoadedBitmap LoadFontGlyph(stbtt_fontinfo fontInfo, s32 codepoint, f32 fontSize)
{
    s32 width, height, xoffset, yoffset;
    f32 scaleY = stbtt_ScaleForPixelHeight(&fontInfo, fontSize);
    u8 *glyphBitmap = stbtt_GetCodepointBitmap(&fontInfo, 0, scaleY, codepoint, &width, &height, &xoffset, &yoffset);

    LoadedBitmap bitmap = {};
    bitmap.width = width;
    bitmap.height = height;
    bitmap.pixels = (Color*)Allocate(width * height * sizeof(Color));

    for (s32 y = 0; y < height; ++y)
    {
        for (s32 x = 0; x < width; ++x)
        {
            s32 pixel = y * width + x;
            u8 alpha = glyphBitmap[pixel];
            bitmap.pixels[pixel].argb = (alpha << 24) | (alpha << 16) | (alpha << 8) | (alpha << 0);
        }
    }

    stbtt_FreeBitmap(glyphBitmap, NULL);

    return bitmap;
}