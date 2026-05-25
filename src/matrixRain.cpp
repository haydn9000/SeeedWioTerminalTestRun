// matrixRain.cpp — Full-screen Matrix digital rain screensaver.
//
// 40 columns × 30 rows at 8×8 px. Each column has a falling green
// character stream with a bright head and fading trail.
//
// Exit: press any button or move the joystick in any direction.

#include <Arduino.h>
#include "globals.h"

#define MTX_COLS   40
#define MTX_ROWS   30
#define MTX_CW      8    // cell width  (px) — slightly wider than 6px font for spacing
#define MTX_CH      8    // cell height (px)

static const char MTX_CHARS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$&?";
static const int  MTX_NCHARS  = (int)(sizeof(MTX_CHARS) - 1);

// Per-column state
static int8_t  mtxHead[MTX_COLS];             // current head row (-ROWS..ROWS)
static uint8_t mtxTrail[MTX_COLS];            // trail length in rows
static uint8_t mtxSpeed[MTX_COLS];            // frames between steps
static uint8_t mtxTick[MTX_COLS];             // frame counter
static uint8_t mtxChars[MTX_COLS][MTX_ROWS];  // stored char index at each cell

// Colour palette — set at runtime (can't call color565 at static init)
static uint16_t CHead, CBright, CMid, CDim, CDark;

// -------------------------------------------------------------------------
static void mtxDrawCell(int col, int row, uint16_t color)
{
    if (row < 0 || row >= MTX_ROWS) return;
    tft.drawChar(col * MTX_CW, row * MTX_CH,
                 (uint8_t)MTX_CHARS[mtxChars[col][row]], color, TFT_BLACK, 1);
}

static void mtxClearCell(int col, int row)
{
    if (row < 0 || row >= MTX_ROWS) return;
    tft.fillRect(col * MTX_CW, row * MTX_CH, MTX_CW, MTX_CH, TFT_BLACK);
}

// -------------------------------------------------------------------------
static void mtxResetCol(int col)
{
    mtxHead[col]  = -(int8_t)random(3, 25);
    mtxTrail[col] = (uint8_t)random(5, 16);
    mtxSpeed[col] = (uint8_t)random(1, 4);
    mtxTick[col]  = 0;
}

static void mtxInit()
{
    for (int c = 0; c < MTX_COLS; c++)
    {
        mtxResetCol(c);
        for (int r = 0; r < MTX_ROWS; r++)
            mtxChars[c][r] = (uint8_t)random(MTX_NCHARS);
    }
}

// -------------------------------------------------------------------------
// Advance one column by one row.
static void mtxStep(int col)
{
    int head = (int)mtxHead[col] + 1;
    mtxHead[col] = (int8_t)head;

    // Assign a new random character to the head cell
    if (head >= 0 && head < MTX_ROWS)
        mtxChars[col][head] = (uint8_t)random(MTX_NCHARS);

    // Draw gradient from head down (head = brightest, trail fades)
    mtxDrawCell(col, head,     CHead);
    mtxDrawCell(col, head - 1, CBright);
    mtxDrawCell(col, head - 2, CMid);
    mtxDrawCell(col, head - 3, CDim);
    mtxDrawCell(col, head - 4, CDark);

    // Randomly flicker a character in the middle of the trail
    if (random(6) == 0)
    {
        int fr = head - 1 - (int)random((int)mtxTrail[col]);
        if (fr >= 0 && fr < MTX_ROWS)
        {
            mtxChars[col][fr] = (uint8_t)random(MTX_NCHARS);
            mtxDrawCell(col, fr, CMid);
        }
    }

    // Erase the cell just behind the tail
    int tail = head - (int)mtxTrail[col];
    mtxClearCell(col, tail);

    // Once the trail has scrolled fully off-screen, reset for next pass
    if (head - (int)mtxTrail[col] >= MTX_ROWS)
        mtxResetCol(col);
}

// -------------------------------------------------------------------------
static bool mtxAnyKey()
{
    return digitalRead(WIO_KEY_A)    == LOW ||
           digitalRead(WIO_KEY_B)    == LOW ||
           digitalRead(WIO_KEY_C)    == LOW ||
           digitalRead(WIO_5S_UP)    == LOW ||
           digitalRead(WIO_5S_DOWN)  == LOW ||
           digitalRead(WIO_5S_LEFT)  == LOW ||
           digitalRead(WIO_5S_RIGHT) == LOW ||
           digitalRead(WIO_5S_PRESS) == LOW;
}

// -------------------------------------------------------------------------
void matrixRainScreen()
{
    // Wait for launch key to be released before sampling for exit
    while (digitalRead(WIO_5S_PRESS) == LOW) delay(10);
    while (mtxAnyKey()) delay(10);

    CHead   = tft.color565(210, 255, 210);
    CBright = tft.color565(80,  255,  80);
    CMid    = tft.color565(0,   190,   0);
    CDim    = tft.color565(0,   110,   0);
    CDark   = tft.color565(0,    55,   0);

    tft.fillScreen(TFT_BLACK);
    mtxInit();

    while (true)
    {
        if (mtxAnyKey()) break;

        for (int c = 0; c < MTX_COLS; c++)
        {
            mtxTick[c]++;
            if (mtxTick[c] >= mtxSpeed[c])
            {
                mtxTick[c] = 0;
                mtxStep(c);
            }
        }

        delay(33);   // ~30 fps
    }

    // Drain all keys before returning so menu doesn't pick them up
    while (mtxAnyKey()) delay(10);
    delay(50);
}
