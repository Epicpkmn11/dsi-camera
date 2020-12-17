#ifndef I2C_HANDLER_H
#define I2C_HANDLER_H

enum {
	CAM_INIT        = 0,
	CAM0_ACTIVATE   = 1,
	CAM0_DEACTIVATE = 2,
	CAM1_ACTIVATE   = 3,
	CAM1_DEACTIVATE = 4,
};

void checkFifo();

#endif
