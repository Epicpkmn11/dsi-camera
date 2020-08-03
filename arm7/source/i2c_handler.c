#include "i2c_handler.h"

#include "aptina.h"
#include "aptina_i2c.h"

#include <nds.h>

void checkFifo() {
	if(fifoCheckValue32(FIFO_USER_01)) {
		switch(fifoGetValue32(FIFO_USER_01)) {
			case 0:
				init(I2C_CAM0);
				init(I2C_CAM1);
				fifoSendValue32(FIFO_USER_02, aptReadRegister(I2C_CAM0, 0));
				break;
			case 1:
				activate(I2C_CAM0);
				fifoSendValue32(FIFO_USER_02, 1);
				break;
			case 2:
				deactivate(I2C_CAM0);
				fifoSendValue32(FIFO_USER_02, 2);
				break;
			case 3:
				activate(I2C_CAM1);
				fifoSendValue32(FIFO_USER_02, 3);
				break;
			case 4:
				deactivate(I2C_CAM1);
				fifoSendValue32(FIFO_USER_02, 4);
				break;
			default:
				break;
		}
	}
}
