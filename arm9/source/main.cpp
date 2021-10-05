#include "camera.h"

#include <fat.h>
#include <nds.h>
#include <stdio.h>

int main(int argc, char **argv) {
	consoleDemoInit();
	vramSetBankA(VRAM_A_MAIN_BG);
	videoSetMode(MODE_5_2D);
	int bg3Main = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 1, 0);

	bool fatInited = fatInitDefault();
	if(!fatInited)
		printf("fatInitDefault() failed, photos\ncannot be saved.\n");

	printf("Initializing...\n");
	// https://problemkaputt.de/gbatek.htm#dsicameras

	REG_SCFG_CLK |= 0x0004; // SCFG_CLK, CamInterfaceClock = ON
	REG_CAM_MCNT = 0x0000;  // CAM_MCNT, Camera Module Control
	swiDelay(0x1E);
	REG_SCFG_CLK |= 0x0100; // SCFG_CLK, CamExternal Clock = ON
	swiDelay(0x1E);
	REG_CAM_MCNT = 0x0022; // CAM_MCNT, Camera Module Control
	swiDelay(0x2008);
	REG_SCFG_CLK &= ~0x0100; // SCFG_CLK, CamExternal Clock = OFF
	REG_CAM_CNT &= ~0x8000;  // CAM_CNT, allow changing params
	REG_CAM_CNT |= 0x0020;   // CAM_CNT, flush data fifo
	REG_CAM_CNT = (REG_CAM_CNT & ~0x0300) | 0x0200;
	REG_CAM_CNT |= 0x0400;
	REG_CAM_CNT |= 0x0800;  // CAM_CNT, irq enable
	REG_SCFG_CLK |= 0x0100; // SCFG_CLK, CamExternal Clock = ON
	swiDelay(0x14);

	fifoSendValue32(FIFO_USER_01, CAM_INIT); // issue "aptina_code_list_init" via I2C bus on ARM7 side
	while(!fifoCheckValue32(FIFO_USER_02))
		swiWaitForVBlank();
	printf("ID: 0x%lx\n", fifoGetValue32(FIFO_USER_02));

	REG_SCFG_CLK &= ~0x0100; // SCFG_CLK, CamExternal Clock = OFF
	REG_SCFG_CLK |= 0x0100;  // SCFG_CLK, CamExternal Clock = ON
	swiDelay(0x14);

	fifoSendValue32(FIFO_USER_01, CAM1_ACTIVATE); // issue "aptina_code_list_activate" via I2C bus on ARM7 side
	while(!fifoCheckValue32(FIFO_USER_02))
		swiWaitForVBlank();
	printf("Active: 0x%lx\n", fifoGetValue32(FIFO_USER_02));

	REG_CAM_CNT |= 0x2000; // CAM_CNT, enable YUV-to-RGB555
	REG_CAM_CNT = (REG_CAM_CNT & ~0x000F) | 0x0003;
	REG_CAM_CNT |= 0x0020;                     // CAM_CNT, flush data fifo
	REG_CAM_CNT |= 0x8000;                     // CAM_CNT, start transfer
	REG_NDMA1SAD  = 0x04004204;                // NDMA1SAD, source CAM_DTA
	REG_NDMA1DAD  = (u32)bgGetGfxPtr(bg3Main); // NDMA1DAD, dest RAM/VRAM
	REG_NDMA1TCNT = 0x00006000;                // NDMA1TCNT, len for 256x192 total
	REG_NDMA1WCNT = 0x00000200;                // NDMA1WCNT, len for 256x4 blocks
	REG_NDMA1BCNT = 0x00000002;                // NDMA1BCNT, timing interval or so
	REG_NDMA1CNT  = 0x8B044000;                // NDMA1CNT, start camera DMA

	printf("\nA to swap, L/R to take picture\n");

	bool inner  = false;
	u16 pressed = 0;
	while(1) {
		do {
			swiWaitForVBlank();
			if(!(REG_NDMA1CNT & BIT(31)))
				REG_NDMA1CNT = 0x8B044000; // NDMA1CNT, start camera DMA
			scanKeys();
			pressed = keysDown();
		} while(!pressed);

		if(pressed & KEY_A) {
			// Wait for previous transfer to finish
			while(REG_NDMA1CNT & BIT(31))
				swiWaitForVBlank();

			// Pause transfer
			REG_CAM_CNT &= ~0x8000;

			// Tell ARM7 to switch cameras
			fifoSendValue32(FIFO_USER_01, inner ? CAM0_DEACTIVATE : CAM1_DEACTIVATE);
			while(!fifoCheckValue32(FIFO_USER_02))
				swiWaitForVBlank();
			printf("Disable: 0x%lx\n", fifoGetValue32(FIFO_USER_02));

			fifoSendValue32(FIFO_USER_01, inner ? CAM1_ACTIVATE : CAM0_ACTIVATE);
			while(!fifoCheckValue32(FIFO_USER_02))
				swiWaitForVBlank();
			printf("Enable: 0x%lx\n", fifoGetValue32(FIFO_USER_02));

			// Set data transfer back up
			REG_CAM_CNT |= 0x2000; // CAM_CNT, enable YUV-to-RGB555
			REG_CAM_CNT = (REG_CAM_CNT & ~0x000F) | 0x0003;
			REG_CAM_CNT |= 0x0020; // CAM_CNT, flush data fifo
			REG_CAM_CNT |= 0x8000; // CAM_CNT, start transfer

			// Resume transfer
			REG_CAM_CNT |= 0x8000;

			inner = !inner;
		} else if(fatInited && pressed & (KEY_L | KEY_R)) {
			FILE *file = fopen("photo.bmp", "wb");
			if(file) {
				printf("Saving BMP... ");

				// Wait for previous transfer to finish
				while(REG_NDMA1CNT & BIT(31))
					swiWaitForVBlank();

				// Get frame
				u16 *buffer  = new u16[256 * 192];
				REG_NDMA1DAD = (u32)buffer; // NDMA1DAD, dest RAM/VRAM
				REG_NDMA1CNT = 0x8B044000;  // NDMA1CNT, start camera DMA
				while(REG_NDMA1CNT & BIT(31))
					swiWaitForVBlank();

				// Pause transfer
				REG_CAM_CNT &= ~0x8000;

				// BGR -> RGB
				for(int i = 0; i < 256 * 192; i++) {
					buffer[i] = ((buffer[i] >> 10) & 0x1F) | (buffer[i] & (0x1F << 5)) | (buffer[i] & 0x1F) << 10;
				}

				// Header for a 256x192 16 bit BMP
				constexpr u8 bmpHeader[] = {0x42, 0x4D, 0x46, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x00,
											0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xC0, 0x00,
											0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x80,
											0x01, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x00, 0x00,
											0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0xE0, 0x03,
											0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

				fwrite(bmpHeader, 1, sizeof(bmpHeader), file);

				// Write image data, upside down as that's how BMPs want it
				for(int i = 191; i >= 0; i--) {
					fwrite(buffer + (i * 256), 2, 256, file);
				}

				delete[] buffer;
				fclose(file);
				printf("Done!\n");

				// Set NDMA back to screen
				REG_NDMA1DAD = (u32)bgGetGfxPtr(bg3Main); // NDMA1DAD, dest RAM/VRAM

				// Resume transfer
				REG_CAM_CNT |= 0x8000;
			}
		} else if(pressed & KEY_START) {
			// Disable camera so the light turns off
			fifoSendValue32(FIFO_USER_01, inner ? 2 : 4);
			while(!fifoCheckValue32(FIFO_USER_02))
				swiWaitForVBlank();

			return 0;
		}
	}
}
