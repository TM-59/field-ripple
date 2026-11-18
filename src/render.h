#ifndef RENDER_H
#define RENDER_H

#include <stddef.h>

#include "field.h"

#define RENDER_COLS 78
#define RENDER_ROWS 18

/* The field phi in [0, 2 pi] drawn as '*' over a window of 0.25 length units per column centred on
 * `centre`; '|' marks `target`. Returns bytes written or 0 if cap is too small. */
/* The window's centre and the target mark, in x. */
typedef struct {
    double centre, target;
} RenderView;
size_t render_frame(const Field *f, RenderView view, char *buf, size_t cap);

#endif
