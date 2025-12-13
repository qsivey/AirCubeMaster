/** ____________________________________________________________________
 *
 * 	AirCube project
 *
 *	GitHub:		qsivey, Nik125Y
 *	Telegram:	@qsivey, @Nik125Y
 *	Email:		qsivey@gmail.com, topnikm@gmail.com
 *	____________________________________________________________________
 */

#include "common.h"


ui8 VolumeGammaConvert (ui8 vol)
{
    const ui8  VOL_MAX = 0x7F;
    const uint16_t REG_MAX = 0xFF; // минимум громкости
    const uint16_t REG_MIN = 0x30; // максимум громкости
    const float    GAMMA   = 0.55f; // < 1.0 => громче на малых vol (попробуйте 0.45..0.70)

    if (vol > VOL_MAX) vol = VOL_MAX;

    float x = (float)vol / (float)VOL_MAX;   // 0..1
    float y = powf(x, GAMMA);                // 0..1 (кривая)
    float reg_f = (float)REG_MAX - (float)(REG_MAX - REG_MIN) * y;

    int reg = (int)(reg_f + 0.5f);           // округление
    if (reg < (int)REG_MIN) reg = REG_MIN;
    if (reg > (int)REG_MAX) reg = REG_MAX;

    return (ui8)reg;
}


void BasketBufferWrite (ui8 *buf, size_t size, volatile size_t *wr, const ui8 *src, size_t n)
{
    const size_t mask = size - 1;
    size_t p = (*wr) & mask;

    size_t first = size - p;
    if (first > n) first = n;

    memcpy(buf + p, src, first);
    if (n > first) memcpy(buf, src + first, n - first);

    *wr = (*wr + n) & mask;
}


void BasketBufferRead (const ui8 *buf, size_t size, volatile size_t *tail, ui8 *dst, size_t n)
{
    const size_t mask = size - 1;
    size_t p = (*tail) & mask;

    size_t first = size - p;
    if (first > n) first = n;

    memcpy(dst, buf + p, first);

    if (n > first)
        memcpy(dst + first, buf, n - first);

    *tail = (*tail + n) & mask;
}
