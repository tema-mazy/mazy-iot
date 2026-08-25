#pragma once

#include "dashdata.h"

/* Number of screens in the rotation for the current snapshot. */
int screens_count(const dash_snapshot_t *s);

/* Render screen `index` into the e-paper framebuffer. Does not refresh. */
void screens_draw(const dash_snapshot_t *s, int index);
