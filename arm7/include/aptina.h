#ifndef APTINA_H
#define APTINA_H

#include <nds.h>

typedef enum {
	CAPTURE_MODE_PREVIEW = 1, // 256x192
	CAPTURE_MODE_CAPTURE = 2  // 640x480
} captureMode;

void init(u8 device);
void activate(u8 device);
void deactivate(u8 device);
void setMode(captureMode mode);

#endif
