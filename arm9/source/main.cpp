#include "camera.h"
#include "lodepng.h"

#include <algorithm>
#include <fat.h>
#include <nds.h>

#define YUV_TO_R(Y, Cr) std::clamp(Y + Cr + (Cr >> 2) + (Cr >> 3) + (Cr >> 5), 0, 0xFF)
#define YUV_TO_G(Y, Cb, Cr) \
	std::clamp(Y - ((Cb >> 2) + (Cb >> 4) + (Cb >> 5)) - ((Cr >> 1) + (Cr >> 3) + (Cr >> 4) + (Cr >> 5)), 0, 0xFF)
#define YUV_TO_B(Y, Cb) std::clamp(Y + Cb + (Cb >> 1) + (Cb >> 2) + (Cb >> 6), 0, 0xFF)

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
	fifoWaitValue32(FIFO_USER_01);
	printf("ID: 0x%lX\n", fifoGetValue32(FIFO_USER_01));

	REG_SCFG_CLK &= ~0x0100; // SCFG_CLK, CamExternal Clock = OFF
	REG_SCFG_CLK |= 0x0100;  // SCFG_CLK, CamExternal Clock = ON
	swiDelay(0x14);

	fifoSendValue32(FIFO_USER_01, CAM1_ACTIVATE); // issue "aptina_code_list_activate" via I2C bus on ARM7 side
	fifoWaitValue32(FIFO_USER_01);
	printf("Active: 0x%lX\n", fifoGetValue32(FIFO_USER_01));

	fifoSendValue32(FIFO_USER_01, CAM_MODE_PREVIEW);
	fifoWaitValue32(FIFO_USER_01);
	printf("Mode: 0x%lX\n", fifoGetValue32(FIFO_USER_01));

	REG_CAM_CNT |= 0x2000; // CAM_CNT, enable YUV-to-RGB555
	REG_CAM_CNT = (REG_CAM_CNT & ~0x000F) | 0x0003;
	REG_CAM_CNT |= 0x0020;                     // CAM_CNT, flush data fifo
	REG_CAM_CNT |= 0x8000;                     // CAM_CNT, start transfer
	REG_NDMA1SAD  = 0x04004204;                // NDMA1SAD, source CAM_DTA
	REG_NDMA1DAD  = (u32)bgGetGfxPtr(bg3Main); // NDMA1DAD, dest RAM/VRAM
	REG_NDMA1TCNT = 256 * 192 / 2;             // NDMA1TCNT, len for 256x192 total
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
			fifoWaitValue32(FIFO_USER_01);
			printf("Disable: 0x%lX\n", fifoGetValue32(FIFO_USER_01));

			fifoSendValue32(FIFO_USER_01, inner ? CAM1_ACTIVATE : CAM0_ACTIVATE);
			fifoWaitValue32(FIFO_USER_01);
			printf("Enable: 0x%lX\n", fifoGetValue32(FIFO_USER_01));

			// Set data transfer back up
			REG_CAM_CNT |= 0x2000; // CAM_CNT, enable YUV-to-RGB555
			REG_CAM_CNT = (REG_CAM_CNT & ~0x000F) | 0x0003;
			REG_CAM_CNT |= 0x0020; // CAM_CNT, flush data fifo
			REG_CAM_CNT |= 0x8000; // CAM_CNT, start transfer

			// Resume transfer
			REG_CAM_CNT |= 0x8000;

			inner = !inner;
		} else if(fatInited && pressed & (KEY_L | KEY_R)) {
			printf("Capturing... ");

			// Wait for previous transfer to finish
			while(REG_NDMA1CNT & BIT(31))
				swiWaitForVBlank();

			// Set camera to capture mode (640x480)
			REG_CAM_CNT &= ~0x8000; // Pause transfer
			REG_CAM_CNT = 0x0000;   // YUF222 mode and "scanline count - 1" of 0
			fifoSendValue32(FIFO_USER_01, CAM_MODE_CAPTURE);
			fifoWaitValue32(FIFO_USER_01);
			fifoGetValue32(FIFO_USER_01);

			u16 *yuv = new u16[640 * 480];
			u8 *rgb  = new u8[640 * 480 * 3];

			// Get frame
			REG_CAM_CNT |= BIT(15);        // Start transfer
			REG_NDMA1DAD  = (u32)yuv;      // NDMA1DAD, dest RAM/VRAM
			REG_NDMA1TCNT = 640 * 480 / 2; // NDMA1TCNT, len for 256x192 total
			REG_NDMA1WCNT = 320;           // NDMA1WCNT, len for 256x4 blocks
			REG_NDMA1CNT  = 0x8B044000;    // NDMA1CNT, start camera DMA
			while(REG_NDMA1CNT & BIT(31))
				swiWaitForVBlank();

			REG_CAM_CNT &= ~0x8000; // Pause transfer

			printf("Done!\nSaving PNG... ");

			// YUV422 -> RGB
			for(int py = 0; py < 480; py++) {
				for(int px = 0; px < 640; px += 2) {
					u8 *val = (u8 *)(yuv + py * 640 + px);

					// Get YUV values
					int Y1 = val[0];
					int Cb = val[1] - 0x80;
					int Y2 = val[2];
					int Cr = val[3] - 0x80;

					u8 *dst = rgb + py * (640 * 3) + px * 3;
					// First pixel R, G, B
					dst[0] = YUV_TO_R(Y1, Cr);
					dst[1] = YUV_TO_G(Y1, Cb, Cr);
					dst[2] = YUV_TO_B(Y1, Cb);
					// Second pixel R, G, B
					dst[3] = YUV_TO_R(Y2, Cr);
					dst[4] = YUV_TO_G(Y2, Cb, Cr);
					dst[5] = YUV_TO_B(Y2, Cb);
				}
			}
			delete[] yuv;

			lodepng_encode24_file("dsi-camera.png", rgb, 640, 480);
			delete[] rgb;

			printf("Done!\n");

			// Set NDMA back to 256x192 to the screen
			REG_CAM_CNT   = 0x2003;                    // BGR mode, "scanline count - 1" of 3
			REG_NDMA1DAD  = (u32)bgGetGfxPtr(bg3Main); // NDMA1DAD, dest RAM/VRAM
			REG_NDMA1TCNT = 256 * 192 / 2;             // NDMA1TCNT, len for 256x192 total
			REG_NDMA1WCNT = 0x00000200;                // NDMA1WCNT, len for 256x4 blocks
			REG_NDMA1CNT  = 0x8B044000;                // NDMA1CNT, start camera DMA

			// Set camera back to preview mode (256x192)
			fifoSendValue32(FIFO_USER_01, CAM_MODE_PREVIEW);
			fifoWaitValue32(FIFO_USER_01);
			fifoGetValue32(FIFO_USER_01);

			REG_CAM_CNT |= 0x2000; // Switch back to BGR conversion mode
			REG_CAM_CNT |= 0x8000; // Resume transfer
		} else if(pressed & KEY_START) {
			// Disable camera so the light turns off
			fifoSendValue32(FIFO_USER_01, inner ? 2 : 4);
			fifoWaitValue32(FIFO_USER_01);
			fifoGetValue32(FIFO_USER_01);

			return 0;
		}
	}
}
