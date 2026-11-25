#include "ui.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

constexpr int32_t COLOR_ROW_BTN_SIZE = 26;
constexpr int32_t COLOR_ROW_BTN_GAP = 3;

static const uint32_t ui_font_extra_codepoints[UI_FONT_EXTRA_COUNT] = {
    0x0490U, /* Ґ */
    0x0491U, /* ґ */
    0x00D7U, /* × */
    0x00B0U, /* ° */
    0x2013U, /* – */
    0x2014U, /* — */
    0x2018U, /* ‘ */
    0x2019U, /* ’ */
    0x2116U, /* № */
    0x2022U, /* • */
    0x2026U, /* … */
    0x2192U  /* → */
};

/* Ordered system UI sans-serif font candidates across common Linux distributions. */
static const char *const system_ui_font_paths[16] = {
    "/usr/share/fonts/noto/NotoSans-Medium.ttf",
    "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Medium.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/google-noto/NotoSans-Medium.ttf",
    "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/Adwaita/AdwaitaSans-Regular.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
    "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
    "/usr/share/fonts/inter/Inter-Regular.ttf",
    "/usr/share/fonts/TTF/Inter-Regular.ttf"
};

const DialogFilterSpec open_dialog_filters[OPEN_DIALOG_FILTER_COUNT] = {
    {"Embroidery files", "jef;pes;pec;dst"},
    {"All files", "*"}
};

const DialogFilterSpec save_dialog_filters[SAVE_DIALOG_FILTER_COUNT] = {
    {"JEF embroidery", "jef"},
    {"All files", "*"}
};

/* Compact 8x8 ASCII bitmap font for code points 32 (' ') through 127 (DEL). */
static const uint8_t bitmap_font_ascii[96][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 32 ' ' */
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, /* 33 '!' */
    {0x36, 0x36, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 34 '"' */
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, /* 35 '#' */
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, /* 36 '$' */
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, /* 37 '%' */
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, /* 38 '&' */
    {0x18, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 39 ''' */
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, /* 40 '(' */
    {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00}, /* 41 ')' */
    {0x00, 0x66, 0x3C, 0x7E, 0x3C, 0x66, 0x00, 0x00}, /* 42 '*' */
    {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00}, /* 43 '+' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x0C}, /* 44 ',' */
    {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00}, /* 45 '-' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, /* 46 '.' */
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, /* 47 '/' */
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, /* 48 '0' */
    {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}, /* 49 '1' */
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}, /* 50 '2' */
    {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00}, /* 51 '3' */
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}, /* 52 '4' */
    {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00}, /* 53 '5' */
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00}, /* 54 '6' */
    {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00}, /* 55 '7' */
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00}, /* 56 '8' */
    {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00}, /* 57 '9' */
    {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00}, /* 58 ':' */
    {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x0C}, /* 59 ';' */
    {0x30, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x30, 0x00}, /* 60 '<' */
    {0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00}, /* 61 '=' */
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, /* 62 '>' */
    {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00}, /* 63 '?' */
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, /* 64 '@' */
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, /* 65 'A' */
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, /* 66 'B' */
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, /* 67 'C' */
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, /* 68 'D' */
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}, /* 69 'E' */
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}, /* 70 'F' */
    {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}, /* 71 'G' */
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, /* 72 'H' */
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* 73 'I' */
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, /* 74 'J' */
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, /* 75 'K' */
    {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}, /* 76 'L' */
    {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}, /* 77 'M' */
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, /* 78 'N' */
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, /* 79 'O' */
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, /* 80 'P' */
    {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}, /* 81 'Q' */
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, /* 82 'R' */
    {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}, /* 83 'S' */
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* 84 'T' */
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00}, /* 85 'U' */
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, /* 86 'V' */
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, /* 87 'W' */
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, /* 88 'X' */
    {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}, /* 89 'Y' */
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, /* 90 'Z' */
    {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00}, /* 91 '[' */
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00}, /* 92 '\' */
    {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00}, /* 93 ']' */
    {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, /* 94 '^' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F}, /* 95 '_' */
    {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 96 '`' */
    {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00}, /* 97 'a' */
    {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00}, /* 98 'b' */
    {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00}, /* 99 'c' */
    {0x38, 0x30, 0x30, 0x3E, 0x33, 0x33, 0x6E, 0x00}, /* 100 'd' */
    {0x00, 0x00, 0x1E, 0x33, 0x3F, 0x03, 0x1E, 0x00}, /* 101 'e' */
    {0x1C, 0x36, 0x06, 0x0F, 0x06, 0x06, 0x0F, 0x00}, /* 102 'f' */
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F}, /* 103 'g' */
    {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00}, /* 104 'h' */
    {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* 105 'i' */
    {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E}, /* 106 'j' */
    {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00}, /* 107 'k' */
    {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* 108 'l' */
    {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, /* 109 'm' */
    {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00}, /* 110 'n' */
    {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00}, /* 111 'o' */
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F}, /* 112 'p' */
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78}, /* 113 'q' */
    {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00}, /* 114 'r' */
    {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00}, /* 115 's' */
    {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00}, /* 116 't' */
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00}, /* 117 'u' */
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, /* 118 'v' */
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00}, /* 119 'w' */
    {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00}, /* 120 'x' */
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F}, /* 121 'y' */
    {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00}, /* 122 'z' */
    {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00}, /* 123 '{' */
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, /* 124 '|' */
    {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00}, /* 125 '}' */
    {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 126 '~' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  /* 127 DEL */
};

/* Cyrillic U+0410..U+042F (А..Я) + U+0430..U+044F (а..я) + Ukrainian Є/є/І/і/Ї/ї/Ґ/ґ. */
static const uint8_t bitmap_font_cyrillic_block[64][8] = {
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, /* А */
    {0x7F, 0x03, 0x03, 0x3F, 0x63, 0x63, 0x3F, 0x00}, /* Б */
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, /* В */
    {0x7F, 0x63, 0x03, 0x03, 0x03, 0x03, 0x07, 0x00}, /* Г */
    {0x3C, 0x36, 0x36, 0x36, 0x36, 0x7F, 0x63, 0x00}, /* Д */
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}, /* Е */
    {0x6B, 0x6B, 0x3E, 0x1C, 0x3E, 0x6B, 0x6B, 0x00}, /* Ж */
    {0x3E, 0x63, 0x60, 0x3C, 0x60, 0x63, 0x3E, 0x00}, /* З */
    {0x63, 0x73, 0x7B, 0x6F, 0x67, 0x63, 0x63, 0x00}, /* И */
    {0x3C, 0x00, 0x63, 0x73, 0x7B, 0x6F, 0x63, 0x00}, /* Й */
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, /* К */
    {0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x63, 0x00}, /* Л */
    {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}, /* М */
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, /* Н */
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, /* О */
    {0x7F, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00}, /* П */
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, /* Р */
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, /* С */
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* Т */
    {0x63, 0x63, 0x63, 0x3E, 0x30, 0x18, 0x0E, 0x00}, /* У */
    {0x1C, 0x7F, 0x6B, 0x6B, 0x7F, 0x1C, 0x1C, 0x00}, /* Ф */
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, /* Х */
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x7F, 0x60, 0x00}, /* Ц */
    {0x63, 0x63, 0x63, 0x7E, 0x60, 0x60, 0x60, 0x00}, /* Ч */
    {0x6B, 0x6B, 0x6B, 0x6B, 0x6B, 0x6B, 0x7F, 0x00}, /* Ш */
    {0x6B, 0x6B, 0x6B, 0x6B, 0x6B, 0x7F, 0x60, 0x00}, /* Щ */
    {0x0F, 0x0C, 0x0C, 0x3E, 0x6C, 0x6C, 0x3E, 0x00}, /* Ъ */
    {0x63, 0x63, 0x63, 0x6F, 0x73, 0x73, 0x6F, 0x00}, /* Ы */
    {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3E, 0x00}, /* Ь */
    {0x3E, 0x63, 0x60, 0x7C, 0x60, 0x63, 0x3E, 0x00}, /* Э */
    {0x3B, 0x6B, 0x6B, 0x6F, 0x6B, 0x6B, 0x3B, 0x00}, /* Ю */
    {0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x63, 0x00}, /* Я */
    {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00}, /* а */
    {0x3C, 0x06, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00}, /* б */
    {0x00, 0x00, 0x3F, 0x66, 0x3E, 0x66, 0x3F, 0x00}, /* в */
    {0x00, 0x00, 0x7F, 0x03, 0x03, 0x03, 0x03, 0x00}, /* г */
    {0x00, 0x00, 0x3C, 0x36, 0x36, 0x7F, 0x63, 0x00}, /* д */
    {0x00, 0x00, 0x1E, 0x33, 0x3F, 0x03, 0x1E, 0x00}, /* е */
    {0x00, 0x00, 0x6B, 0x3E, 0x1C, 0x3E, 0x6B, 0x00}, /* ж */
    {0x00, 0x00, 0x3E, 0x60, 0x3C, 0x60, 0x3E, 0x00}, /* з */
    {0x00, 0x00, 0x63, 0x73, 0x7B, 0x6F, 0x63, 0x00}, /* и */
    {0x3C, 0x00, 0x63, 0x73, 0x7B, 0x6F, 0x63, 0x00}, /* й */
    {0x00, 0x00, 0x67, 0x36, 0x1E, 0x36, 0x67, 0x00}, /* к */
    {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x63, 0x00}, /* л */
    {0x00, 0x00, 0x63, 0x77, 0x7F, 0x6B, 0x63, 0x00}, /* м */
    {0x00, 0x00, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, /* н */
    {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00}, /* о */
    {0x00, 0x00, 0x7F, 0x63, 0x63, 0x63, 0x63, 0x00}, /* п */
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F}, /* р */
    {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00}, /* с */
    {0x00, 0x00, 0x7F, 0x18, 0x18, 0x18, 0x18, 0x00}, /* т */
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F}, /* у */
    {0x00, 0x18, 0x7E, 0x5A, 0x7E, 0x18, 0x18, 0x00}, /* ф */
    {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00}, /* х */
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x7F, 0x60, 0x00}, /* ц */
    {0x00, 0x00, 0x63, 0x63, 0x7E, 0x60, 0x60, 0x00}, /* ч */
    {0x00, 0x00, 0x6B, 0x6B, 0x6B, 0x6B, 0x7F, 0x00}, /* ш */
    {0x00, 0x00, 0x6B, 0x6B, 0x6B, 0x7F, 0x60, 0x00}, /* щ */
    {0x00, 0x00, 0x0F, 0x0C, 0x3E, 0x6C, 0x3E, 0x00}, /* ъ */
    {0x00, 0x00, 0x63, 0x6F, 0x73, 0x73, 0x6F, 0x00}, /* ы */
    {0x00, 0x00, 0x07, 0x06, 0x3E, 0x66, 0x3E, 0x00}, /* ь */
    {0x00, 0x00, 0x3E, 0x60, 0x7C, 0x60, 0x3E, 0x00}, /* э */
    {0x00, 0x00, 0x3B, 0x6B, 0x6F, 0x6B, 0x3B, 0x00}, /* ю */
    {0x00, 0x00, 0x7C, 0x66, 0x7C, 0x6C, 0x63, 0x00}  /* я */
};

static const uint8_t bitmap_font_ukrainian_extra[8][8] = {
    {0x3C, 0x66, 0x03, 0x1F, 0x03, 0x66, 0x3C, 0x00}, /* 0: Є U+0404 */
    {0x00, 0x00, 0x3C, 0x06, 0x3E, 0x06, 0x3C, 0x00}, /* 1: є U+0454 */
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* 2: І U+0406 */
    {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* 3: і U+0456 */
    {0x36, 0x00, 0x1E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* 4: Ї U+0407 */
    {0x36, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* 5: ї U+0457 */
    {0x60, 0x7F, 0x03, 0x03, 0x03, 0x03, 0x07, 0x00}, /* 6: Ґ U+0490 */
    {0x00, 0x60, 0x7F, 0x03, 0x03, 0x03, 0x07, 0x00}  /* 7: ґ U+0491 */
};

static uint32_t decode_utf8_codepoint(const char *text, size_t *offset_in_out) {
    const size_t pos = *offset_in_out;
    const uint8_t first = (uint8_t)text[pos];
    if (first == 0U) {
        return 0U;
    }
    if ((first & 0x80U) == 0U) {
        *offset_in_out = pos + 1U;
        return (uint32_t)first;
    }
    if ((first & 0xE0U) == 0xC0U && text[pos + 1U] != '\0') {
        const uint8_t second = (uint8_t)text[pos + 1U];
        if ((second & 0xC0U) == 0x80U) {
            *offset_in_out = pos + 2U;
            return (((uint32_t)(first & 0x1FU)) << 6U) | (uint32_t)(second & 0x3FU);
        }
    }
    if ((first & 0xF0U) == 0xE0U && text[pos + 1U] != '\0' && text[pos + 2U] != '\0') {
        const uint8_t second = (uint8_t)text[pos + 1U];
        const uint8_t third = (uint8_t)text[pos + 2U];
        if ((second & 0xC0U) == 0x80U && (third & 0xC0U) == 0x80U) {
            *offset_in_out = pos + 3U;
            return (((uint32_t)(first & 0x0FU)) << 12U) |
                   (((uint32_t)(second & 0x3FU)) << 6U) |
                   (uint32_t)(third & 0x3FU);
        }
    }
    if ((first & 0xF8U) == 0xF0U &&
        text[pos + 1U] != '\0' && text[pos + 2U] != '\0' && text[pos + 3U] != '\0') {
        *offset_in_out = pos + 4U;
        return (uint32_t)'?';
    }
    *offset_in_out = pos + 1U;
    return (uint32_t)'?';
}

static const uint8_t *lookup_glyph_8x8(uint32_t codepoint) {
    if (codepoint >= 32U && codepoint <= 127U) {
        return bitmap_font_ascii[codepoint - 32U];
    }
    if (codepoint >= 0x0410U && codepoint <= 0x044FU) {
        return bitmap_font_cyrillic_block[codepoint - 0x0410U];
    }
    if (codepoint == 0x0404U) {
        return bitmap_font_ukrainian_extra[0];
    }
    if (codepoint == 0x0454U) {
        return bitmap_font_ukrainian_extra[1];
    }
    if (codepoint == 0x0406U) {
        return bitmap_font_ukrainian_extra[2];
    }
    if (codepoint == 0x0456U) {
        return bitmap_font_ukrainian_extra[3];
    }
    if (codepoint == 0x0407U) {
        return bitmap_font_ukrainian_extra[4];
    }
    if (codepoint == 0x0457U) {
        return bitmap_font_ukrainian_extra[5];
    }
    if (codepoint == 0x0490U) {
        return bitmap_font_ukrainian_extra[6];
    }
    if (codepoint == 0x0491U) {
        return bitmap_font_ukrainian_extra[7];
    }
    if (codepoint == 0x2019U || codepoint == 0x2018U) {
        return bitmap_font_ascii[(size_t)('\'' - ' ')];
    }
    if (codepoint == 0x2013U || codepoint == 0x2014U) {
        return bitmap_font_ascii[(size_t)('-' - ' ')];
    }
    return bitmap_font_ascii[(size_t)('?' - ' ')];
}

static double effective_ui_scale(const EditorState *state) {
    if (state == nullptr || state->ui_scale < 1.0) {
        return 1.0;
    }
    if (state->ui_scale > 3.0) {
        return 3.0;
    }
    return state->ui_scale;
}

static int32_t scale_ui_px(double ui_scale, int32_t base_px) {
    if (ui_scale <= 1.01) {
        return base_px;
    }
    return (int32_t)llround((double)base_px * ui_scale);
}

static int32_t clamp_ui_font_pixel_size(int32_t pixel_size) {
    if (pixel_size < UI_FONT_MIN_PIXEL_SIZE) {
        return UI_FONT_MIN_PIXEL_SIZE;
    }
    if (pixel_size > UI_FONT_MAX_PIXEL_SIZE) {
        return UI_FONT_MAX_PIXEL_SIZE;
    }
    return pixel_size;
}

static int32_t ft_fixed_26_6_to_px(FT_Pos fixed_value) {
    if (fixed_value >= 0L) {
        return (int32_t)((fixed_value + 32L) / 64L);
    }
    return -(int32_t)(((-fixed_value) + 32L) / 64L);
}

static int16_t clamp_to_int16(int32_t value) {
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    return (int16_t)value;
}

static uint32_t ui_font_slot_to_codepoint(size_t slot_index) {
    if (slot_index < UI_FONT_ASCII_COUNT) {
        return 0x0020U + (uint32_t)slot_index;
    }
    const size_t cyrillic_offset = slot_index - UI_FONT_ASCII_COUNT;
    if (cyrillic_offset < UI_FONT_CYRILLIC_COUNT) {
        return 0x0400U + (uint32_t)cyrillic_offset;
    }
    const size_t extra_offset = cyrillic_offset - UI_FONT_CYRILLIC_COUNT;
    if (extra_offset < UI_FONT_EXTRA_COUNT) {
        return ui_font_extra_codepoints[extra_offset];
    }
    return (uint32_t)'?';
}

static size_t ui_font_codepoint_to_slot(uint32_t codepoint) {
    if (codepoint >= 0x0020U && codepoint <= 0x007EU) {
        return (size_t)(codepoint - 0x0020U);
    }
    if (codepoint >= 0x0400U && codepoint <= 0x045FU) {
        return UI_FONT_ASCII_COUNT + (size_t)(codepoint - 0x0400U);
    }
    const size_t extra_base = UI_FONT_ASCII_COUNT + UI_FONT_CYRILLIC_COUNT;
    for (size_t extra_index = 0U; extra_index < UI_FONT_EXTRA_COUNT; extra_index += 1U) {
        if (ui_font_extra_codepoints[extra_index] == codepoint) {
            return extra_base + extra_index;
        }
    }
    return (size_t)('?' - ' ');
}

static void init_ui_font_fallback_metrics(UiFont *font, int32_t pixel_size) {
    const int32_t clamped_size = clamp_ui_font_pixel_size(pixel_size);
    int32_t fallback_scale = (int32_t)llround((double)clamped_size / 8.0);
    if (fallback_scale < 2) {
        fallback_scale = 2;
    }
    *font = (UiFont){};
    font->pixel_size = clamped_size;
    font->fallback_scale = fallback_scale;
    font->ascender = 7 * fallback_scale;
    font->descender = fallback_scale;
    font->cap_height = 7 * fallback_scale;
    font->line_height = 8 * fallback_scale;
    font->using_freetype = false;
}

static bool copy_ft_bitmap_to_atlas(
    const FT_Bitmap *bitmap,
    uint8_t *destination,
    size_t destination_capacity,
    size_t offset
) {
    const size_t width = (size_t)bitmap->width;
    const size_t height = (size_t)bitmap->rows;
    size_t glyph_bytes = 0U;
    size_t end_offset = 0U;
    if (ckd_mul(&glyph_bytes, width, height) ||
        ckd_add(&end_offset, offset, glyph_bytes) ||
        end_offset > destination_capacity) {
        return false;
    }
    const int32_t pitch = bitmap->pitch;
    for (size_t row_index = 0U; row_index < height; row_index += 1U) {
        const uint8_t *source_row = nullptr;
        if (pitch >= 0) {
            source_row = bitmap->buffer + row_index * (size_t)pitch;
        } else {
            const size_t abs_pitch = (size_t)(-pitch);
            source_row = bitmap->buffer + (height - 1U - row_index) * abs_pitch;
        }
        memcpy(destination + offset + row_index * width, source_row, width);
    }
    return true;
}

static bool bake_freetype_face_into_font(
    UiFont *font,
    FT_Library library,
    const char *font_path,
    int32_t pixel_size,
    bool require_cyrillic
) {
    bool succeeded = false;
    FT_Face face = nullptr;
    uint8_t *alpha_pixels = nullptr;
    const int32_t clamped_size = clamp_ui_font_pixel_size(pixel_size);
    /* Equivalent to FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT without signed-shift macros. */
    const FT_Int32 load_flags =
        (FT_Int32)((1U << 2U) | (((uint32_t)FT_RENDER_MODE_LIGHT & 0xFU) << 16U));

    if (font_path == nullptr || font_path[0] == '\0') {
        goto cleanup;
    }
    if (FT_New_Face(library, font_path, 0L, &face) != 0 || face == nullptr) {
        goto cleanup;
    }
    if (FT_Get_Char_Index(face, (FT_ULong)'A') == 0U) {
        goto cleanup;
    }
    if (require_cyrillic &&
        (FT_Get_Char_Index(face, 0x0407UL) == 0U || FT_Get_Char_Index(face, 0x0491UL) == 0U)) {
        goto cleanup;
    }
    if (FT_Set_Pixel_Sizes(face, 0U, (FT_UInt)clamped_size) != 0) {
        goto cleanup;
    }

    /* Pass 1: measure exact flat alpha atlas size for all 203 pre-baked glyphs. */
    size_t total_alpha_bytes = 0U;
    for (size_t slot_index = 0U; slot_index < UI_FONT_GLYPH_COUNT; slot_index += 1U) {
        const uint32_t codepoint = ui_font_slot_to_codepoint(slot_index);
        if (FT_Load_Char(face, (FT_ULong)codepoint, load_flags) != 0) {
            continue;
        }
        const FT_Bitmap *bitmap = &face->glyph->bitmap;
        if (bitmap->pixel_mode != FT_PIXEL_MODE_GRAY ||
            bitmap->buffer == nullptr ||
            bitmap->width == 0U ||
            bitmap->rows == 0U) {
            continue;
        }
        if (bitmap->width > UINT16_MAX || bitmap->rows > UINT16_MAX) {
            goto cleanup;
        }
        size_t glyph_bytes = 0U;
        if (ckd_mul(&glyph_bytes, (size_t)bitmap->width, (size_t)bitmap->rows) ||
            ckd_add(&total_alpha_bytes, total_alpha_bytes, glyph_bytes)) {
            goto cleanup;
        }
    }
    if (total_alpha_bytes == 0U || total_alpha_bytes > (size_t)UINT32_MAX) {
        goto cleanup;
    }

    alpha_pixels = (uint8_t *)calloc(total_alpha_bytes, 1U);
    if (alpha_pixels == nullptr) {
        goto cleanup;
    }

    /* Pass 2: rasterize and pack every glyph into the flat cache-friendly atlas. */
    UiFont baked = {};
    init_ui_font_fallback_metrics(&baked, clamped_size);
    size_t write_offset = 0U;

    for (size_t slot_index = 0U; slot_index < UI_FONT_GLYPH_COUNT; slot_index += 1U) {
        const uint32_t codepoint = ui_font_slot_to_codepoint(slot_index);
        if (FT_Load_Char(face, (FT_ULong)codepoint, load_flags) != 0) {
            continue;
        }
        const FT_GlyphSlot slot = face->glyph;
        const FT_Bitmap *bitmap = &slot->bitmap;
        UiGlyphMetric metric = {};
        metric.advance_x = clamp_to_int16(ft_fixed_26_6_to_px(slot->advance.x));
        metric.bitmap_left = clamp_to_int16((int32_t)slot->bitmap_left);
        metric.bitmap_top = clamp_to_int16((int32_t)slot->bitmap_top);
        metric.valid = 1U;

        if (bitmap->pixel_mode == FT_PIXEL_MODE_GRAY &&
            bitmap->buffer != nullptr &&
            bitmap->width > 0U &&
            bitmap->rows > 0U) {
            if (!copy_ft_bitmap_to_atlas(bitmap, alpha_pixels, total_alpha_bytes, write_offset)) {
                goto cleanup;
            }
            metric.atlas_offset = (uint32_t)write_offset;
            metric.width = (uint16_t)bitmap->width;
            metric.height = (uint16_t)bitmap->rows;
            write_offset += (size_t)bitmap->width * (size_t)bitmap->rows;
        }
        baked.glyphs[slot_index] = metric;
    }

    int32_t ascender = ft_fixed_26_6_to_px(face->size->metrics.ascender);
    int32_t descender = ft_fixed_26_6_to_px(-face->size->metrics.descender);
    int32_t line_height = ft_fixed_26_6_to_px(face->size->metrics.height);
    if (ascender <= 0) {
        ascender = clamped_size;
    }
    if (descender < 0) {
        descender = 0;
    }
    if (line_height < ascender + descender) {
        line_height = ascender + descender;
    }

    int32_t cap_height = ascender;
    const size_t cap_slot = ui_font_codepoint_to_slot((uint32_t)'H');
    if (cap_slot < UI_FONT_GLYPH_COUNT &&
        baked.glyphs[cap_slot].valid != 0U &&
        baked.glyphs[cap_slot].bitmap_top > 0) {
        cap_height = (int32_t)baked.glyphs[cap_slot].bitmap_top;
    }

    baked.alpha_pixels = alpha_pixels;
    baked.alpha_byte_count = total_alpha_bytes;
    baked.ascender = ascender;
    baked.descender = descender;
    baked.cap_height = cap_height;
    baked.line_height = line_height;
    baked.using_freetype = true;

    free_ui_font(font);
    *font = baked;
    alpha_pixels = nullptr;
    succeeded = true;

cleanup:
    free(alpha_pixels);
    if (face != nullptr) {
        (void)FT_Done_Face(face);
    }
    return succeeded;
}

bool init_ui_font_with_path(UiFont *font, const char *font_path, int32_t pixel_size) {
    bool loaded = false;
    FT_Library library = nullptr;

    init_ui_font_fallback_metrics(font, pixel_size);
    if (font_path == nullptr || font_path[0] == '\0') {
        goto cleanup;
    }
    if (FT_Init_FreeType(&library) != 0 || library == nullptr) {
        goto cleanup;
    }
    loaded = bake_freetype_face_into_font(font, library, font_path, pixel_size, false);

cleanup:
    if (library != nullptr) {
        (void)FT_Done_FreeType(library);
    }
    return loaded;
}

bool init_ui_font(UiFont *font, int32_t pixel_size) {
    bool loaded = false;
    FT_Library library = nullptr;

    init_ui_font_fallback_metrics(font, pixel_size);
    if (FT_Init_FreeType(&library) != 0 || library == nullptr) {
        goto cleanup;
    }

    const char *env_font_path = getenv("EMBROIDERY_FONT");
    if (env_font_path != nullptr && env_font_path[0] != '\0') {
        loaded = bake_freetype_face_into_font(font, library, env_font_path, pixel_size, false);
        if (loaded) {
            goto cleanup;
        }
    }

    constexpr size_t path_count = sizeof(system_ui_font_paths) / sizeof(system_ui_font_paths[0]);
    for (size_t pass_index = 0U; pass_index < 2U; pass_index += 1U) {
        const bool require_cyrillic = (pass_index == 0U);
        for (size_t path_index = 0U; path_index < path_count; path_index += 1U) {
            if (bake_freetype_face_into_font(
                    font,
                    library,
                    system_ui_font_paths[path_index],
                    pixel_size,
                    require_cyrillic
                )) {
                loaded = true;
                goto cleanup;
            }
        }
    }

cleanup:
    if (library != nullptr) {
        (void)FT_Done_FreeType(library);
    }
    return loaded;
}

void free_ui_font(UiFont *font) {
    if (font == nullptr) {
        return;
    }
    free(font->alpha_pixels);
    *font = (UiFont){};
}

bool point_is_in_box(
    int32_t point_x,
    int32_t point_y,
    int32_t box_x,
    int32_t box_y,
    int32_t box_width,
    int32_t box_height
) {
    return point_x >= box_x && point_x < box_x + box_width &&
           point_y >= box_y && point_y < box_y + box_height;
}

bool rect_contains_point(UiRect rect, int32_t point_x, int32_t point_y) {
    return point_is_in_box(point_x, point_y, rect.x, rect.y, rect.width, rect.height);
}

static int32_t fallback_glyph_step(const UiFont *font) {
    int32_t scale = 2;
    if (font != nullptr && font->fallback_scale >= 2) {
        scale = font->fallback_scale;
    }
    return 7 * scale;
}

int32_t measure_ui_text_width(const UiFont *font, const char *text) {
    if (text == nullptr || text[0] == '\0') {
        return 0;
    }
    if (font == nullptr || !font->using_freetype || font->alpha_pixels == nullptr) {
        int32_t glyph_count = 0;
        size_t offset = 0U;
        while (text[offset] != '\0') {
            const uint32_t codepoint = decode_utf8_codepoint(text, &offset);
            if (codepoint == 0U) {
                break;
            }
            glyph_count += 1;
        }
        return glyph_count * fallback_glyph_step(font);
    }

    int32_t pen_x = 0;
    int32_t max_right = 0;
    size_t offset = 0U;
    while (text[offset] != '\0') {
        const uint32_t codepoint = decode_utf8_codepoint(text, &offset);
        if (codepoint == 0U) {
            break;
        }
        const size_t slot_index = ui_font_codepoint_to_slot(codepoint);
        const UiGlyphMetric metric = font->glyphs[slot_index];
        if (metric.valid == 0U) {
            continue;
        }
        const int32_t glyph_right = pen_x + (int32_t)metric.bitmap_left + (int32_t)metric.width;
        if (glyph_right > max_right) {
            max_right = glyph_right;
        }
        pen_x += (int32_t)metric.advance_x;
        if (pen_x > max_right) {
            max_right = pen_x;
        }
    }
    return max_right;
}

static void blit_glyph_alpha(
    PixelCanvas *canvas,
    const uint8_t *alpha_bitmap,
    int32_t glyph_x,
    int32_t glyph_y,
    int32_t glyph_width,
    int32_t glyph_height,
    uint32_t packed_color
) {
    int32_t start_x = glyph_x;
    int32_t start_y = glyph_y;
    int32_t end_x = glyph_x + glyph_width;
    int32_t end_y = glyph_y + glyph_height;

    if (start_x < canvas->clip_min_x) {
        start_x = canvas->clip_min_x;
    }
    if (start_y < canvas->clip_min_y) {
        start_y = canvas->clip_min_y;
    }
    if (end_x > canvas->clip_max_x) {
        end_x = canvas->clip_max_x;
    }
    if (end_y > canvas->clip_max_y) {
        end_y = canvas->clip_max_y;
    }
    if (start_x >= end_x || start_y >= end_y) {
        return;
    }

    const uint32_t fg_red = (packed_color >> 16U) & 0xFFU;
    const uint32_t fg_green = (packed_color >> 8U) & 0xFFU;
    const uint32_t fg_blue = packed_color & 0xFFU;

    for (int32_t screen_y = start_y; screen_y < end_y; screen_y += 1) {
        const int32_t src_row_index = screen_y - glyph_y;
        const uint8_t *src_row = alpha_bitmap + (size_t)src_row_index * (size_t)glyph_width;
        uint32_t *dst_row = canvas->pixels + (size_t)screen_y * (size_t)canvas->width;

        for (int32_t screen_x = start_x; screen_x < end_x; screen_x += 1) {
            const uint8_t alpha = src_row[screen_x - glyph_x];
            if (alpha == 0U) {
                continue;
            }
            if (alpha == 255U) {
                dst_row[screen_x] = packed_color;
                continue;
            }
            const uint32_t dst_pixel = dst_row[screen_x];
            const uint32_t alpha_u32 = (uint32_t)alpha;
            const uint32_t inv_alpha = 255U - alpha_u32;
            const uint32_t bg_red = (dst_pixel >> 16U) & 0xFFU;
            const uint32_t bg_green = (dst_pixel >> 8U) & 0xFFU;
            const uint32_t bg_blue = dst_pixel & 0xFFU;
            const uint32_t out_red = (fg_red * alpha_u32 + bg_red * inv_alpha + 127U) / 255U;
            const uint32_t out_green = (fg_green * alpha_u32 + bg_green * inv_alpha + 127U) / 255U;
            const uint32_t out_blue = (fg_blue * alpha_u32 + bg_blue * inv_alpha + 127U) / 255U;
            dst_row[screen_x] = (0xFFU << 24U) | (out_red << 16U) | (out_green << 8U) | out_blue;
        }
    }
}

static void draw_fallback_text_scaled(
    PixelCanvas *canvas,
    const UiFont *font,
    int32_t origin_x,
    int32_t origin_y,
    const char *text,
    uint32_t packed_color
) {
    int32_t scale = 2;
    if (font != nullptr && font->fallback_scale >= 2) {
        scale = font->fallback_scale;
    }
    const int32_t step_x = 7 * scale;
    int32_t cursor_x = origin_x;
    size_t offset = 0U;

    while (text[offset] != '\0') {
        const uint32_t codepoint = decode_utf8_codepoint(text, &offset);
        if (codepoint == 0U) {
            break;
        }
        const uint8_t *glyph_rows = lookup_glyph_8x8(codepoint);
        for (int32_t row = 0; row < 8; row += 1) {
            const uint8_t row_bits = glyph_rows[row];
            for (int32_t column = 0; column < 8; column += 1) {
                if ((row_bits & (1U << (uint32_t)column)) != 0U) {
                    draw_filled_rectangle(
                        canvas,
                        cursor_x + column * scale,
                        origin_y + row * scale,
                        scale,
                        scale,
                        packed_color
                    );
                }
            }
        }
        cursor_x += step_x;
    }
}

static void draw_ui_text_at_baseline(
    PixelCanvas *canvas,
    const UiFont *font,
    int32_t origin_x,
    int32_t baseline_y,
    const char *text,
    uint32_t packed_color
) {
    if (canvas == nullptr || canvas->pixels == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }
    if (font == nullptr || !font->using_freetype || font->alpha_pixels == nullptr) {
        int32_t ascender = 14;
        if (font != nullptr && font->ascender > 0) {
            ascender = font->ascender;
        }
        draw_fallback_text_scaled(canvas, font, origin_x, baseline_y - ascender, text, packed_color);
        return;
    }

    int32_t pen_x = origin_x;
    size_t offset = 0U;
    while (text[offset] != '\0') {
        const uint32_t codepoint = decode_utf8_codepoint(text, &offset);
        if (codepoint == 0U) {
            break;
        }
        const size_t slot_index = ui_font_codepoint_to_slot(codepoint);
        const UiGlyphMetric metric = font->glyphs[slot_index];
        if (metric.valid == 0U) {
            continue;
        }
        if (metric.width > 0U && metric.height > 0U) {
            const size_t glyph_end =
                (size_t)metric.atlas_offset + (size_t)metric.width * (size_t)metric.height;
            if (glyph_end <= font->alpha_byte_count) {
                blit_glyph_alpha(
                    canvas,
                    font->alpha_pixels + metric.atlas_offset,
                    pen_x + (int32_t)metric.bitmap_left,
                    baseline_y - (int32_t)metric.bitmap_top,
                    (int32_t)metric.width,
                    (int32_t)metric.height,
                    packed_color
                );
            }
        }
        pen_x += (int32_t)metric.advance_x;
    }
}

void draw_ui_text(
    PixelCanvas *canvas,
    const UiFont *font,
    int32_t origin_x,
    int32_t origin_y,
    const char *text,
    uint32_t packed_color
) {
    int32_t ascender = 14;
    if (font != nullptr && font->ascender > 0) {
        ascender = font->ascender;
    }
    draw_ui_text_at_baseline(canvas, font, origin_x, origin_y + ascender, text, packed_color);
}

void draw_ui_text_vcentered(
    PixelCanvas *canvas,
    const UiFont *font,
    int32_t origin_x,
    int32_t box_top,
    int32_t box_height,
    const char *text,
    uint32_t packed_color
) {
    int32_t cap_height = 12;
    if (font != nullptr && font->cap_height > 0) {
        cap_height = font->cap_height;
    }
    const int32_t baseline_y = box_top + (box_height + cap_height) / 2;
    draw_ui_text_at_baseline(canvas, font, origin_x, baseline_y, text, packed_color);
}

void draw_ui_text_centered(
    PixelCanvas *canvas,
    const UiFont *font,
    UiRect box,
    const char *text,
    uint32_t packed_color
) {
    if (canvas == nullptr || box.width <= 0 || box.height <= 0 || text == nullptr || text[0] == '\0') {
        return;
    }
    const int32_t saved_min_x = canvas->clip_min_x;
    const int32_t saved_min_y = canvas->clip_min_y;
    const int32_t saved_max_x = canvas->clip_max_x;
    const int32_t saved_max_y = canvas->clip_max_y;

    if (box.x > canvas->clip_min_x) {
        canvas->clip_min_x = box.x;
    }
    if (box.y > canvas->clip_min_y) {
        canvas->clip_min_y = box.y;
    }
    if (box.x + box.width < canvas->clip_max_x) {
        canvas->clip_max_x = box.x + box.width;
    }
    if (box.y + box.height < canvas->clip_max_y) {
        canvas->clip_max_y = box.y + box.height;
    }

    const int32_t text_width = measure_ui_text_width(font, text);
    int32_t origin_x = box.x + (box.width - text_width) / 2;
    if (origin_x < box.x) {
        origin_x = box.x;
    }
    draw_ui_text_vcentered(canvas, font, origin_x, box.y, box.height, text, packed_color);

    canvas->clip_min_x = saved_min_x;
    canvas->clip_min_y = saved_min_y;
    canvas->clip_max_x = saved_max_x;
    canvas->clip_max_y = saved_max_y;
}

void draw_ui_button(
    PixelCanvas *canvas,
    const UiFont *font,
    const UiButton *button,
    int32_t mouse_x,
    int32_t mouse_y
) {
    const bool is_hovered =
        !button->is_readout &&
        point_is_in_box(mouse_x, mouse_y, button->left_x, button->top_y, button->width, button->height);

    uint32_t background = pack_rgb_pixel(52U, 56U, 64U);
    uint32_t border = pack_rgb_pixel(88U, 94U, 108U);
    uint32_t text_color = pack_rgb_pixel(236U, 240U, 246U);

    if (button->is_readout) {
        background = pack_rgb_pixel(28U, 31U, 37U);
        border = pack_rgb_pixel(66U, 72U, 84U);
        text_color = pack_rgb_pixel(190U, 224U, 255U);
    } else if (button->is_danger) {
        background = pack_rgb_pixel(184U, 42U, 42U);
        border = pack_rgb_pixel(252U, 148U, 148U);
        text_color = pack_rgb_pixel(255U, 255U, 255U);
    } else if (button->is_active) {
        background = pack_rgb_pixel(38U, 108U, 196U);
        border = pack_rgb_pixel(124U, 190U, 255U);
        text_color = pack_rgb_pixel(255U, 255U, 255U);
    } else if (button->is_accent) {
        background = pack_rgb_pixel(38U, 126U, 72U);
        border = pack_rgb_pixel(96U, 204U, 134U);
        text_color = pack_rgb_pixel(255U, 255U, 255U);
    }
    if (is_hovered) {
        if (button->is_danger) {
            background = pack_rgb_pixel(212U, 54U, 54U);
        } else if (button->is_active) {
            background = pack_rgb_pixel(52U, 126U, 218U);
        } else if (button->is_accent) {
            background = pack_rgb_pixel(48U, 148U, 86U);
        } else {
            background = pack_rgb_pixel(72U, 78U, 90U);
        }
        border = pack_rgb_pixel(210U, 224U, 242U);
    }

    draw_filled_rectangle(canvas, button->left_x, button->top_y, button->width, button->height, background);
    draw_rectangle_outline(canvas, button->left_x, button->top_y, button->width, button->height, border, 0);

    if (button->has_swatch) {
        int32_t swatch_pad = button->height / 6;
        if (swatch_pad < 4) {
            swatch_pad = 4;
        }
        const int32_t swatch_size = button->height - 2 * swatch_pad;
        const int32_t swatch_x = button->left_x + swatch_pad + 1;
        const int32_t swatch_y = button->top_y + swatch_pad;
        draw_filled_rectangle(
            canvas,
            swatch_x,
            swatch_y,
            swatch_size,
            swatch_size,
            pack_rgb_pixel(
                button->swatch_color.red,
                button->swatch_color.green,
                button->swatch_color.blue
            )
        );
        draw_rectangle_outline(
            canvas,
            swatch_x,
            swatch_y,
            swatch_size,
            swatch_size,
            pack_rgb_pixel(230U, 230U, 230U),
            0
        );
        const int32_t text_left = swatch_x + swatch_size + swatch_pad + 1;
        const int32_t text_width = (button->left_x + button->width - 3) - text_left;
        if (text_width > 0) {
            const UiRect text_rect = {text_left, button->top_y + 1, text_width, button->height - 2};
            draw_ui_text_centered(canvas, font, text_rect, button->label, text_color);
        }
        return;
    }

    const UiRect label_rect = {button->left_x + 2, button->top_y + 1, button->width - 4, button->height - 2};
    draw_ui_text_centered(canvas, font, label_rect, button->label, text_color);
}

void set_status_message(EditorState *state, const char *message) {
    const int written = snprintf(state->status_message, sizeof(state->status_message), "%s", message);
    if (written < 0) {
        state->status_message[0] = '\0';
    }
    state->needs_redraw = true;
}

void editor_mark_design_changed(EditorState *state, bool mark_dirty) {
    state->design.stats_valid = false;
    state->design.geometry_revision += 1U;
    state->design_revision += 1U;
    spatial_index_invalidate(&state->spatial_index);
    cross_cluster_cache_invalidate(&state->cross_cache);
    invalidate_stitch_layer_cache(&state->stitch_cache);
    if (mark_dirty) {
        state->is_dirty = true;
    }
    state->needs_redraw = true;
}

void editor_ensure_spatial_index(EditorState *state) {
    int32_t pitch = state->grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }
    if (state->spatial_index.valid &&
        state->spatial_index.built_revision == state->design.geometry_revision &&
        state->spatial_index.component_pitch_units == pitch) {
        return;
    }
    (void)spatial_index_build(&state->spatial_index, &state->design, pitch, &state->scratch);
}

void editor_update_canvas_hover(
    EditorState *state,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t window_width,
    int32_t window_height
) {
    const RenderViewport viewport = get_editor_viewport(state, window_width, window_height);
    if (!point_is_in_box(mouse_x, mouse_y, viewport.left, viewport.top, viewport.width, viewport.height) ||
        state->design.chain_count == 0U ||
        state->has_floating_fragment) {
        if (state->hover_seed_chain != SIZE_MAX ||
            state->hover_component_id != SIZE_MAX ||
            state->has_hover_cross_box ||
            state->has_draw_cursor_cell) {
            clear_hover_preview(state);
            state->has_draw_cursor_cell = false;
            state->needs_redraw = true;
        }
        return;
    }

    const int32_t world_x = viewport_screen_to_world_x(viewport, state->camera, mouse_x);
    const int32_t world_y = viewport_screen_to_world_y(viewport, state->camera, mouse_y);

    if (state->active_tool == TOOL_DRAW_CROSS) {
        int32_t cell_col = 0;
        int32_t cell_row = 0;
        world_to_grid_cell(world_x, world_y, state->grid, &cell_col, &cell_row);
        if (!state->has_draw_cursor_cell ||
            cell_col != state->last_draw_cursor_column ||
            cell_row != state->last_draw_cursor_row) {
            state->has_draw_cursor_cell = true;
            state->last_draw_cursor_column = cell_col;
            state->last_draw_cursor_row = cell_row;
            state->needs_redraw = true;
        }
        return;
    }

    if (state->active_tool == TOOL_SELECT_CROSS) {
        editor_ensure_spatial_index(state);
        bool snapped = false;
        const CutRectangle box = resolve_cross_box_at_point_indexed(
            &state->design,
            &state->spatial_index,
            state->grid,
            world_x,
            world_y,
            &snapped
        );
        if (!state->has_hover_cross_box ||
            box.minimum_x != state->hover_cross_box.minimum_x ||
            box.minimum_y != state->hover_cross_box.minimum_y) {
            state->hover_cross_box = box;
            state->has_hover_cross_box = true;
            state->needs_redraw = true;
        }
        return;
    }

    if (state->active_tool == TOOL_SELECT_ELEMENT) {
        editor_ensure_spatial_index(state);
        int32_t search_radius = state->grid.cross_pitch_units;
        const int32_t pixel_radius = (int32_t)llround(12.0 / state->camera.pixels_per_unit);
        if (pixel_radius > search_radius) {
            search_radius = pixel_radius;
        }
        size_t hovered_chain = SIZE_MAX;
        if (!find_nearest_chain_at_point_indexed(
                &state->design,
                &state->spatial_index,
                world_x,
                world_y,
                search_radius,
                &hovered_chain
            )) {
            if (state->hover_seed_chain != SIZE_MAX || state->hover_component_id != SIZE_MAX) {
                clear_hover_preview(state);
                state->needs_redraw = true;
            }
            return;
        }

        size_t next_component = SIZE_MAX;
        if (state->spatial_index.valid &&
            state->spatial_index.chain_component_id != nullptr &&
            hovered_chain < state->spatial_index.chain_count) {
            next_component = state->spatial_index.chain_component_id[hovered_chain];
        }
        if (hovered_chain == state->hover_seed_chain && next_component == state->hover_component_id) {
            return;
        }
        state->hover_seed_chain = hovered_chain;
        state->hover_component_id = next_component;
        state->needs_redraw = true;
    }
}

void clear_segment_selection(EditorState *state) {
    free(state->selection_mask);
    state->selection_mask = nullptr;
    state->selection_mask_length = 0U;
    state->selected_segment_count = 0U;
    state->mask_revision += 1U;
}

void clear_hover_preview(EditorState *state) {
    free(state->hover_mask);
    state->hover_mask = nullptr;
    state->hover_mask_length = 0U;
    state->hover_seed_chain = SIZE_MAX;
    state->hover_component_id = SIZE_MAX;
    state->has_hover_cross_box = false;
}

void clear_floating_fragment(EditorState *state) {
    free_stitch_design(&state->floating_fragment);
    state->has_floating_fragment = false;
    state->floating_is_grid_origin_aligned = false;
    state->floating_offset_x = 0;
    state->floating_offset_y = 0;
}

void clear_live_preview(EditorState *state) {
    free(state->edit_preview_keep_mask);
    state->edit_preview_keep_mask = nullptr;
    state->edit_preview_mask_length = 0U;
    state->has_edit_preview_mask = false;
    state->preview_dirty = false;
    state->mask_revision += 1U;
}

bool ensure_selection_mask_allocated(EditorState *state) {
    const size_t total_segments = count_design_segments(&state->design);
    if (total_segments == 0U) {
        clear_segment_selection(state);
        return false;
    }
    if (state->selection_mask != nullptr && state->selection_mask_length == total_segments) {
        return true;
    }
    clear_segment_selection(state);
    state->selection_mask = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
    if (state->selection_mask == nullptr) {
        return false;
    }
    state->selection_mask_length = total_segments;
    return true;
}

void recount_selected_segments(EditorState *state) {
    size_t count = 0U;
    for (size_t segment_index = 0U; segment_index < state->selection_mask_length; segment_index += 1U) {
        if (state->selection_mask[segment_index] != 0U) {
            count += 1U;
        }
    }
    state->selected_segment_count = count;
    state->mask_revision += 1U;
}

void strip_history_push(EditorState *state) {
    if (state->strip_history.state_count == MAX_STRIP_HISTORY_STATES) {
        for (size_t shift_index = 1U; shift_index < MAX_STRIP_HISTORY_STATES; shift_index += 1U) {
            state->strip_history.states[shift_index - 1U] = state->strip_history.states[shift_index];
        }
        state->strip_history.state_count = MAX_STRIP_HISTORY_STATES - 1U;
    }
    state->strip_history.states[state->strip_history.state_count] = state->strip_pattern;
    state->strip_history.state_count += 1U;
}

const char *basename_of_path(const char *path) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash == nullptr) {
        return path;
    }
    return last_slash + 1;
}

static void add_button_to_list(
    UiButton *buttons,
    size_t *button_count,
    size_t capacity,
    int32_t left_x,
    int32_t top_y,
    int32_t width,
    int32_t height,
    uint8_t command_id,
    const char *label,
    bool is_active,
    bool is_accent,
    bool is_danger,
    bool is_readout,
    bool has_swatch,
    RgbColor swatch_color
) {
    if (*button_count >= capacity) {
        return;
    }
    UiButton *button = &buttons[*button_count];
    *button = (UiButton){};
    button->left_x = left_x;
    button->top_y = top_y;
    button->width = width;
    button->height = height;
    button->command_id = command_id;
    button->is_active = is_active;
    button->is_accent = is_accent;
    button->is_danger = is_danger;
    button->is_readout = is_readout;
    button->has_swatch = has_swatch;
    button->swatch_color = swatch_color;
    (void)snprintf(button->label, sizeof(button->label), "%.31s", label);
    *button_count += 1U;
}

static void append_top_bar_button(
    const EditorState *state,
    EditorLayout *layout,
    int32_t *cursor_x,
    int32_t right_limit,
    int32_t button_y,
    int32_t button_height,
    int32_t horizontal_padding,
    int32_t button_gap,
    uint8_t command_id,
    const char *label,
    bool is_active,
    bool is_accent
) {
    const int32_t button_width = measure_ui_text_width(&state->ui_font, label) + horizontal_padding;
    if (*cursor_x + button_width > right_limit) {
        return;
    }
    const RgbColor no_swatch = {};
    add_button_to_list(
        layout->top_buttons,
        &layout->top_button_count,
        MAX_TOP_BUTTONS,
        *cursor_x,
        button_y,
        button_width,
        button_height,
        command_id,
        label,
        is_active,
        is_accent,
        false,
        false,
        false,
        no_swatch
    );
    *cursor_x += button_width + button_gap;
}

int32_t get_editor_color_row_height(const EditorState *state) {
    return scale_ui_px(effective_ui_scale(state), COLOR_ROW_HEIGHT);
}

/* Single source of truth for all panel/button/slider/viewport rectangles (zero SDL). */
EditorLayout compute_editor_layout(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
) {
    EditorLayout layout = {};
    const RgbColor no_swatch = {};
    const double ui_scale = effective_ui_scale(state);

    int32_t safe_width = window_width;
    if (safe_width < 320) {
        safe_width = 320;
    }
    int32_t safe_height = window_height;
    if (safe_height < 240) {
        safe_height = 240;
    }

    const int32_t top_bar_h = scale_ui_px(ui_scale, TOP_BAR_HEIGHT);
    const int32_t left_rail_w = scale_ui_px(ui_scale, LEFT_RAIL_WIDTH);
    const int32_t right_panel_base_w = scale_ui_px(ui_scale, RIGHT_PANEL_WIDTH);
    const int32_t bottom_bar_h = scale_ui_px(ui_scale, BOTTOM_BAR_HEIGHT);
    const int32_t min_viewport_w = scale_ui_px(ui_scale, 200);

    layout.top_bar = (UiRect){0, 0, safe_width, top_bar_h};
    layout.bottom_bar = (UiRect){0, safe_height - bottom_bar_h, safe_width, bottom_bar_h};

    const int32_t middle_top = top_bar_h;
    int32_t middle_height = safe_height - top_bar_h - bottom_bar_h;
    if (middle_height < 64) {
        middle_height = 64;
    }

    layout.left_rail = (UiRect){0, middle_top, left_rail_w, middle_height};
    if (state->right_panel_collapsed) {
        layout.right_panel = (UiRect){safe_width, middle_top, 0, middle_height};
    } else {
        int32_t right_width = right_panel_base_w;
        if (safe_width - left_rail_w - right_width < min_viewport_w) {
            right_width = 0;
        }
        layout.right_panel = (UiRect){safe_width - right_width, middle_top, right_width, middle_height};
    }

    int32_t viewport_width = safe_width - layout.left_rail.width - layout.right_panel.width;
    if (viewport_width < 64) {
        viewport_width = 64;
    }
    layout.viewport = (UiRect){layout.left_rail.width, middle_top, viewport_width, middle_height};

    /* Top bar: single scaled row with compact labels below 1200 logical px width. */
    const int32_t logical_width = (int32_t)llround((double)safe_width / ui_scale);
    const bool compact_top = (logical_width < 1200);
    const char *toggle_panel_label = ">>";
    if (state->right_panel_collapsed) {
        toggle_panel_label = "<<";
    } else if (!compact_top) {
        toggle_panel_label = "Panel";
    }
    int32_t top_btn_pad = scale_ui_px(ui_scale, 10);
    if (state->ui_font.using_freetype) {
        top_btn_pad = scale_ui_px(ui_scale, 16);
    }
    const int32_t top_btn_h = scale_ui_px(ui_scale, 28);
    const int32_t top_btn_y = (top_bar_h - top_btn_h) / 2;
    const int32_t top_btn_gap = scale_ui_px(ui_scale, 4);
    const int32_t edge_pad = scale_ui_px(ui_scale, 6);

    const int32_t panel_btn_width = measure_ui_text_width(&state->ui_font, toggle_panel_label) + top_btn_pad;
    const int32_t panel_btn_x = safe_width - panel_btn_width - edge_pad;
    const int32_t left_limit = panel_btn_x - scale_ui_px(ui_scale, 8);

    int32_t cursor_x = edge_pad;
    append_top_bar_button(state, &layout, &cursor_x, left_limit, top_btn_y, top_btn_h, top_btn_pad, top_btn_gap, COMMAND_OPEN_FILE_DIALOG, "Open", false, false);
    if (compact_top) {
        append_top_bar_button(state, &layout, &cursor_x, left_limit, top_btn_y, top_btn_h, top_btn_pad, top_btn_gap, COMMAND_MERGE_FILE_DIALOG, "Mrg", false, false);
    } else {
        append_top_bar_button(state, &layout, &cursor_x, left_limit, top_btn_y, top_btn_h, top_btn_pad, top_btn_gap, COMMAND_MERGE_FILE_DIALOG, "Merge", false, false);
    }
    append_top_bar_button(state, &layout, &cursor_x, left_limit, top_btn_y, top_btn_h, top_btn_pad, top_btn_gap, COMMAND_SAVE, "Save", false, true);
    if (compact_top) {
        append_top_bar_button(state, &layout, &cursor_x, left_limit, top_btn_y, top_btn_h, top_btn_pad, top_btn_gap, COMMAND_SAVE_AS_DIALOG, "As", false, false);
        append_top_bar_button(state, &layout, &cursor_x, left_limit, top_btn_y, top_btn_h, top_btn_pad, top_btn_gap, COMMAND_OPEN_STRIP_MODAL, "Strp", state->strip_modal_open, false);
    } else {
        append_top_bar_button(state, &layout, &cursor_x, left_limit, top_btn_y, top_btn_h, top_btn_pad, top_btn_gap, COMMAND_SAVE_AS_DIALOG, "SaveAs", false, false);
        append_top_bar_button(state, &layout, &cursor_x, left_limit, top_btn_y, top_btn_h, top_btn_pad, top_btn_gap, COMMAND_OPEN_STRIP_MODAL, "Strip", state->strip_modal_open, false);
    }
    append_top_bar_button(state, &layout, &cursor_x, left_limit, top_btn_y, top_btn_h, top_btn_pad, top_btn_gap, COMMAND_UNDO, "Undo", false, false);
    append_top_bar_button(state, &layout, &cursor_x, left_limit, top_btn_y, top_btn_h, top_btn_pad, top_btn_gap, COMMAND_REDO, "Redo", false, false);

    const char *view_label = "View:Thread";
    if (compact_top) {
        if (state->view_mode == VIEW_MODE_BLOCKS) {
            view_label = "Blck";
        } else if (state->view_mode == VIEW_MODE_LINE) {
            view_label = "Line";
        } else {
            view_label = "Thrd";
        }
    } else if (state->view_mode == VIEW_MODE_BLOCKS) {
        view_label = "View:Blocks";
    } else if (state->view_mode == VIEW_MODE_LINE) {
        view_label = "View:Line";
    }
    append_top_bar_button(state, &layout, &cursor_x, left_limit, top_btn_y, top_btn_h, top_btn_pad, top_btn_gap, COMMAND_CYCLE_VIEW_MODE, view_label, false, false);
    append_top_bar_button(state, &layout, &cursor_x, left_limit, top_btn_y, top_btn_h, top_btn_pad, top_btn_gap, COMMAND_FIT_VIEW, "Fit", false, false);

    const char *drop_label = "Drop:Open";
    if (compact_top) {
        if (state->drop_merge_mode) {
            drop_label = "DrpM";
        } else {
            drop_label = "DrpO";
        }
    } else if (state->drop_merge_mode) {
        drop_label = "Drop:Merge";
    }
    append_top_bar_button(state, &layout, &cursor_x, left_limit, top_btn_y, top_btn_h, top_btn_pad, top_btn_gap, COMMAND_TOGGLE_DROP_MODE, drop_label, state->drop_merge_mode, false);

    add_button_to_list(
        layout.top_buttons,
        &layout.top_button_count,
        MAX_TOP_BUTTONS,
        panel_btn_x,
        top_btn_y,
        panel_btn_width,
        top_btn_h,
        COMMAND_TOGGLE_RIGHT_PANEL,
        toggle_panel_label,
        !state->right_panel_collapsed,
        false,
        false,
        false,
        false,
        no_swatch
    );

    /* Left tool rail: 9 tools + 4 transforms + 2 selection actions. */
    const int32_t rail_btn_x = scale_ui_px(ui_scale, 4);
    const int32_t rail_btn_w = left_rail_w - 2 * rail_btn_x;
    const int32_t rail_btn_h = scale_ui_px(ui_scale, 32);
    const int32_t rail_gap = scale_ui_px(ui_scale, 4);
    const int32_t rail_group_gap = scale_ui_px(ui_scale, 8);
    int32_t rail_y = middle_top + scale_ui_px(ui_scale, 6);
    const int32_t rail_bottom = middle_top + middle_height - scale_ui_px(ui_scale, 4);

    const struct {
        const char *label;
        uint8_t cmd;
        bool active;
        bool danger;
    } rail_items[15] = {
        {"Elem", COMMAND_TOOL_SELECT_ELEMENT, state->active_tool == TOOL_SELECT_ELEMENT, false},
        {"SelX", COMMAND_TOOL_SELECT_CROSS, state->active_tool == TOOL_SELECT_CROSS, false},
        {"Draw", COMMAND_TOOL_DRAW_CROSS, state->active_tool == TOOL_DRAW_CROSS, false},
        {"Crop", COMMAND_TOOL_CROP, state->active_tool == TOOL_CROP_KEEP, false},
        {"Cut", COMMAND_TOOL_CUT, state->active_tool == TOOL_CUT_OUT, false},
        {"SplV", COMMAND_TOOL_SPLIT_V, state->active_tool == TOOL_SPLIT_VERTICAL, false},
        {"SplH", COMMAND_TOOL_SPLIT_H, state->active_tool == TOOL_SPLIT_HORIZONTAL, false},
        {"Clon", COMMAND_TOOL_CLONE, state->active_tool == TOOL_CLONE, false},
        {"Pick", COMMAND_TOOL_PICK, state->active_tool == TOOL_PICK_COLOR, false},
        {"Rot", COMMAND_ROTATE_90, false, false},
        {"FlpX", COMMAND_FLIP_X, false, false},
        {"FlpY", COMMAND_FLIP_Y, false, false},
        {"Ctr", COMMAND_CENTER, false, false},
        {"Rcol", COMMAND_RECOLOR_SELECTION, false, false},
        {"Del", COMMAND_DELETE_SELECTION, false, state->selected_segment_count > 0U}
    };

    for (size_t item_index = 0U; item_index < 15U; item_index += 1U) {
        if (item_index == 9U || item_index == 13U) {
            rail_y += rail_group_gap;
        }
        if (rail_y + rail_btn_h > rail_bottom) {
            break;
        }
        add_button_to_list(
            layout.rail_buttons,
            &layout.rail_button_count,
            MAX_RAIL_BUTTONS,
            rail_btn_x,
            rail_y,
            rail_btn_w,
            rail_btn_h,
            rail_items[item_index].cmd,
            rail_items[item_index].label,
            rail_items[item_index].active,
            false,
            rail_items[item_index].danger,
            false,
            false,
            no_swatch
        );
        rail_y += rail_btn_h + rail_gap;
    }

    /* Right panel: compact Props rows + scrollable Colors list below. */
    if (layout.right_panel.width > 0) {
        const int32_t prop_pad = scale_ui_px(ui_scale, 8);
        const int32_t prop_gap = scale_ui_px(ui_scale, 4);
        const int32_t prop_x = layout.right_panel.x + prop_pad;
        const int32_t inner_w = layout.right_panel.width - 2 * prop_pad;
        int32_t prop_y = middle_top + prop_pad;
        const int32_t row_h = scale_ui_px(ui_scale, 28);
        const int32_t row_step = scale_ui_px(ui_scale, 33);
        char label_buf[32] = {};

        /* Row 1: Grid pitch (- / value / +) and passes (1x..6x). */
        const int32_t r1_w1 = scale_ui_px(ui_scale, 42);
        const int32_t r1_w2 = scale_ui_px(ui_scale, 88);
        const int32_t r1_w3 = scale_ui_px(ui_scale, 42);
        const int32_t r1_w4 = inner_w - (r1_w1 + r1_w2 + r1_w3 + 3 * prop_gap);
        add_button_to_list(layout.prop_buttons, &layout.prop_button_count, MAX_PROP_BUTTONS, prop_x, prop_y, r1_w1, row_h, COMMAND_PITCH_DECREASE, "P-", false, false, false, false, false, no_swatch);
        (void)snprintf(label_buf, sizeof(label_buf), "%.1fmm", (double)state->grid.cross_pitch_units * 0.1);
        add_button_to_list(layout.prop_buttons, &layout.prop_button_count, MAX_PROP_BUTTONS, prop_x + r1_w1 + prop_gap, prop_y, r1_w2, row_h, COMMAND_NONE, label_buf, false, false, false, true, false, no_swatch);
        add_button_to_list(layout.prop_buttons, &layout.prop_button_count, MAX_PROP_BUTTONS, prop_x + r1_w1 + r1_w2 + 2 * prop_gap, prop_y, r1_w3, row_h, COMMAND_PITCH_INCREASE, "P+", false, false, false, false, false, no_swatch);
        (void)snprintf(label_buf, sizeof(label_buf), "%dx", state->grid.cross_passes);
        add_button_to_list(layout.prop_buttons, &layout.prop_button_count, MAX_PROP_BUTTONS, prop_x + r1_w1 + r1_w2 + r1_w3 + 3 * prop_gap, prop_y, r1_w4, row_h, COMMAND_CYCLE_PASSES, label_buf, false, false, false, false, false, no_swatch);
        prop_y += row_step;

        /* Row 2: Thread thickness, Hoop preset, Rotate hoop. */
        size_t safe_thk = state->thread_thickness_index;
        if (safe_thk >= THREAD_THICKNESS_PRESET_COUNT) {
            safe_thk = 1U;
        }
        const int32_t r2_w1 = scale_ui_px(ui_scale, 106);
        const int32_t r2_w2 = scale_ui_px(ui_scale, 106);
        const int32_t r2_w3 = inner_w - (r2_w1 + r2_w2 + 2 * prop_gap);
        (void)snprintf(label_buf, sizeof(label_buf), "Thk.%.0f", thread_thickness_units[safe_thk] * 10.0);
        add_button_to_list(layout.prop_buttons, &layout.prop_button_count, MAX_PROP_BUTTONS, prop_x, prop_y, r2_w1, row_h, COMMAND_CYCLE_THREAD_THICKNESS, label_buf, false, false, false, false, false, no_swatch);
        size_t safe_hoop = state->hoop_preset_index;
        if (safe_hoop >= HOOP_PRESET_COUNT) {
            safe_hoop = 0U;
        }
        add_button_to_list(layout.prop_buttons, &layout.prop_button_count, MAX_PROP_BUTTONS, prop_x + r2_w1 + prop_gap, prop_y, r2_w2, row_h, COMMAND_CYCLE_HOOP, standard_hoop_presets[safe_hoop].label, safe_hoop != 0U, false, false, false, false, no_swatch);
        add_button_to_list(layout.prop_buttons, &layout.prop_button_count, MAX_PROP_BUTTONS, prop_x + r2_w1 + r2_w2 + 2 * prop_gap, prop_y, r2_w3, row_h, COMMAND_ROTATE_HOOP, "R", state->hoop_rotated, false, false, false, false, no_swatch);
        prop_y += row_step;

        /* Row 3: Grid / Jumps / Locks toggles. */
        const int32_t r3_w1 = (inner_w - 2 * prop_gap) / 3;
        const int32_t r3_w2 = r3_w1;
        const int32_t r3_w3 = inner_w - (r3_w1 + r3_w2 + 2 * prop_gap);
        add_button_to_list(layout.prop_buttons, &layout.prop_button_count, MAX_PROP_BUTTONS, prop_x, prop_y, r3_w1, row_h, COMMAND_TOGGLE_GRID, "Grid", state->show_grid, false, false, false, false, no_swatch);
        add_button_to_list(layout.prop_buttons, &layout.prop_button_count, MAX_PROP_BUTTONS, prop_x + r3_w1 + prop_gap, prop_y, r3_w2, row_h, COMMAND_TOGGLE_JUMPS, "Jump", state->show_jumps, false, false, false, false, no_swatch);
        add_button_to_list(layout.prop_buttons, &layout.prop_button_count, MAX_PROP_BUTTONS, prop_x + r3_w1 + r3_w2 + 2 * prop_gap, prop_y, r3_w3, row_h, COMMAND_TOGGLE_LOCKS, "Lock", state->show_locks, false, false, false, false, no_swatch);
        prop_y += row_step;

        /* Row 4: + Color and Merge Colors. */
        const int32_t r4_w1 = (inner_w - prop_gap) / 2;
        const int32_t r4_w2 = inner_w - prop_gap - r4_w1;
        add_button_to_list(layout.prop_buttons, &layout.prop_button_count, MAX_PROP_BUTTONS, prop_x, prop_y, r4_w1, row_h, COMMAND_ADD_COLOR_BLOCK, "+Color", false, true, false, false, false, no_swatch);
        add_button_to_list(layout.prop_buttons, &layout.prop_button_count, MAX_PROP_BUTTONS, prop_x + r4_w1 + prop_gap, prop_y, r4_w2, row_h, COMMAND_MERGE_COLORS, "MrgCol", false, false, false, false, false, no_swatch);
        prop_y += row_step + scale_ui_px(ui_scale, 4);

        const int32_t list_pad = scale_ui_px(ui_scale, 6);
        int32_t list_height = (middle_top + middle_height) - prop_y - list_pad;
        if (list_height < 0) {
            list_height = 0;
        }
        layout.colors_list_rect = (UiRect){
            layout.right_panel.x + list_pad,
            prop_y,
            layout.right_panel.width - 2 * list_pad,
            list_height
        };
    }

    /* Bottom bar slider track rect (deterministic, never written during render). */
    const int32_t slider_pad = scale_ui_px(ui_scale, 16);
    int32_t slider_left = measure_ui_text_width(&state->ui_font, "Sim:100%") + scale_ui_px(ui_scale, 20);
    const int32_t min_slider_left = scale_ui_px(ui_scale, 96);
    if (slider_left < min_slider_left) {
        slider_left = min_slider_left;
    }
    int32_t metrics_reserve_base = 380;
    if (state->ui_font.using_freetype) {
        metrics_reserve_base = 270;
        if (logical_width < 1100) {
            metrics_reserve_base = 240;
        }
    } else if (logical_width < 1100) {
        metrics_reserve_base = 300;
    }
    const int32_t metrics_reserve = scale_ui_px(ui_scale, metrics_reserve_base);
    int32_t slider_width = safe_width - slider_left - metrics_reserve - slider_pad;
    const int32_t min_slider_w = scale_ui_px(ui_scale, 80);
    if (slider_width < min_slider_w) {
        slider_width = min_slider_w;
    }
    const int32_t bottom_half = bottom_bar_h / 2;
    const int32_t track_h = scale_ui_px(ui_scale, 18);
    layout.slider_track = (UiRect){
        slider_left,
        layout.bottom_bar.y + (bottom_half - track_h) / 2,
        slider_width,
        track_h
    };

    return layout;
}

RenderViewport get_editor_viewport(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
) {
    const EditorLayout layout = compute_editor_layout(state, window_width, window_height);
    return (RenderViewport){
        .left = layout.viewport.x,
        .top = layout.viewport.y,
        .width = layout.viewport.width,
        .height = layout.viewport.height
    };
}

int32_t editor_world_to_screen_x(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t world_x
) {
    const RenderViewport viewport = get_editor_viewport(state, window_width, window_height);
    return viewport_world_to_screen_x(viewport, state->camera, world_x);
}

int32_t editor_world_to_screen_y(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t world_y
) {
    const RenderViewport viewport = get_editor_viewport(state, window_width, window_height);
    return viewport_world_to_screen_y(viewport, state->camera, world_y);
}

int32_t editor_screen_to_world_x(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t screen_x
) {
    const RenderViewport viewport = get_editor_viewport(state, window_width, window_height);
    return viewport_screen_to_world_x(viewport, state->camera, screen_x);
}

int32_t editor_screen_to_world_y(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t screen_y
) {
    const RenderViewport viewport = get_editor_viewport(state, window_width, window_height);
    return viewport_screen_to_world_y(viewport, state->camera, screen_y);
}

int32_t snap_coordinate_to_grid(int32_t value, int32_t origin, int32_t pitch) {
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        return value;
    }
    const int32_t cell_index = floor_divide_int32(value - origin + pitch / 2, pitch);
    return origin + cell_index * pitch;
}

void fit_camera_to_design(EditorState *state, int32_t window_width, int32_t window_height) {
    const RenderViewport viewport = get_editor_viewport(state, window_width, window_height);

    if (!state->design.has_bounds) {
        state->camera.center_x = 0.0;
        state->camera.center_y = 0.0;
        state->camera.pixels_per_unit = 0.5;
        state->needs_redraw = true;
        return;
    }

    double design_width = (double)state->design.maximum_x - (double)state->design.minimum_x;
    double design_height = (double)state->design.maximum_y - (double)state->design.minimum_y;
    if (design_width < 20.0) {
        design_width = 20.0;
    }
    if (design_height < 20.0) {
        design_height = 20.0;
    }

    const double scale_x = ((double)viewport.width * 0.86) / design_width;
    const double scale_y = ((double)viewport.height * 0.86) / design_height;
    double best_scale = scale_x;
    if (scale_y < best_scale) {
        best_scale = scale_y;
    }
    if (best_scale < 0.05) {
        best_scale = 0.05;
    }
    if (best_scale > 12.0) {
        best_scale = 12.0;
    }

    state->camera.center_x =
        ((double)state->design.minimum_x + (double)state->design.maximum_x) * 0.5;
    state->camera.center_y =
        ((double)state->design.minimum_y + (double)state->design.maximum_y) * 0.5;
    state->camera.pixels_per_unit = best_scale;
    state->needs_redraw = true;
}

bool tool_uses_box_preview(EditorTool tool) {
    return tool == TOOL_CROP_KEEP ||
           tool == TOOL_CUT_OUT ||
           tool == TOOL_SPLIT_VERTICAL ||
           tool == TOOL_SPLIT_HORIZONTAL;
}

EditOperationKind tool_to_edit_operation(EditorTool tool) {
    if (tool == TOOL_CUT_OUT) {
        return EDIT_CUT_OUT;
    }
    if (tool == TOOL_SPLIT_VERTICAL) {
        return EDIT_SPLIT_VERTICAL;
    }
    if (tool == TOOL_SPLIT_HORIZONTAL) {
        return EDIT_SPLIT_HORIZONTAL;
    }
    return EDIT_CROP_KEEP;
}

void refresh_live_preview(EditorState *state) {
    if (!state->preview_dirty) {
        return;
    }
    state->preview_dirty = false;

    if (!state->has_box_selection || !tool_uses_box_preview(state->active_tool)) {
        state->has_edit_preview_mask = false;
        return;
    }

    const size_t total_segments = count_design_segments(&state->design);
    if (total_segments == 0U) {
        state->has_edit_preview_mask = false;
        return;
    }

    if (state->edit_preview_keep_mask == nullptr || state->edit_preview_mask_length != total_segments) {
        free(state->edit_preview_keep_mask);
        state->edit_preview_keep_mask = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
        state->edit_preview_mask_length = total_segments;
    }
    if (state->edit_preview_keep_mask == nullptr) {
        state->edit_preview_mask_length = 0U;
        state->has_edit_preview_mask = false;
        return;
    }

    const CutRectangle rectangle = {
        state->box_start_x,
        state->box_start_y,
        state->box_end_x,
        state->box_end_y
    };
    const EditOperationKind operation = tool_to_edit_operation(state->active_tool);
    state->has_edit_preview_mask = build_edit_segment_mask_cached(
        &state->cross_cache,
        &state->design,
        state->grid,
        operation,
        rectangle,
        state->split_coordinate,
        false,
        state->edit_preview_keep_mask,
        state->edit_preview_mask_length,
        &state->scratch
    );
    state->mask_revision += 1U;
}

bool should_show_start_screen(const EditorState *state) {
    return state->design.chain_count == 0U &&
           state->design.block_count == 0U &&
           !state->has_floating_fragment &&
           !state->strip_modal_open &&
           state->palette_target == PALETTE_TARGET_NONE;
}

void update_slider_from_layout(EditorState *state, const EditorLayout *layout, int32_t mouse_x) {
    if (layout->slider_track.width <= 0) {
        return;
    }
    double ratio =
        (double)(mouse_x - layout->slider_track.x) / (double)layout->slider_track.width;
    if (ratio < 0.0) {
        ratio = 0.0;
    }
    if (ratio > 1.0) {
        ratio = 1.0;
    }
    state->simulator_percent = ratio * 100.0;
    state->needs_redraw = true;
}

StartScreenLayout compute_start_screen_layout(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
) {
    const RenderViewport viewport = get_editor_viewport(state, window_width, window_height);
    const double ui_scale = effective_ui_scale(state);

    StartScreenLayout layout = {};
    layout.card_width = scale_ui_px(ui_scale, 580);
    const int32_t margin = scale_ui_px(ui_scale, 24);
    const int32_t min_card_w = scale_ui_px(ui_scale, 280);
    if (layout.card_width > viewport.width - margin) {
        layout.card_width = viewport.width - margin;
    }
    if (layout.card_width < min_card_w) {
        layout.card_width = min_card_w;
    }
    layout.card_height = scale_ui_px(ui_scale, 240);
    layout.card_left = viewport.left + (viewport.width - layout.card_width) / 2;
    layout.card_top = viewport.top + (viewport.height - layout.card_height) / 2;

    int32_t button_width = scale_ui_px(ui_scale, 200);
    const int32_t button_height = scale_ui_px(ui_scale, 48);
    const int32_t button_gap = scale_ui_px(ui_scale, 20);
    if (button_width * 2 + button_gap > layout.card_width - margin) {
        button_width = (layout.card_width - margin - button_gap) / 2;
    }
    const int32_t pair_left = layout.card_left + (layout.card_width - (button_width * 2 + button_gap)) / 2;
    const int32_t button_top = layout.card_top + scale_ui_px(ui_scale, 96);

    layout.open_button.left_x = pair_left;
    layout.open_button.top_y = button_top;
    layout.open_button.width = button_width;
    layout.open_button.height = button_height;
    layout.open_button.is_active = true;
    (void)snprintf(layout.open_button.label, sizeof(layout.open_button.label), "Open File");

    layout.strip_button.left_x = pair_left + button_width + button_gap;
    layout.strip_button.top_y = button_top;
    layout.strip_button.width = button_width;
    layout.strip_button.height = button_height;
    layout.strip_button.is_accent = true;
    (void)snprintf(layout.strip_button.label, sizeof(layout.strip_button.label), "New Strip");

    return layout;
}

PaletteModalLayout compute_palette_modal_layout(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
) {
    const double ui_scale = effective_ui_scale(state);
    PaletteModalLayout layout = {};
    layout.columns = 10;
    layout.rows = 8;
    layout.cell_size = scale_ui_px(ui_scale, 56);
    layout.cell_gap = scale_ui_px(ui_scale, 6);
    const int32_t logical_w = (int32_t)llround((double)window_width / ui_scale);
    const int32_t logical_h = (int32_t)llround((double)window_height / ui_scale);
    if (logical_w < 720 || logical_h < 640) {
        layout.cell_size = scale_ui_px(ui_scale, 46);
        layout.cell_gap = scale_ui_px(ui_scale, 4);
    }

    const int32_t grid_width = layout.columns * layout.cell_size + (layout.columns - 1) * layout.cell_gap;
    const int32_t grid_height = layout.rows * layout.cell_size + (layout.rows - 1) * layout.cell_gap;
    layout.modal_width = grid_width + scale_ui_px(ui_scale, 40);
    layout.modal_height = grid_height + scale_ui_px(ui_scale, 112);
    layout.modal_left = (window_width - layout.modal_width) / 2;
    layout.modal_top = (window_height - layout.modal_height) / 2;
    const int32_t min_margin = scale_ui_px(ui_scale, 8);
    if (layout.modal_left < min_margin) {
        layout.modal_left = min_margin;
    }
    if (layout.modal_top < min_margin) {
        layout.modal_top = min_margin;
    }
    layout.grid_left = layout.modal_left + scale_ui_px(ui_scale, 20);
    layout.grid_top = layout.modal_top + scale_ui_px(ui_scale, 46);
    return layout;
}

StripModalLayout compute_strip_modal_layout(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
) {
    const double ui_scale = effective_ui_scale(state);
    StripModalLayout layout = {};

    const int32_t margin_x = scale_ui_px(ui_scale, 32);
    const int32_t margin_y = scale_ui_px(ui_scale, 40);
    const int32_t max_w = scale_ui_px(ui_scale, 1280);
    const int32_t max_h = scale_ui_px(ui_scale, 820);

    layout.modal_width = window_width - margin_x;
    if (layout.modal_width > max_w) {
        layout.modal_width = max_w;
    }
    layout.modal_height = window_height - margin_y;
    if (layout.modal_height > max_h) {
        layout.modal_height = max_h;
    }
    layout.modal_left = (window_width - layout.modal_width) / 2;
    layout.modal_top = (window_height - layout.modal_height) / 2;

    const RgbColor no_swatch = {};
    const int32_t pad_12 = scale_ui_px(ui_scale, 12);
    const int32_t btn_h = scale_ui_px(ui_scale, 28);
    const int32_t row_one_y = layout.modal_top + scale_ui_px(ui_scale, 40);
    int32_t cursor_x = layout.modal_left + pad_12;
    char value_text[32] = {};

    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, scale_ui_px(ui_scale, 56), btn_h, STRIP_CMD_WIDTH_DEC, "W-", false, false, false, false, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 60);
    (void)snprintf(value_text, sizeof(value_text), "%d", state->strip_pattern.width);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, scale_ui_px(ui_scale, 44), btn_h, STRIP_CMD_NONE, value_text, false, false, false, true, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 48);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, scale_ui_px(ui_scale, 30), btn_h, STRIP_CMD_WIDTH_INC, "+", false, false, false, false, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 40);

    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, scale_ui_px(ui_scale, 56), btn_h, STRIP_CMD_HEIGHT_DEC, "H-", false, false, false, false, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 60);
    (void)snprintf(value_text, sizeof(value_text), "%d", state->strip_pattern.height);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, scale_ui_px(ui_scale, 44), btn_h, STRIP_CMD_NONE, value_text, false, false, false, true, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 48);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, scale_ui_px(ui_scale, 30), btn_h, STRIP_CMD_HEIGHT_INC, "+", false, false, false, false, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 40);

    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, scale_ui_px(ui_scale, 82), btn_h, STRIP_CMD_REPEAT_X_DEC, "RepX-", false, false, false, false, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 86);
    (void)snprintf(value_text, sizeof(value_text), "%d", state->strip_pattern.repeat_x);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, scale_ui_px(ui_scale, 44), btn_h, STRIP_CMD_NONE, value_text, false, false, false, true, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 48);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, scale_ui_px(ui_scale, 30), btn_h, STRIP_CMD_REPEAT_X_INC, "+", false, false, false, false, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 40);

    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, scale_ui_px(ui_scale, 82), btn_h, STRIP_CMD_REPEAT_Y_DEC, "RepY-", false, false, false, false, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 86);
    (void)snprintf(value_text, sizeof(value_text), "%d", state->strip_pattern.repeat_y);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, scale_ui_px(ui_scale, 44), btn_h, STRIP_CMD_NONE, value_text, false, false, false, true, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 48);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, scale_ui_px(ui_scale, 30), btn_h, STRIP_CMD_REPEAT_Y_INC, "+", false, false, false, false, false, no_swatch);

    const int32_t row_two_y = row_one_y + scale_ui_px(ui_scale, 34);
    cursor_x = layout.modal_left + pad_12;

    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, scale_ui_px(ui_scale, 76), btn_h, STRIP_CMD_PITCH_DEC, "Sz-", false, false, false, false, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 80);
    (void)snprintf(value_text, sizeof(value_text), "%.1fmm", (double)state->strip_pattern.pitch_units * 0.1);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, scale_ui_px(ui_scale, 84), btn_h, STRIP_CMD_NONE, value_text, false, false, false, true, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 88);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, scale_ui_px(ui_scale, 30), btn_h, STRIP_CMD_PITCH_INC, "+", false, false, false, false, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 38);

    (void)snprintf(value_text, sizeof(value_text), "Pass:%dx", state->strip_pattern.passes);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, scale_ui_px(ui_scale, 110), btn_h, STRIP_CMD_CYCLE_PASSES, value_text, false, false, false, false, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 116);

    uint8_t active_color = state->strip_active_color;
    if (active_color == 0U || active_color >= JANOME_PALETTE_COUNT) {
        active_color = 10U;
    }
    (void)snprintf(value_text, sizeof(value_text), "Col#%02u", (unsigned int)active_color);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, scale_ui_px(ui_scale, 120), btn_h, STRIP_CMD_PICK_COLOR, value_text, false, false, false, false, true, janome_palette[active_color]);
    cursor_x += scale_ui_px(ui_scale, 126);

    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, scale_ui_px(ui_scale, 82), btn_h, STRIP_CMD_CLEAR, "Clear", false, false, false, false, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 88);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, scale_ui_px(ui_scale, 68), btn_h, STRIP_CMD_MIRROR_X, "MirX", false, false, false, false, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 74);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, scale_ui_px(ui_scale, 68), btn_h, STRIP_CMD_MIRROR_Y, "MirY", false, false, false, false, false, no_swatch);
    cursor_x += scale_ui_px(ui_scale, 74);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, scale_ui_px(ui_scale, 68), btn_h, STRIP_CMD_UNDO, "Undo", false, false, false, false, false, no_swatch);

    const int32_t footer_btn_h = scale_ui_px(ui_scale, 30);
    const int32_t footer_y = layout.modal_top + layout.modal_height - scale_ui_px(ui_scale, 40);
    const int32_t close_x = layout.modal_left + layout.modal_width - scale_ui_px(ui_scale, 96);
    const int32_t save_jef_x = close_x - scale_ui_px(ui_scale, 130);
    const int32_t save_as_x = save_jef_x - scale_ui_px(ui_scale, 114);
    const int32_t insert_x = save_as_x - scale_ui_px(ui_scale, 104);

    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, insert_x, footer_y, scale_ui_px(ui_scale, 98), footer_btn_h, STRIP_CMD_INSERT, "Insert", false, true, false, false, false, no_swatch);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, save_as_x, footer_y, scale_ui_px(ui_scale, 108), footer_btn_h, STRIP_CMD_SAVE_AS, "SaveAs", false, false, false, false, false, no_swatch);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, save_jef_x, footer_y, scale_ui_px(ui_scale, 124), footer_btn_h, STRIP_CMD_SAVE_JEF, "SaveJEF", false, false, false, false, false, no_swatch);
    add_button_to_list(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, close_x, footer_y, scale_ui_px(ui_scale, 84), footer_btn_h, STRIP_CMD_CLOSE, "Close", false, false, false, false, false, no_swatch);

    const int32_t panes_top = row_two_y + scale_ui_px(ui_scale, 38);
    const int32_t panes_height = footer_y - panes_top - pad_12;
    const int32_t inner_width = layout.modal_width - 3 * pad_12;
    layout.editor_left = layout.modal_left + pad_12;
    layout.editor_top = panes_top;
    layout.editor_width = (inner_width * 58) / 100;
    layout.editor_height = panes_height;

    layout.preview_left = layout.editor_left + layout.editor_width + pad_12;
    layout.preview_top = panes_top;
    layout.preview_width = inner_width - layout.editor_width;
    layout.preview_height = panes_height;

    const int32_t pad_16 = scale_ui_px(ui_scale, 16);
    const int32_t cell_width = (layout.editor_width - pad_16) / state->strip_pattern.width;
    const int32_t cell_height = (layout.editor_height - pad_16) / state->strip_pattern.height;
    int32_t cell_size = cell_width;
    if (cell_height < cell_size) {
        cell_size = cell_height;
    }
    const int32_t min_cell = scale_ui_px(ui_scale, 6);
    const int32_t max_cell = scale_ui_px(ui_scale, 54);
    if (cell_size < min_cell) {
        cell_size = min_cell;
    }
    if (cell_size > max_cell) {
        cell_size = max_cell;
    }
    layout.editor_cell_size = cell_size;
    return layout;
}

static uint8_t hit_test_button_array(
    const UiButton *buttons,
    size_t count,
    int32_t mouse_x,
    int32_t mouse_y
) {
    for (size_t button_index = 0U; button_index < count; button_index += 1U) {
        const UiButton *button = &buttons[button_index];
        if (!button->is_readout &&
            point_is_in_box(mouse_x, mouse_y, button->left_x, button->top_y, button->width, button->height)) {
            return button->command_id;
        }
    }
    return COMMAND_NONE;
}

uint8_t hit_test_editor_buttons(const EditorLayout *layout, int32_t mouse_x, int32_t mouse_y) {
    uint8_t cmd = hit_test_button_array(layout->top_buttons, layout->top_button_count, mouse_x, mouse_y);
    if (cmd != COMMAND_NONE) {
        return cmd;
    }
    cmd = hit_test_button_array(layout->rail_buttons, layout->rail_button_count, mouse_x, mouse_y);
    if (cmd != COMMAND_NONE) {
        return cmd;
    }
    return hit_test_button_array(layout->prop_buttons, layout->prop_button_count, mouse_x, mouse_y);
}

bool hit_test_slider_layout(const EditorLayout *layout, int32_t mouse_x, int32_t mouse_y) {
    return point_is_in_box(
        mouse_x,
        mouse_y,
        layout->slider_track.x - 8,
        layout->slider_track.y - 4,
        layout->slider_track.width + 16,
        layout->slider_track.height + 8
    );
}

ColorListHitKind hit_test_color_list(
    const EditorState *state,
    const EditorLayout *layout,
    int32_t mouse_x,
    int32_t mouse_y,
    size_t *block_index_out
) {
    if (block_index_out != nullptr) {
        *block_index_out = SIZE_MAX;
    }
    if (layout->right_panel.width <= 0 || !rect_contains_point(layout->right_panel, mouse_x, mouse_y)) {
        return COLOR_LIST_HIT_NONE;
    }
    if (!rect_contains_point(layout->colors_list_rect, mouse_x, mouse_y)) {
        return COLOR_LIST_HIT_PANEL_EMPTY;
    }

    const double ui_scale = effective_ui_scale(state);
    const int32_t row_height = scale_ui_px(ui_scale, COLOR_ROW_HEIGHT);
    const int32_t btn_size = scale_ui_px(ui_scale, COLOR_ROW_BTN_SIZE);
    const int32_t btn_gap = scale_ui_px(ui_scale, COLOR_ROW_BTN_GAP);

    const int32_t row_index_signed =
        (mouse_y - layout->colors_list_rect.y + state->color_scroll_offset) / row_height;
    if (row_index_signed < 0 || (size_t)row_index_signed >= state->design.block_count) {
        return COLOR_LIST_HIT_PANEL_EMPTY;
    }
    const size_t block_index = (size_t)row_index_signed;
    if (block_index_out != nullptr) {
        *block_index_out = block_index;
    }

    const int32_t row_y =
        layout->colors_list_rect.y + row_index_signed * row_height - state->color_scroll_offset;
    const int32_t buttons_total_w = 5 * btn_size + 4 * btn_gap;
    const int32_t buttons_start_x =
        layout->colors_list_rect.x + layout->colors_list_rect.width - scale_ui_px(ui_scale, 4) - buttons_total_w;
    const int32_t button_y = row_y + (row_height - btn_size) / 2;

    static const ColorListHitKind action_kinds[5] = {
        COLOR_LIST_HIT_TOGGLE_VISIBILITY,
        COLOR_LIST_HIT_MOVE_UP,
        COLOR_LIST_HIT_MOVE_DOWN,
        COLOR_LIST_HIT_RECOLOR_BLOCK,
        COLOR_LIST_HIT_DELETE_BLOCK
    };
    for (int32_t action_index = 0; action_index < 5; action_index += 1) {
        const int32_t button_x = buttons_start_x + action_index * (btn_size + btn_gap);
        if (point_is_in_box(mouse_x, mouse_y, button_x, button_y, btn_size, btn_size)) {
            return action_kinds[action_index];
        }
    }
    return COLOR_LIST_HIT_SELECT_BLOCK;
}

bool hit_test_palette_modal(
    const EditorState *state,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t window_width,
    int32_t window_height,
    int32_t *palette_index_out
) {
    *palette_index_out = -1;
    const PaletteModalLayout layout = compute_palette_modal_layout(state, window_width, window_height);
    if (!point_is_in_box(mouse_x, mouse_y, layout.modal_left, layout.modal_top, layout.modal_width, layout.modal_height)) {
        return false;
    }
    for (int32_t palette_index = 0; palette_index < (int32_t)JANOME_PALETTE_COUNT; palette_index += 1) {
        const int32_t column = palette_index % layout.columns;
        const int32_t row = palette_index / layout.columns;
        const int32_t cell_x = layout.grid_left + column * (layout.cell_size + layout.cell_gap);
        const int32_t cell_y = layout.grid_top + row * (layout.cell_size + layout.cell_gap);
        if (point_is_in_box(mouse_x, mouse_y, cell_x, cell_y, layout.cell_size, layout.cell_size)) {
            *palette_index_out = palette_index;
            return true;
        }
    }
    return true;
}

uint8_t hit_test_strip_modal_button(
    const StripModalLayout *layout,
    int32_t mouse_x,
    int32_t mouse_y
) {
    for (size_t button_index = 0U; button_index < layout->button_count; button_index += 1U) {
        const UiButton *button = &layout->buttons[button_index];
        if (!button->is_readout &&
            point_is_in_box(mouse_x, mouse_y, button->left_x, button->top_y, button->width, button->height)) {
            return button->command_id;
        }
    }
    return STRIP_CMD_NONE;
}

bool hit_test_strip_editor_cell(
    const StripPattern *pattern,
    const StripModalLayout *layout,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t *column_out,
    int32_t *row_out
) {
    const int32_t grid_pixel_width = pattern->width * layout->editor_cell_size;
    const int32_t grid_pixel_height = pattern->height * layout->editor_cell_size;
    const int32_t grid_origin_x = layout->editor_left + (layout->editor_width - grid_pixel_width) / 2;
    const int32_t grid_origin_y = layout->editor_top + (layout->editor_height - grid_pixel_height) / 2;

    if (!point_is_in_box(mouse_x, mouse_y, grid_origin_x, grid_origin_y, grid_pixel_width, grid_pixel_height)) {
        return false;
    }
    const int32_t column = (mouse_x - grid_origin_x) / layout->editor_cell_size;
    const int32_t row = (mouse_y - grid_origin_y) / layout->editor_cell_size;
    if (column < 0 || column >= pattern->width || row < 0 || row >= pattern->height) {
        return false;
    }
    *column_out = column;
    *row_out = row;
    return true;
}

static void render_start_screen(PixelCanvas *canvas, const EditorState *state) {
    if (!should_show_start_screen(state)) {
        return;
    }
    const double ui_scale = effective_ui_scale(state);
    const StartScreenLayout layout = compute_start_screen_layout(state, canvas->width, canvas->height);
    draw_filled_rectangle(
        canvas,
        layout.card_left,
        layout.card_top,
        layout.card_width,
        layout.card_height,
        pack_rgb_pixel(36U, 40U, 48U)
    );
    draw_rectangle_outline(
        canvas,
        layout.card_left,
        layout.card_top,
        layout.card_width,
        layout.card_height,
        pack_rgb_pixel(110U, 148U, 204U),
        0
    );

    const UiRect title_rect = {
        layout.card_left + scale_ui_px(ui_scale, 12),
        layout.card_top + scale_ui_px(ui_scale, 20),
        layout.card_width - scale_ui_px(ui_scale, 24),
        scale_ui_px(ui_scale, 44)
    };
    draw_ui_text_centered(canvas, &state->ui_font, title_rect, "embroidery-studio", pack_rgb_pixel(242U, 246U, 252U));

    draw_ui_button(canvas, &state->ui_font, &layout.open_button, state->mouse_screen_x, state->mouse_screen_y);
    draw_ui_button(canvas, &state->ui_font, &layout.strip_button, state->mouse_screen_x, state->mouse_screen_y);

    const UiRect hint_rect = {
        layout.card_left + scale_ui_px(ui_scale, 12),
        layout.card_top + scale_ui_px(ui_scale, 174),
        layout.card_width - scale_ui_px(ui_scale, 24),
        scale_ui_px(ui_scale, 36)
    };
    draw_ui_text_centered(
        canvas,
        &state->ui_font,
        hint_rect,
        "Drop .jef .pes .pec .dst or Ctrl+O",
        pack_rgb_pixel(174U, 188U, 206U)
    );
}

static void render_top_bar(PixelCanvas *canvas, const EditorState *state, const EditorLayout *layout) {
    const double ui_scale = effective_ui_scale(state);
    draw_filled_rectangle(
        canvas,
        layout->top_bar.x,
        layout->top_bar.y,
        layout->top_bar.width,
        layout->top_bar.height,
        pack_rgb_pixel(28U, 31U, 38U)
    );
    draw_line_clipped(
        canvas,
        0,
        layout->top_bar.height - 1,
        layout->top_bar.width,
        layout->top_bar.height - 1,
        pack_rgb_pixel(70U, 76U, 88U),
        0
    );
    for (size_t button_index = 0U; button_index < layout->top_button_count; button_index += 1U) {
        draw_ui_button(canvas, &state->ui_font, &layout->top_buttons[button_index], state->mouse_screen_x, state->mouse_screen_y);
    }

    /* Render current filename (+ '*' dirty flag) in free space between left group and right button. */
    if (layout->top_button_count >= 2U) {
        const UiButton *last_left = &layout->top_buttons[layout->top_button_count - 2U];
        const UiButton *panel_btn = &layout->top_buttons[layout->top_button_count - 1U];
        const int32_t title_left = last_left->left_x + last_left->width + scale_ui_px(ui_scale, 12);
        const int32_t title_right = panel_btn->left_x - scale_ui_px(ui_scale, 10);
        if (title_right - title_left >= scale_ui_px(ui_scale, 64)) {
            char file_badge[160] = {};
            const char *base_name = "untitled.jef";
            if (state->has_file_path) {
                base_name = basename_of_path(state->current_file_path);
            }
            if (state->is_dirty) {
                (void)snprintf(file_badge, sizeof(file_badge), "*%.140s", base_name);
            } else {
                (void)snprintf(file_badge, sizeof(file_badge), "%.140s", base_name);
            }
            canvas->clip_min_x = title_left;
            canvas->clip_max_x = title_right;
            uint32_t badge_color = pack_rgb_pixel(176U, 192U, 212U);
            if (state->is_dirty) {
                badge_color = pack_rgb_pixel(255U, 214U, 118U);
            }
            draw_ui_text_vcentered(
                canvas,
                &state->ui_font,
                title_left,
                layout->top_bar.y,
                layout->top_bar.height,
                file_badge,
                badge_color
            );
            reset_canvas_clip(canvas);
        }
    }
}

static void render_left_tool_rail(PixelCanvas *canvas, const EditorState *state, const EditorLayout *layout) {
    draw_filled_rectangle(
        canvas,
        layout->left_rail.x,
        layout->left_rail.y,
        layout->left_rail.width,
        layout->left_rail.height,
        pack_rgb_pixel(32U, 35U, 42U)
    );
    draw_line_clipped(
        canvas,
        layout->left_rail.width - 1,
        layout->left_rail.y,
        layout->left_rail.width - 1,
        layout->left_rail.y + layout->left_rail.height,
        pack_rgb_pixel(70U, 76U, 88U),
        0
    );
    for (size_t button_index = 0U; button_index < layout->rail_button_count; button_index += 1U) {
        draw_ui_button(canvas, &state->ui_font, &layout->rail_buttons[button_index], state->mouse_screen_x, state->mouse_screen_y);
    }
}

static void render_right_panel(PixelCanvas *canvas, const EditorState *state, const EditorLayout *layout) {
    if (layout->right_panel.width <= 0) {
        return;
    }
    const double ui_scale = effective_ui_scale(state);
    const UiRect panel = layout->right_panel;
    draw_filled_rectangle(canvas, panel.x, panel.y, panel.width, panel.height, pack_rgb_pixel(34U, 37U, 44U));
    draw_line_clipped(canvas, panel.x, panel.y, panel.x, panel.y + panel.height, pack_rgb_pixel(70U, 76U, 88U), 0);

    for (size_t button_index = 0U; button_index < layout->prop_button_count; button_index += 1U) {
        draw_ui_button(canvas, &state->ui_font, &layout->prop_buttons[button_index], state->mouse_screen_x, state->mouse_screen_y);
    }

    const UiRect list_rect = layout->colors_list_rect;
    draw_line_clipped(
        canvas,
        panel.x + scale_ui_px(ui_scale, 6),
        list_rect.y - scale_ui_px(ui_scale, 4),
        panel.x + panel.width - scale_ui_px(ui_scale, 6),
        list_rect.y - scale_ui_px(ui_scale, 4),
        pack_rgb_pixel(64U, 70U, 82U),
        0
    );

    canvas->clip_min_x = list_rect.x;
    canvas->clip_min_y = list_rect.y;
    canvas->clip_max_x = list_rect.x + list_rect.width;
    canvas->clip_max_y = list_rect.y + list_rect.height;

    const int32_t row_height = scale_ui_px(ui_scale, COLOR_ROW_HEIGHT);
    const int32_t btn_size = scale_ui_px(ui_scale, COLOR_ROW_BTN_SIZE);
    const int32_t btn_gap = scale_ui_px(ui_scale, COLOR_ROW_BTN_GAP);
    const int32_t swatch_size = scale_ui_px(ui_scale, 20);
    const int32_t pad_4 = scale_ui_px(ui_scale, 4);
    const int32_t pad_6 = scale_ui_px(ui_scale, 6);

    if (state->design.block_count == 0U) {
        draw_ui_text_vcentered(
            canvas,
            &state->ui_font,
            list_rect.x + scale_ui_px(ui_scale, 12),
            list_rect.y,
            row_height,
            "No colors",
            pack_rgb_pixel(146U, 154U, 168U)
        );
        reset_canvas_clip(canvas);
        return;
    }

    static const char *const row_symbols[5] = {"V", "^", "v", "C", "X"};
    const int32_t buttons_total_w = 5 * btn_size + 4 * btn_gap;
    const int32_t buttons_start_x = list_rect.x + list_rect.width - pad_4 - buttons_total_w;

    for (size_t block_index = 0U; block_index < state->design.block_count; block_index += 1U) {
        const int32_t row_y =
            list_rect.y + (int32_t)block_index * row_height - state->color_scroll_offset;
        if (row_y + row_height < list_rect.y || row_y >= list_rect.y + list_rect.height) {
            continue;
        }

        const ColorBlock block = state->design.blocks[block_index];
        const bool is_active = (block_index == state->active_color_block_index);
        uint32_t row_background = pack_rgb_pixel(42U, 46U, 54U);
        if (is_active) {
            row_background = pack_rgb_pixel(54U, 82U, 118U);
        }
        draw_filled_rectangle(canvas, list_rect.x + 2, row_y + 2, list_rect.width - 4, row_height - 4, row_background);
        if (is_active) {
            draw_rectangle_outline(canvas, list_rect.x + 2, row_y + 2, list_rect.width - 4, row_height - 4, pack_rgb_pixel(128U, 190U, 255U), 0);
        }

        const int32_t swatch_x = list_rect.x + pad_6;
        const int32_t swatch_y = row_y + (row_height - swatch_size) / 2;
        draw_filled_rectangle(
            canvas,
            swatch_x,
            swatch_y,
            swatch_size,
            swatch_size,
            pack_rgb_pixel(block.display_color.red, block.display_color.green, block.display_color.blue)
        );
        draw_rectangle_outline(canvas, swatch_x, swatch_y, swatch_size, swatch_size, pack_rgb_pixel(232U, 236U, 242U), 0);

        char label[32] = {};
        (void)snprintf(label, sizeof(label), "%02zu:%02u", block_index + 1U, (unsigned int)block.janome_color_index);
        uint32_t label_color = pack_rgb_pixel(240U, 242U, 248U);
        if (!block.enabled) {
            label_color = pack_rgb_pixel(124U, 130U, 140U);
        }
        const int32_t saved_clip_max_x = canvas->clip_max_x;
        if (buttons_start_x - pad_4 < canvas->clip_max_x) {
            canvas->clip_max_x = buttons_start_x - pad_4;
        }
        draw_ui_text_vcentered(
            canvas,
            &state->ui_font,
            swatch_x + swatch_size + pad_6,
            row_y,
            row_height,
            label,
            label_color
        );
        canvas->clip_max_x = saved_clip_max_x;

        const int32_t button_y = row_y + (row_height - btn_size) / 2;
        for (int32_t action_index = 0; action_index < 5; action_index += 1) {
            const int32_t button_x = buttons_start_x + action_index * (btn_size + btn_gap);
            const bool hovered = point_is_in_box(
                state->mouse_screen_x,
                state->mouse_screen_y,
                button_x,
                button_y,
                btn_size,
                btn_size
            );
            uint32_t button_background = pack_rgb_pixel(60U, 65U, 75U);
            uint32_t text_color = pack_rgb_pixel(234U, 238U, 244U);
            const char *symbol = row_symbols[action_index];
            if (action_index == 0) {
                if (block.enabled) {
                    button_background = pack_rgb_pixel(42U, 118U, 72U);
                } else {
                    button_background = pack_rgb_pixel(46U, 48U, 54U);
                    symbol = "-";
                    text_color = pack_rgb_pixel(140U, 144U, 152U);
                }
            } else if (action_index == 4) {
                button_background = pack_rgb_pixel(132U, 48U, 48U);
            }
            if (hovered) {
                button_background = pack_rgb_pixel(88U, 102U, 124U);
            }
            draw_filled_rectangle(canvas, button_x, button_y, btn_size, btn_size, button_background);
            draw_rectangle_outline(canvas, button_x, button_y, btn_size, btn_size, pack_rgb_pixel(106U, 114U, 128U), 0);
            const UiRect sym_rect = {button_x, button_y, btn_size, btn_size};
            draw_ui_text_centered(canvas, &state->ui_font, sym_rect, symbol, text_color);
        }
    }

    reset_canvas_clip(canvas);
}

static void render_bottom_bar(
    PixelCanvas *canvas,
    const EditorState *state,
    const EditorLayout *layout,
    DesignStats stats
) {
    const double ui_scale = effective_ui_scale(state);
    const UiRect bar = layout->bottom_bar;
    const int32_t half_h = bar.height / 2;
    const int32_t pad_8 = scale_ui_px(ui_scale, 8);
    draw_filled_rectangle(canvas, bar.x, bar.y, bar.width, bar.height, pack_rgb_pixel(26U, 29U, 35U));
    draw_line_clipped(canvas, 0, bar.y, bar.width, bar.y, pack_rgb_pixel(70U, 76U, 88U), 0);

    char sim_label[24] = {};
    (void)snprintf(sim_label, sizeof(sim_label), "Sim:%3d%%", (int)llround(state->simulator_percent));
    draw_ui_text_vcentered(canvas, &state->ui_font, pad_8, bar.y, half_h, sim_label, pack_rgb_pixel(204U, 222U, 246U));

    const UiRect track = layout->slider_track;
    if (track.width >= scale_ui_px(ui_scale, 40)) {
        const int32_t track_bar_h = scale_ui_px(ui_scale, 10);
        const int32_t bar_y = track.y + (track.height - track_bar_h) / 2;
        const int32_t knob_w = scale_ui_px(ui_scale, 12);
        draw_filled_rectangle(canvas, track.x, bar_y, track.width, track_bar_h, pack_rgb_pixel(54U, 60U, 72U));
        const int32_t fill_w = (int32_t)llround((double)track.width * (state->simulator_percent / 100.0));
        draw_filled_rectangle(canvas, track.x, bar_y, fill_w, track_bar_h, pack_rgb_pixel(56U, 146U, 232U));
        draw_rectangle_outline(canvas, track.x, bar_y, track.width, track_bar_h, pack_rgb_pixel(104U, 116U, 136U), 0);
        const int32_t knob_x = track.x + fill_w - knob_w / 2;
        draw_filled_rectangle(canvas, knob_x, track.y, knob_w, track.height, pack_rgb_pixel(236U, 242U, 250U));
        draw_rectangle_outline(canvas, knob_x, track.y, knob_w, track.height, pack_rgb_pixel(32U, 88U, 152U), 0);
    }

    double width_mm = 0.0;
    double height_mm = 0.0;
    if (state->design.has_bounds) {
        width_mm = (double)((int64_t)state->design.maximum_x - (int64_t)state->design.minimum_x) * 0.1;
        height_mm = (double)((int64_t)state->design.maximum_y - (int64_t)state->design.minimum_y) * 0.1;
    }
    char metrics[96] = {};
    (void)snprintf(
        metrics,
        sizeof(metrics),
        "%.1fx%.1fmm St:%zu C:%zu",
        width_mm,
        height_mm,
        stats.stitch_count,
        stats.color_count
    );
    const int32_t metrics_w = measure_ui_text_width(&state->ui_font, metrics);
    int32_t metrics_x = bar.width - metrics_w - scale_ui_px(ui_scale, 10);
    const int32_t min_metrics_x = track.x + track.width + scale_ui_px(ui_scale, 12);
    if (metrics_x < min_metrics_x) {
        metrics_x = min_metrics_x;
    }
    canvas->clip_min_x = track.x + track.width + scale_ui_px(ui_scale, 10);
    canvas->clip_max_x = bar.width - pad_8;
    draw_ui_text_vcentered(canvas, &state->ui_font, metrics_x, bar.y, half_h, metrics, pack_rgb_pixel(224U, 230U, 240U));
    reset_canvas_clip(canvas);

    const bool out_of_hoop =
        design_exceeds_hoop(&state->design, state->hoop_preset_index, state->hoop_rotated);
    int32_t status_max_x = bar.width - scale_ui_px(ui_scale, 10);
    const int32_t row2_y = bar.y + half_h;
    const int32_t row2_h = bar.height - half_h;
    if (out_of_hoop) {
        const char *warn_text = "OUT OF HOOP!";
        const int32_t badge_w = measure_ui_text_width(&state->ui_font, warn_text) + scale_ui_px(ui_scale, 16);
        const int32_t badge_h = scale_ui_px(ui_scale, 22);
        const int32_t badge_x = bar.width - badge_w - pad_8;
        const int32_t badge_y = row2_y + (row2_h - badge_h) / 2;
        status_max_x = badge_x - pad_8;
        draw_filled_rectangle(canvas, badge_x, badge_y, badge_w, badge_h, pack_rgb_pixel(204U, 30U, 30U));
        draw_rectangle_outline(canvas, badge_x, badge_y, badge_w, badge_h, pack_rgb_pixel(255U, 196U, 196U), 0);
        const UiRect warn_rect = {badge_x, badge_y, badge_w, badge_h};
        draw_ui_text_centered(canvas, &state->ui_font, warn_rect, warn_text, pack_rgb_pixel(255U, 255U, 255U));
    }

    canvas->clip_min_x = pad_8;
    canvas->clip_max_x = status_max_x;
    draw_ui_text_vcentered(canvas, &state->ui_font, pad_8, row2_y, row2_h, state->status_message, pack_rgb_pixel(154U, 214U, 255U));
    reset_canvas_clip(canvas);
}

static void render_palette_modal(PixelCanvas *canvas, EditorState *state) {
    if (state->palette_target == PALETTE_TARGET_NONE) {
        return;
    }
    const double ui_scale = effective_ui_scale(state);

    draw_blended_rectangle(canvas, 0, 0, canvas->width, canvas->height, 10U, 12U, 16U, 165U);
    const PaletteModalLayout layout = compute_palette_modal_layout(state, canvas->width, canvas->height);
    draw_filled_rectangle(canvas, layout.modal_left, layout.modal_top, layout.modal_width, layout.modal_height, pack_rgb_pixel(36U, 40U, 48U));
    draw_rectangle_outline(canvas, layout.modal_left, layout.modal_top, layout.modal_width, layout.modal_height, pack_rgb_pixel(130U, 170U, 224U), 0);

    draw_ui_text_vcentered(
        canvas,
        &state->ui_font,
        layout.modal_left + scale_ui_px(ui_scale, 20),
        layout.modal_top,
        scale_ui_px(ui_scale, 46),
        "JEF 79-Color Palette (Esc)",
        pack_rgb_pixel(245U, 248U, 252U)
    );

    state->hovered_palette_index = -1;
    for (int32_t palette_index = 0; palette_index < (int32_t)JANOME_PALETTE_COUNT; palette_index += 1) {
        const int32_t column = palette_index % layout.columns;
        const int32_t row = palette_index / layout.columns;
        const int32_t cell_x = layout.grid_left + column * (layout.cell_size + layout.cell_gap);
        const int32_t cell_y = layout.grid_top + row * (layout.cell_size + layout.cell_gap);
        const RgbColor color = janome_palette[palette_index];
        const bool hovered = point_is_in_box(
            state->mouse_screen_x,
            state->mouse_screen_y,
            cell_x,
            cell_y,
            layout.cell_size,
            layout.cell_size
        );
        if (hovered) {
            state->hovered_palette_index = palette_index;
        }

        draw_filled_rectangle(canvas, cell_x, cell_y, layout.cell_size, layout.cell_size, pack_rgb_pixel(color.red, color.green, color.blue));
        uint32_t border_color = pack_rgb_pixel(90U, 96U, 108U);
        if (hovered) {
            border_color = pack_rgb_pixel(255U, 255U, 255U);
            draw_rectangle_outline(canvas, cell_x - 2, cell_y - 2, layout.cell_size + 4, layout.cell_size + 4, border_color, 0);
        }
        draw_rectangle_outline(canvas, cell_x, cell_y, layout.cell_size, layout.cell_size, border_color, 0);

        char index_text[16] = {};
        (void)snprintf(index_text, sizeof(index_text), "%02d", palette_index);
        const uint32_t luminance = compute_rgb_luminance(color);
        uint32_t label_color = pack_rgb_pixel(255U, 255U, 255U);
        if (luminance > 140U) {
            label_color = pack_rgb_pixel(16U, 18U, 22U);
        }
        const UiRect cell_rect = {cell_x, cell_y, layout.cell_size, layout.cell_size};
        draw_ui_text_centered(canvas, &state->ui_font, cell_rect, index_text, label_color);
    }

    const int32_t footer_y = layout.modal_top + layout.modal_height - scale_ui_px(ui_scale, 44);
    const int32_t footer_h = scale_ui_px(ui_scale, 28);
    if (state->hovered_palette_index >= 0 && state->hovered_palette_index < (int32_t)JANOME_PALETTE_COUNT) {
        const RgbColor color = janome_palette[state->hovered_palette_index];
        const int32_t swatch_x = layout.modal_left + scale_ui_px(ui_scale, 20);
        const int32_t swatch_w = scale_ui_px(ui_scale, 34);
        draw_filled_rectangle(canvas, swatch_x, footer_y, swatch_w, footer_h, pack_rgb_pixel(color.red, color.green, color.blue));
        draw_rectangle_outline(canvas, swatch_x, footer_y, swatch_w, footer_h, pack_rgb_pixel(235U, 240U, 248U), 0);
        char info_text[64] = {};
        (void)snprintf(
            info_text,
            sizeof(info_text),
            "JEF #%02d  #%02X%02X%02X",
            state->hovered_palette_index,
            (unsigned int)color.red,
            (unsigned int)color.green,
            (unsigned int)color.blue
        );
        draw_ui_text_vcentered(
            canvas,
            &state->ui_font,
            layout.modal_left + scale_ui_px(ui_scale, 64),
            footer_y,
            footer_h,
            info_text,
            pack_rgb_pixel(234U, 238U, 244U)
        );
    } else {
        draw_ui_text_vcentered(
            canvas,
            &state->ui_font,
            layout.modal_left + scale_ui_px(ui_scale, 20),
            footer_y,
            footer_h,
            "Hover: #ID & HEX | Click: pick",
            pack_rgb_pixel(180U, 190U, 204U)
        );
    }
}

static void render_strip_modal(PixelCanvas *canvas, const EditorState *state) {
    if (!state->strip_modal_open) {
        return;
    }
    const double ui_scale = effective_ui_scale(state);

    draw_blended_rectangle(canvas, 0, 0, canvas->width, canvas->height, 10U, 12U, 16U, 175U);
    const StripModalLayout layout = compute_strip_modal_layout(state, canvas->width, canvas->height);
    draw_filled_rectangle(canvas, layout.modal_left, layout.modal_top, layout.modal_width, layout.modal_height, pack_rgb_pixel(34U, 38U, 46U));
    draw_rectangle_outline(canvas, layout.modal_left, layout.modal_top, layout.modal_width, layout.modal_height, pack_rgb_pixel(124U, 176U, 232U), 0);

    draw_ui_text_vcentered(
        canvas,
        &state->ui_font,
        layout.modal_left + scale_ui_px(ui_scale, 12),
        layout.modal_top,
        scale_ui_px(ui_scale, 40),
        "Strip Designer (LMB draw, RMB erase)",
        pack_rgb_pixel(242U, 246U, 252U)
    );

    for (size_t button_index = 0U; button_index < layout.button_count; button_index += 1U) {
        draw_ui_button(canvas, &state->ui_font, &layout.buttons[button_index], state->mouse_screen_x, state->mouse_screen_y);
    }

    draw_filled_rectangle(canvas, layout.editor_left, layout.editor_top, layout.editor_width, layout.editor_height, pack_rgb_pixel(246U, 247U, 250U));
    draw_rectangle_outline(canvas, layout.editor_left, layout.editor_top, layout.editor_width, layout.editor_height, pack_rgb_pixel(92U, 100U, 114U), 0);

    const int32_t grid_pixel_width = state->strip_pattern.width * layout.editor_cell_size;
    const int32_t grid_pixel_height = state->strip_pattern.height * layout.editor_cell_size;
    const int32_t grid_origin_x = layout.editor_left + (layout.editor_width - grid_pixel_width) / 2;
    const int32_t grid_origin_y = layout.editor_top + (layout.editor_height - grid_pixel_height) / 2;

    for (int32_t row = 0; row < state->strip_pattern.height; row += 1) {
        for (int32_t column = 0; column < state->strip_pattern.width; column += 1) {
            const int32_t cell_x = grid_origin_x + column * layout.editor_cell_size;
            const int32_t cell_y = grid_origin_y + row * layout.editor_cell_size;
            const uint8_t color_index = state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + column];
            if (color_index > 0U && color_index < JANOME_PALETTE_COUNT) {
                const RgbColor color = janome_palette[color_index];
                const uint32_t packed = pack_rgb_pixel(color.red, color.green, color.blue);
                draw_thick_line_clipped(canvas, cell_x + 2, cell_y + 2, cell_x + layout.editor_cell_size - 3, cell_y + layout.editor_cell_size - 3, packed);
                draw_thick_line_clipped(canvas, cell_x + layout.editor_cell_size - 3, cell_y + 2, cell_x + 2, cell_y + layout.editor_cell_size - 3, packed);
            }
            draw_rectangle_outline(canvas, cell_x, cell_y, layout.editor_cell_size, layout.editor_cell_size, pack_rgb_pixel(204U, 210U, 220U), 0);
        }
    }

    draw_filled_rectangle(canvas, layout.preview_left, layout.preview_top, layout.preview_width, layout.preview_height, pack_rgb_pixel(242U, 244U, 248U));
    draw_rectangle_outline(canvas, layout.preview_left, layout.preview_top, layout.preview_width, layout.preview_height, pack_rgb_pixel(92U, 100U, 114U), 0);

    const int32_t total_columns = state->strip_pattern.width * state->strip_pattern.repeat_x;
    const int32_t total_rows = state->strip_pattern.height * state->strip_pattern.repeat_y;
    const double scale_x = (double)(layout.preview_width - 20) / (double)total_columns;
    const double scale_y = (double)(layout.preview_height - 20) / (double)total_rows;
    double preview_cell = scale_x;
    if (scale_y < preview_cell) {
        preview_cell = scale_y;
    }
    if (preview_cell > 28.0) {
        preview_cell = 28.0;
    }

    const int32_t preview_total_width = (int32_t)llround((double)total_columns * preview_cell);
    const int32_t preview_total_height = (int32_t)llround((double)total_rows * preview_cell);
    const int32_t preview_start_x = layout.preview_left + (layout.preview_width - preview_total_width) / 2;
    const int32_t preview_start_y = layout.preview_top + (layout.preview_height - preview_total_height) / 2;

    for (int32_t row = 0; row < total_rows; row += 1) {
        const int32_t source_row = row % state->strip_pattern.height;
        const int32_t top_edge = preview_start_y + (int32_t)llround((double)row * preview_cell);
        const int32_t bottom_edge = preview_start_y + (int32_t)llround((double)(row + 1) * preview_cell);
        for (int32_t column = 0; column < total_columns; column += 1) {
            const int32_t source_column = column % state->strip_pattern.width;
            const uint8_t color_index = state->strip_pattern.cells[source_row * STRIP_MAX_GRID_DIM + source_column];
            if (color_index == 0U || color_index >= JANOME_PALETTE_COUNT) {
                continue;
            }
            const int32_t left_edge = preview_start_x + (int32_t)llround((double)column * preview_cell);
            const int32_t right_edge = preview_start_x + (int32_t)llround((double)(column + 1) * preview_cell);
            const RgbColor color = janome_palette[color_index];
            const uint32_t packed = pack_rgb_pixel(color.red, color.green, color.blue);
            if (preview_cell >= 5.0) {
                draw_line_clipped(canvas, left_edge, top_edge, right_edge - 1, bottom_edge - 1, packed, 0);
                draw_line_clipped(canvas, right_edge - 1, top_edge, left_edge, bottom_edge - 1, packed, 0);
            } else {
                int32_t fill_width = right_edge - left_edge;
                int32_t fill_height = bottom_edge - top_edge;
                if (fill_width < 1) {
                    fill_width = 1;
                }
                if (fill_height < 1) {
                    fill_height = 1;
                }
                draw_filled_rectangle(canvas, left_edge, top_edge, fill_width, fill_height, packed);
            }
        }
    }

    const double final_width_mm = (double)(total_columns * state->strip_pattern.pitch_units) * 0.1;
    const double final_height_mm = (double)(total_rows * state->strip_pattern.pitch_units) * 0.1;
    char summary[96] = {};
    (void)snprintf(summary, sizeof(summary), "%.1fx%.1fmm (%dx%d)", final_width_mm, final_height_mm, total_columns, total_rows);
    const int32_t footer_y = layout.modal_top + layout.modal_height - scale_ui_px(ui_scale, 40);
    const int32_t footer_h = scale_ui_px(ui_scale, 30);
    const int32_t summary_x = layout.modal_left + scale_ui_px(ui_scale, 12);
    if (layout.button_count >= 4U) {
        const int32_t insert_left = layout.buttons[layout.button_count - 4U].left_x - scale_ui_px(ui_scale, 8);
        if (insert_left > summary_x) {
            canvas->clip_min_x = summary_x;
            canvas->clip_max_x = insert_left;
        }
    }
    draw_ui_text_vcentered(canvas, &state->ui_font, summary_x, footer_y, footer_h, summary, pack_rgb_pixel(204U, 232U, 255U));
    reset_canvas_clip(canvas);
}

static void render_draw_x_cursor_preview(
    PixelCanvas *canvas,
    const EditorState *state,
    RenderViewport viewport
) {
    const int32_t world_x = viewport_screen_to_world_x(viewport, state->camera, state->mouse_screen_x);
    const int32_t world_y = viewport_screen_to_world_y(viewport, state->camera, state->mouse_screen_y);
    int32_t cell_column = 0;
    int32_t cell_row = 0;
    world_to_grid_cell(world_x, world_y, state->grid, &cell_column, &cell_row);
    const int32_t pitch = state->grid.cross_pitch_units;
    const int32_t cell_min_x = state->grid.grid_origin_x + cell_column * pitch;
    const int32_t cell_min_y = state->grid.grid_origin_y + cell_row * pitch;
    const int32_t left_edge = viewport_world_to_screen_x(viewport, state->camera, cell_min_x);
    const int32_t right_edge = viewport_world_to_screen_x(viewport, state->camera, cell_min_x + pitch);
    const int32_t top_edge = viewport_world_to_screen_y(viewport, state->camera, cell_min_y);
    const int32_t bottom_edge = viewport_world_to_screen_y(viewport, state->camera, cell_min_y + pitch);

    RgbColor preview_rgb = janome_palette[10];
    if (state->active_color_block_index < state->design.block_count) {
        preview_rgb = state->design.blocks[state->active_color_block_index].display_color;
    }
    const uint32_t cross_color = pack_rgb_pixel(preview_rgb.red, preview_rgb.green, preview_rgb.blue);

    if (state->view_mode == VIEW_MODE_BLOCKS) {
        draw_blended_rectangle(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, preview_rgb.red, preview_rgb.green, preview_rgb.blue, 148U);
        draw_rectangle_outline(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, cross_color, 0);
        return;
    }

    if (state->view_mode == VIEW_MODE_THREAD) {
        draw_blended_rectangle(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, preview_rgb.red, preview_rgb.green, preview_rgb.blue, 55U);
        uint32_t preview_core = cross_color;
        uint32_t preview_outline = cross_color;
        compute_thread_colors(preview_rgb, false, &preview_core, &preview_outline);
        const int32_t preview_stroke =
            compute_thread_stroke_px(state->thread_thickness_index, state->camera.pixels_per_unit);
        if (preview_stroke >= 3) {
            draw_thread_capsule_clipped(canvas, left_edge, top_edge, right_edge, bottom_edge, preview_stroke, preview_core, preview_outline);
            draw_thread_capsule_clipped(canvas, right_edge, top_edge, left_edge, bottom_edge, preview_stroke, preview_core, preview_outline);
        } else {
            draw_thick_line_clipped(canvas, left_edge, top_edge, right_edge, bottom_edge, preview_core);
            draw_thick_line_clipped(canvas, right_edge, top_edge, left_edge, bottom_edge, preview_core);
        }
        draw_rectangle_outline(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, cross_color, 0);
        return;
    }

    draw_blended_rectangle(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, preview_rgb.red, preview_rgb.green, preview_rgb.blue, 70U);
    draw_thick_line_clipped(canvas, left_edge, top_edge, right_edge, bottom_edge, cross_color);
    draw_thick_line_clipped(canvas, right_edge, top_edge, left_edge, bottom_edge, cross_color);
    draw_rectangle_outline(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, cross_color, 0);
}

static void render_box_selection_overlay(
    PixelCanvas *canvas,
    const EditorState *state,
    RenderViewport viewport
) {
    if (!state->has_box_selection) {
        return;
    }
    if (state->active_tool == TOOL_SPLIT_VERTICAL) {
        const int32_t line_x = viewport_world_to_screen_x(viewport, state->camera, state->split_coordinate);
        draw_line_clipped(canvas, line_x, viewport.top, line_x, viewport.top + viewport.height, pack_rgb_pixel(230U, 52U, 36U), 6);
        draw_line_clipped(canvas, line_x + 1, viewport.top, line_x + 1, viewport.top + viewport.height, pack_rgb_pixel(230U, 52U, 36U), 6);
        return;
    }
    if (state->active_tool == TOOL_SPLIT_HORIZONTAL) {
        const int32_t line_y = viewport_world_to_screen_y(viewport, state->camera, state->split_coordinate);
        draw_line_clipped(canvas, viewport.left, line_y, viewport.left + viewport.width, line_y, pack_rgb_pixel(230U, 52U, 36U), 6);
        draw_line_clipped(canvas, viewport.left, line_y + 1, viewport.left + viewport.width, line_y + 1, pack_rgb_pixel(230U, 52U, 36U), 6);
        return;
    }

    int32_t left_edge = viewport_world_to_screen_x(viewport, state->camera, state->box_start_x);
    int32_t right_edge = viewport_world_to_screen_x(viewport, state->camera, state->box_end_x);
    int32_t top_edge = viewport_world_to_screen_y(viewport, state->camera, state->box_start_y);
    int32_t bottom_edge = viewport_world_to_screen_y(viewport, state->camera, state->box_end_y);
    if (right_edge < left_edge) {
        const int32_t swap_x = left_edge;
        left_edge = right_edge;
        right_edge = swap_x;
    }
    if (bottom_edge < top_edge) {
        const int32_t swap_y = top_edge;
        top_edge = bottom_edge;
        bottom_edge = swap_y;
    }
    uint8_t tint_red = 40U;
    uint8_t tint_green = 152U;
    uint8_t tint_blue = 228U;
    if (state->active_tool == TOOL_CUT_OUT) {
        tint_red = 224U;
        tint_green = 52U;
        tint_blue = 52U;
    } else if (state->active_tool == TOOL_CROP_KEEP || state->active_tool == TOOL_CLONE) {
        tint_red = 42U;
        tint_green = 172U;
        tint_blue = 92U;
    }
    draw_blended_rectangle(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, tint_red, tint_green, tint_blue, 36U);
    draw_rectangle_outline(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, pack_rgb_pixel(tint_red, tint_green, tint_blue), 0);

    /* Live dimensions badge in crosses and mm for Cut/Crop/Clone/Sel X box. */
    int64_t world_width = (int64_t)state->box_end_x - (int64_t)state->box_start_x;
    if (world_width < 0LL) {
        world_width = -world_width;
    }
    int64_t world_height = (int64_t)state->box_end_y - (int64_t)state->box_start_y;
    if (world_height < 0LL) {
        world_height = -world_height;
    }
    if (world_width > 0LL || world_height > 0LL) {
        int32_t pitch = state->grid.cross_pitch_units;
        if (pitch < MIN_CROSS_PITCH_UNITS) {
            pitch = DEFAULT_CROSS_PITCH_UNITS;
        }
        const int64_t crosses_x = (world_width + (int64_t)pitch / 2LL) / (int64_t)pitch;
        const int64_t crosses_y = (world_height + (int64_t)pitch / 2LL) / (int64_t)pitch;
        char badge_text[64] = {};
        (void)snprintf(
            badge_text,
            sizeof(badge_text),
            "%lldx%lld X | %.1fx%.1fmm",
            (long long)crosses_x,
            (long long)crosses_y,
            (double)world_width * 0.1,
            (double)world_height * 0.1
        );
        const double ui_scale = effective_ui_scale(state);
        const int32_t badge_w = measure_ui_text_width(&state->ui_font, badge_text) + scale_ui_px(ui_scale, 16);
        const int32_t badge_h = scale_ui_px(ui_scale, 24);
        const int32_t badge_gap = scale_ui_px(ui_scale, 4);
        int32_t badge_x = left_edge;
        int32_t badge_y = top_edge - badge_h - badge_gap;
        if (badge_y < viewport.top + badge_gap) {
            badge_y = top_edge + badge_gap;
        }
        if (badge_x < viewport.left + badge_gap) {
            badge_x = viewport.left + badge_gap;
        }
        if (badge_x + badge_w > viewport.left + viewport.width - badge_gap) {
            badge_x = viewport.left + viewport.width - badge_w - badge_gap;
        }
        draw_filled_rectangle(canvas, badge_x, badge_y, badge_w, badge_h, pack_rgb_pixel(22U, 26U, 34U));
        draw_rectangle_outline(canvas, badge_x, badge_y, badge_w, badge_h, pack_rgb_pixel(tint_red, tint_green, tint_blue), 0);
        const UiRect badge_rect = {badge_x, badge_y, badge_w, badge_h};
        draw_ui_text_centered(canvas, &state->ui_font, badge_rect, badge_text, pack_rgb_pixel(242U, 246U, 252U));
    }
}

static uint64_t fnv1a_mix_u64(uint64_t hash_value, uint64_t data) {
    hash_value ^= data;
    hash_value *= 1099511628211ULL;
    return hash_value;
}

static uint64_t fnv1a_mix_string(uint64_t hash_value, const char *text) {
    size_t offset = 0U;
    while (text[offset] != '\0') {
        hash_value ^= (uint64_t)(uint8_t)text[offset];
        hash_value *= 1099511628211ULL;
        offset += 1U;
    }
    return hash_value;
}

static uint64_t compute_view_signature(const EditorState *state, size_t max_visible_stitches) {
    uint64_t hash_value = 14695981039346656037ULL;
    uint64_t bits_center_x = 0ULL;
    uint64_t bits_center_y = 0ULL;
    uint64_t bits_scale = 0ULL;
    memcpy(&bits_center_x, &state->camera.center_x, sizeof(bits_center_x));
    memcpy(&bits_center_y, &state->camera.center_y, sizeof(bits_center_y));
    memcpy(&bits_scale, &state->camera.pixels_per_unit, sizeof(bits_scale));

    hash_value = fnv1a_mix_u64(hash_value, bits_center_x);
    hash_value = fnv1a_mix_u64(hash_value, bits_center_y);
    hash_value = fnv1a_mix_u64(hash_value, bits_scale);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)(uint32_t)state->grid.cross_pitch_units);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)(uint32_t)state->grid.grid_origin_x);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)(uint32_t)state->grid.grid_origin_y);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->view_mode);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->thread_thickness_index);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->show_grid);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->show_jumps);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->show_locks);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->hoop_preset_index);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->hoop_rotated);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)max_visible_stitches);
    return hash_value;
}

static uint64_t compute_ui_chrome_signature(
    const EditorState *state,
    const EditorLayout *layout,
    int32_t canvas_width,
    int32_t canvas_height,
    bool mouse_in_viewport
) {
    uint64_t hash_value = 14695981039346656037ULL;
    uint64_t bits_sim = 0ULL;
    memcpy(&bits_sim, &state->simulator_percent, sizeof(bits_sim));
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)(uint32_t)canvas_width);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)(uint32_t)canvas_height);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->right_panel_collapsed);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->active_tool);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->view_mode);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->thread_thickness_index);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->show_grid);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->show_jumps);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->show_locks);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->hoop_preset_index);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->hoop_rotated);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->drop_merge_mode);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)(uint32_t)state->grid.cross_pitch_units);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)(uint32_t)state->grid.cross_passes);
    hash_value = fnv1a_mix_u64(hash_value, bits_sim);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->active_color_block_index);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)(uint32_t)state->color_scroll_offset);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->selected_segment_count);
    hash_value = fnv1a_mix_u64(hash_value, state->design_revision);
    hash_value = fnv1a_mix_u64(hash_value, state->design.geometry_revision);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->is_dirty);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)state->has_file_path);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)layout->viewport.width);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)(uint32_t)state->ui_font.pixel_size);
    hash_value = fnv1a_mix_u64(hash_value, (uint64_t)llround(effective_ui_scale(state) * 100.0));
    hash_value = fnv1a_mix_string(hash_value, state->current_file_path);
    hash_value = fnv1a_mix_string(hash_value, state->status_message);

    if (!mouse_in_viewport) {
        hash_value = fnv1a_mix_u64(hash_value, 1ULL);
        hash_value = fnv1a_mix_u64(hash_value, (uint64_t)(uint32_t)state->mouse_screen_x);
        hash_value = fnv1a_mix_u64(hash_value, (uint64_t)(uint32_t)state->mouse_screen_y);
    } else {
        hash_value = fnv1a_mix_u64(hash_value, 0ULL);
    }
    return hash_value;
}

static void union_overlay_rect(
    bool *has_rect,
    int32_t *min_x,
    int32_t *min_y,
    int32_t *max_x,
    int32_t *max_y,
    int32_t add_min_x,
    int32_t add_min_y,
    int32_t add_max_x,
    int32_t add_max_y
) {
    if (add_max_x <= add_min_x || add_max_y <= add_min_y) {
        return;
    }
    if (!(*has_rect)) {
        *min_x = add_min_x;
        *min_y = add_min_y;
        *max_x = add_max_x;
        *max_y = add_max_y;
        *has_rect = true;
        return;
    }
    if (add_min_x < *min_x) {
        *min_x = add_min_x;
    }
    if (add_min_y < *min_y) {
        *min_y = add_min_y;
    }
    if (add_max_x > *max_x) {
        *max_x = add_max_x;
    }
    if (add_max_y > *max_y) {
        *max_y = add_max_y;
    }
}

void render_editor_frame(PixelCanvas *canvas, EditorState *state) {
    if (!state->is_dragging_box) {
        refresh_live_preview(state);
    }

    const EditorLayout layout = compute_editor_layout(state, canvas->width, canvas->height);
    const RenderViewport viewport = {
        .left = layout.viewport.x,
        .top = layout.viewport.y,
        .width = layout.viewport.width,
        .height = layout.viewport.height
    };
    const DesignStats total_stats = compute_design_stats(&state->design);
    size_t max_visible_stitches = SIZE_MAX;
    if (state->simulator_percent < 99.95) {
        max_visible_stitches = (size_t)llround((double)total_stats.stitch_count * (state->simulator_percent / 100.0));
    }

    const bool any_modal_open =
        state->strip_modal_open || state->palette_target != PALETTE_TARGET_NONE;
    const bool show_start_screen = should_show_start_screen(state);
    const bool mouse_in_viewport = rect_contains_point(layout.viewport, state->mouse_screen_x, state->mouse_screen_y);

    StitchLayerCache *cache = &state->stitch_cache;
    const bool target_changed =
        cache->last_target_pixels != canvas->pixels ||
        cache->last_target_width != canvas->width ||
        cache->last_target_height != canvas->height ||
        cache->last_viewport_left != viewport.left ||
        cache->last_viewport_top != viewport.top;

    bool need_full_viewport_blit =
        target_changed ||
        any_modal_open ||
        cache->last_modal_was_open ||
        show_start_screen ||
        cache->last_had_start_screen;

    bool has_curr_overlay = false;
    int32_t curr_min_x = 0;
    int32_t curr_min_y = 0;
    int32_t curr_max_x = 0;
    int32_t curr_max_y = 0;

    bool has_dirty = false;
    int32_t dirty_min_x = 0;
    int32_t dirty_min_y = 0;
    int32_t dirty_max_x = 0;
    int32_t dirty_max_y = 0;

    canvas->clip_min_x = viewport.left;
    canvas->clip_min_y = viewport.top;
    canvas->clip_max_x = viewport.left + viewport.width;
    canvas->clip_max_y = viewport.top + viewport.height;

    if (show_start_screen) {
        draw_filled_rectangle(
            canvas,
            viewport.left,
            viewport.top,
            viewport.width,
            viewport.height,
            pack_rgb_pixel(245U, 246U, 248U)
        );
        draw_cross_grid_background(canvas, viewport, state->camera, state->grid, state->show_grid);
        draw_hoop_overlay(canvas, viewport, state->camera, &state->design, state->hoop_preset_index, state->hoop_rotated);
        render_start_screen(canvas, state);
        union_overlay_rect(&has_dirty, &dirty_min_x, &dirty_min_y, &dirty_max_x, &dirty_max_y, 0, 0, canvas->width, canvas->height);
    } else {
        DesignRenderParams main_params = {
            .viewport = viewport,
            .camera = state->camera,
            .grid = state->grid,
            .view_mode = state->view_mode,
            .thread_thickness_index = state->thread_thickness_index,
            .show_jumps = state->show_jumps,
            .show_locks = state->show_locks,
            .draw_overlays = true,
            .world_offset_x = 0,
            .world_offset_y = 0,
            .keep_preview_mask = nullptr,
            .keep_preview_mask_length = 0U,
            .selection_mask = state->selection_mask,
            .selection_mask_length = state->selection_mask_length,
            .hover_mask = nullptr,
            .hover_mask_length = 0U,
            .maximum_stitches_to_draw = max_visible_stitches,
            .block_dedup = &state->block_dedup
        };
        if (!state->is_dragging_box && state->has_edit_preview_mask && state->edit_preview_keep_mask != nullptr) {
            main_params.keep_preview_mask = state->edit_preview_keep_mask;
            main_params.keep_preview_mask_length = state->edit_preview_mask_length;
        }

        const uint64_t view_signature = compute_view_signature(state, max_visible_stitches);
        const uint64_t effective_design_rev = state->design_revision ^ (state->design.geometry_revision << 1U);
        const uint64_t effective_mask_rev = state->mask_revision;
        bool cache_rebuilt = false;

        if (ensure_stitch_layer_cache(
                cache,
                &state->design,
                &main_params,
                effective_design_rev,
                view_signature,
                effective_mask_rev,
                state->show_grid,
                state->hoop_preset_index,
                state->hoop_rotated,
                &cache_rebuilt
            )) {
            if (cache_rebuilt || need_full_viewport_blit) {
                need_full_viewport_blit = true;
                blit_stitch_layer_cache(canvas, cache, viewport);
                union_overlay_rect(
                    &has_dirty,
                    &dirty_min_x,
                    &dirty_min_y,
                    &dirty_max_x,
                    &dirty_max_y,
                    viewport.left,
                    viewport.top,
                    viewport.left + viewport.width,
                    viewport.top + viewport.height
                );
            } else if (cache->has_prev_overlay) {
                blit_stitch_layer_cache_rect(
                    canvas,
                    cache,
                    viewport,
                    cache->prev_overlay_min_x,
                    cache->prev_overlay_min_y,
                    cache->prev_overlay_max_x,
                    cache->prev_overlay_max_y
                );
                union_overlay_rect(
                    &has_dirty,
                    &dirty_min_x,
                    &dirty_min_y,
                    &dirty_max_x,
                    &dirty_max_y,
                    cache->prev_overlay_min_x,
                    cache->prev_overlay_min_y,
                    cache->prev_overlay_max_x,
                    cache->prev_overlay_max_y
                );
            }
        } else {
            need_full_viewport_blit = true;
            draw_filled_rectangle(
                canvas,
                viewport.left,
                viewport.top,
                viewport.width,
                viewport.height,
                pack_rgb_pixel(245U, 246U, 248U)
            );
            draw_cross_grid_background(canvas, viewport, state->camera, state->grid, state->show_grid);
            draw_hoop_overlay(canvas, viewport, state->camera, &state->design, state->hoop_preset_index, state->hoop_rotated);
            if (state->design.chain_count > 0U) {
                render_design_chains(canvas, &state->design, &main_params);
            }
        }

        if (state->active_tool == TOOL_SELECT_ELEMENT &&
            state->hover_component_id < state->spatial_index.component_count &&
            state->spatial_index.valid &&
            state->spatial_index.component_chain_offsets != nullptr &&
            state->spatial_index.component_chains != nullptr) {
            const size_t begin = state->spatial_index.component_chain_offsets[state->hover_component_id];
            const size_t end = state->spatial_index.component_chain_offsets[state->hover_component_id + 1U];
            if (end > begin && end <= state->spatial_index.chain_count) {
                int32_t sub_min_x = 0;
                int32_t sub_min_y = 0;
                int32_t sub_max_x = 0;
                int32_t sub_max_y = 0;
                render_chain_subset_overlay(
                    canvas,
                    &state->design,
                    &main_params,
                    state->spatial_index.component_chains + begin,
                    end - begin,
                    2U,
                    &sub_min_x,
                    &sub_min_y,
                    &sub_max_x,
                    &sub_max_y
                );
                union_overlay_rect(&has_curr_overlay, &curr_min_x, &curr_min_y, &curr_max_x, &curr_max_y, sub_min_x, sub_min_y, sub_max_x, sub_max_y);
            }
        }
    }

    if (state->has_floating_fragment) {
        const DesignRenderParams fragment_params = {
            .viewport = viewport,
            .camera = state->camera,
            .grid = state->grid,
            .view_mode = state->view_mode,
            .thread_thickness_index = state->thread_thickness_index,
            .show_jumps = false,
            .show_locks = false,
            .draw_overlays = false,
            .world_offset_x = state->floating_offset_x,
            .world_offset_y = state->floating_offset_y,
            .keep_preview_mask = nullptr,
            .keep_preview_mask_length = 0U,
            .selection_mask = nullptr,
            .selection_mask_length = 0U,
            .hover_mask = nullptr,
            .hover_mask_length = 0U,
            .maximum_stitches_to_draw = SIZE_MAX,
            .block_dedup = &state->block_dedup
        };
        render_design_chains(canvas, &state->floating_fragment, &fragment_params);
        if (state->floating_fragment.has_bounds) {
            const int32_t box_left = viewport_world_to_screen_x(viewport, state->camera, state->floating_fragment.minimum_x + state->floating_offset_x);
            const int32_t box_right = viewport_world_to_screen_x(viewport, state->camera, state->floating_fragment.maximum_x + state->floating_offset_x);
            const int32_t box_top = viewport_world_to_screen_y(viewport, state->camera, state->floating_fragment.minimum_y + state->floating_offset_y);
            const int32_t box_bottom = viewport_world_to_screen_y(viewport, state->camera, state->floating_fragment.maximum_y + state->floating_offset_y);
            draw_rectangle_outline(canvas, box_left, box_top, box_right - box_left + 1, box_bottom - box_top + 1, pack_rgb_pixel(32U, 156U, 90U), 6);
            union_overlay_rect(
                &has_curr_overlay,
                &curr_min_x,
                &curr_min_y,
                &curr_max_x,
                &curr_max_y,
                box_left - MAX_THREAD_STROKE_PX - 4,
                box_top - MAX_THREAD_STROKE_PX - 4,
                box_right + MAX_THREAD_STROKE_PX + 6,
                box_bottom + MAX_THREAD_STROKE_PX + 6
            );
        }
    }

    if (!any_modal_open && !state->has_floating_fragment && mouse_in_viewport) {
        if (state->active_tool == TOOL_SELECT_CROSS && state->has_hover_cross_box) {
            const int32_t left_edge = viewport_world_to_screen_x(viewport, state->camera, state->hover_cross_box.minimum_x);
            const int32_t right_edge = viewport_world_to_screen_x(viewport, state->camera, state->hover_cross_box.maximum_x);
            const int32_t top_edge = viewport_world_to_screen_y(viewport, state->camera, state->hover_cross_box.minimum_y);
            const int32_t bottom_edge = viewport_world_to_screen_y(viewport, state->camera, state->hover_cross_box.maximum_y);
            draw_blended_rectangle(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, 0U, 180U, 255U, 54U);
            draw_rectangle_outline(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, pack_rgb_pixel(0U, 140U, 232U), 0);
            union_overlay_rect(&has_curr_overlay, &curr_min_x, &curr_min_y, &curr_max_x, &curr_max_y, left_edge - 1, top_edge - 1, right_edge + 2, bottom_edge + 2);
        } else if (state->active_tool == TOOL_DRAW_CROSS) {
            render_draw_x_cursor_preview(canvas, state, viewport);
            const int32_t world_x = viewport_screen_to_world_x(viewport, state->camera, state->mouse_screen_x);
            const int32_t world_y = viewport_screen_to_world_y(viewport, state->camera, state->mouse_screen_y);
            int32_t cell_col = 0;
            int32_t cell_row = 0;
            world_to_grid_cell(world_x, world_y, state->grid, &cell_col, &cell_row);
            const int32_t pitch = state->grid.cross_pitch_units;
            const int32_t cell_min_x = state->grid.grid_origin_x + cell_col * pitch;
            const int32_t cell_min_y = state->grid.grid_origin_y + cell_row * pitch;
            const int32_t left_edge = viewport_world_to_screen_x(viewport, state->camera, cell_min_x);
            const int32_t right_edge = viewport_world_to_screen_x(viewport, state->camera, cell_min_x + pitch);
            const int32_t top_edge = viewport_world_to_screen_y(viewport, state->camera, cell_min_y);
            const int32_t bottom_edge = viewport_world_to_screen_y(viewport, state->camera, cell_min_y + pitch);
            union_overlay_rect(
                &has_curr_overlay,
                &curr_min_x,
                &curr_min_y,
                &curr_max_x,
                &curr_max_y,
                left_edge - MAX_THREAD_STROKE_PX - 2,
                top_edge - MAX_THREAD_STROKE_PX - 2,
                right_edge + MAX_THREAD_STROKE_PX + 3,
                bottom_edge + MAX_THREAD_STROKE_PX + 3
            );
        }
    }

    if (state->has_box_selection) {
        render_box_selection_overlay(canvas, state, viewport);
        union_overlay_rect(
            &has_curr_overlay,
            &curr_min_x,
            &curr_min_y,
            &curr_max_x,
            &curr_max_y,
            viewport.left,
            viewport.top,
            viewport.left + viewport.width,
            viewport.top + viewport.height
        );
    }

    if (has_curr_overlay) {
        if (curr_min_x < viewport.left) {
            curr_min_x = viewport.left;
        }
        if (curr_min_y < viewport.top) {
            curr_min_y = viewport.top;
        }
        if (curr_max_x > viewport.left + viewport.width) {
            curr_max_x = viewport.left + viewport.width;
        }
        if (curr_max_y > viewport.top + viewport.height) {
            curr_max_y = viewport.top + viewport.height;
        }
        union_overlay_rect(&has_dirty, &dirty_min_x, &dirty_min_y, &dirty_max_x, &dirty_max_y, curr_min_x, curr_min_y, curr_max_x, curr_max_y);
    }

    const uint64_t ui_signature =
        compute_ui_chrome_signature(state, &layout, canvas->width, canvas->height, mouse_in_viewport);
    const bool need_ui_redraw =
        need_full_viewport_blit || ui_signature != cache->last_ui_signature;

    reset_canvas_clip(canvas);
    if (need_ui_redraw) {
        render_top_bar(canvas, state, &layout);
        render_left_tool_rail(canvas, state, &layout);
        render_right_panel(canvas, state, &layout);
        render_bottom_bar(canvas, state, &layout, total_stats);
        render_strip_modal(canvas, state);
        render_palette_modal(canvas, state);
        union_overlay_rect(&has_dirty, &dirty_min_x, &dirty_min_y, &dirty_max_x, &dirty_max_y, 0, 0, canvas->width, canvas->height);
    }

    cache->last_target_pixels = canvas->pixels;
    cache->last_target_width = canvas->width;
    cache->last_target_height = canvas->height;
    cache->last_viewport_left = viewport.left;
    cache->last_viewport_top = viewport.top;
    cache->last_ui_signature = ui_signature;
    cache->last_modal_was_open = any_modal_open;
    cache->last_had_start_screen = show_start_screen;
    cache->has_prev_overlay = has_curr_overlay;
    cache->prev_overlay_min_x = curr_min_x;
    cache->prev_overlay_min_y = curr_min_y;
    cache->prev_overlay_max_x = curr_max_x;
    cache->prev_overlay_max_y = curr_max_y;
    cache->has_dirty_rect = has_dirty;
    cache->dirty_min_x = dirty_min_x;
    cache->dirty_min_y = dirty_min_y;
    cache->dirty_max_x = dirty_max_x;
    cache->dirty_max_y = dirty_max_y;
}

void editor_set_ui_scale(EditorState *state, double ui_scale) {
    if (state == nullptr) {
        return;
    }
    double clamped_scale = ui_scale;
    if (clamped_scale < 1.0) {
        clamped_scale = 1.0;
    }
    if (clamped_scale > 3.0) {
        clamped_scale = 3.0;
    }
    const int32_t target_pixel_size =
        clamp_ui_font_pixel_size((int32_t)llround((double)UI_FONT_BASE_PIXEL_SIZE * clamped_scale));
    if (fabs(clamped_scale - state->ui_scale) < 0.01 &&
        state->ui_font.pixel_size == target_pixel_size) {
        return;
    }

    UiFont next_font = {};
    (void)init_ui_font(&next_font, target_pixel_size);
    free_ui_font(&state->ui_font);
    state->ui_font = next_font;
    state->ui_scale = clamped_scale;
    invalidate_stitch_layer_cache(&state->stitch_cache);
    state->needs_redraw = true;
}

void init_default_editor_state(EditorState *state) {
    *state = (EditorState){};
    history_init(&state->history);
    scratch_arena_init(&state->scratch);
    spatial_index_init(&state->spatial_index);
    cross_cluster_cache_init(&state->cross_cache);
    init_stitch_layer_cache(&state->stitch_cache);
    state->ui_scale = 1.0;
    (void)init_ui_font(&state->ui_font, UI_FONT_BASE_PIXEL_SIZE);
    state->grid.cross_pitch_units = DEFAULT_CROSS_PITCH_UNITS;
    state->grid.cross_passes = 3;
    state->grid.raw_detected_passes = 3;

    state->strip_pattern.width = 5;
    state->strip_pattern.height = 5;
    state->strip_pattern.repeat_x = 1;
    state->strip_pattern.repeat_y = 6;
    state->strip_pattern.pitch_units = DEFAULT_CROSS_PITCH_UNITS;
    state->strip_pattern.passes = 3;
    state->strip_active_color = 10U;

    state->active_tool = TOOL_SELECT_ELEMENT;
    state->view_mode = VIEW_MODE_THREAD;
    state->thread_thickness_index = 1U;
    (void)init_block_dedup_table(&state->block_dedup);
    state->show_grid = true;
    state->show_jumps = false;
    state->show_locks = false;
    state->hoop_preset_index = 4U;
    state->simulator_percent = 100.0;
    state->camera.pixels_per_unit = 0.5;
    state->hover_seed_chain = SIZE_MAX;
    state->hover_component_id = SIZE_MAX;
    state->hovered_palette_index = -1;
    state->needs_redraw = true;
    set_status_message(state, "Ready | Open (Ctrl+O), Strip, Tab=Panel, or Drag & Drop");
}

void free_editor_state(EditorState *state) {
    free_ui_font(&state->ui_font);
    free_stitch_layer_cache(&state->stitch_cache);
    cross_cluster_cache_free(&state->cross_cache);
    spatial_index_free(&state->spatial_index);
    scratch_arena_free(&state->scratch);
    free_block_dedup_table(&state->block_dedup);
    clear_segment_selection(state);
    clear_hover_preview(state);
    clear_floating_fragment(state);
    clear_live_preview(state);
    history_clear(&state->history);
    free_stitch_design(&state->design);
}
