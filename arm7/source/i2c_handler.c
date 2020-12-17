#include "i2c_handler.h"

#include "aptina.h"
#include "aptina_i2c.h"

#include <nds.h>

void checkFifo() {
	if(fifoCheckValue32(FIFO_USER_01)) {
		switch(fifoGetValue32(FIFO_USER_01)) {
			case CAM_INIT:
				init(I2C_CAM0);
				init(I2C_CAM1);
				fifoSendValue32(FIFO_USER_02, aptReadRegister(I2C_CAM0, 0));
				break;
			case CAM0_ACTIVATE:
				activate(I2C_CAM0);
				fifoSendValue32(FIFO_USER_02, 1);
				break;
			case CAM0_DEACTIVATE:
				deactivate(I2C_CAM0);
				fifoSendValue32(FIFO_USER_02, 2);
				break;
			case CAM1_ACTIVATE:
				activate(I2C_CAM1);
				fifoSendValue32(FIFO_USER_02, 3);
				break;
			case CAM1_DEACTIVATE:
				deactivate(I2C_CAM1);
				fifoSendValue32(FIFO_USER_02, 4);
				break;
			default:
				break;
		}
	}
}
