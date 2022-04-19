#ifndef FIFOVARS_H
#define FIFOVARS_H

#ifdef __cplusplus
extern "C" {
#endif

#define FIFO_CAMERA FIFO_USER_01

typedef enum {
	CAM_INIT,
	CAM0_ACTIVATE,
	CAM0_DEACTIVATE,
	CAM1_ACTIVATE,
	CAM1_DEACTIVATE,
	CAM_SET_MODE_PREVIEW,
	CAM_SET_MODE_CAPTURE
} FifoCommand;

typedef enum {
	CAPTURE_MODE_PREVIEW = 1, // 256x192
	CAPTURE_MODE_CAPTURE = 2  // 640x480
} CaptureMode;

#ifdef __cplusplus
}
#endif

#endif // FIFOVARS_H
