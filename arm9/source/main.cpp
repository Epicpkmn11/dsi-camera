#include <nds.h>
#include <stdio.h>

int main(int argc, char **argv) {
	consoleDemoInit();
	vramSetBankA(VRAM_A_MAIN_BG);
	videoSetMode(MODE_5_2D);
	int bg3Main = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 1, 0);

	printf("Initializing...\n");
	// https://problemkaputt.de/gbatek.htm#dsicameras
	*(u16 *)0x4004004 |= 0x0004; // SCFG_CLK, CamInterfaceClock = ON
	*(u16 *)0x4004200 = 0x0000;  // CAM_MCNT, Camera Module Control
	swiDelay(0x1E);
	*(u16 *)0x4004004 |= 0x0100; // SCFG_CLK, CamExternal Clock = ON
	swiDelay(0x1E);
	*(u16 *)0x4004200 = 0x0022; // CAM_MCNT, Camera Module Control
	swiDelay(0x2008);
	*(u16 *)0x4004004 &= ~0x0100; // SCFG_CLK, CamExternal Clock = OFF
	*(u16 *)0x4004202 &= ~0x8000; // CAM_CNT, allow changing params
	*(u16 *)0x4004202 |= 0x0020;  // CAM_CNT, flush data fifo
	*(u16 *)0x4004202 = (*(u16 *)0x4004202 & ~0x0300) | 0x0200;
	*(u16 *)0x4004202 |= 0x0400;
	*(u16 *)0x4004202 |= 0x0800; // CAM_CNT, irq enable
	*(u16 *)0x4004004 |= 0x0100; // SCFG_CLK, CamExternal Clock = ON
	swiDelay(0x14);

	fifoSendValue32(FIFO_USER_01, 0); // issue "aptina_code_list_init" via I2C bus on ARM7 side
	while(!fifoCheckValue32(FIFO_USER_02))
		swiWaitForVBlank();
	printf("ID: 0x%lx\n", fifoGetValue32(FIFO_USER_02));

	*(u16 *)0x4004004 &= ~0x0100; // SCFG_CLK, CamExternal Clock = OFF
	*(u16 *)0x4004004 |= 0x0100;  // SCFG_CLK, CamExternal Clock = ON
	swiDelay(0x14);

	fifoSendValue32(FIFO_USER_01, 3); // issue "aptina_code_list_activate" via I2C bus on ARM7 side
	while(!fifoCheckValue32(FIFO_USER_02))
		swiWaitForVBlank();
	printf("Active: 0x%lx\n", fifoGetValue32(FIFO_USER_02));

	*(u16 *)0x4004202 |= 0x2000; // CAM_CNT, enable YUV-to-RGB555
	*(u16 *)0x4004202 = (*(u16 *)0x4004202 & ~0x000F) | 0x0003;
	*(u16 *)0x4004202 |= 0x0020;                   // CAM_CNT, flush data fifo
	*(u16 *)0x4004202 |= 0x8000;                   // CAM_CNT, start transfer
	*(u32 *)0x4004120 = 0x04004204;                // NDMA1SAD, source CAM_DTA
	*(u32 *)0x4004124 = (u32)bgGetGfxPtr(bg3Main); // NDMA1DAD, dest RAM/VRAM
	*(u32 *)0x4004128 = 0x00006000;                // NDMA1TCNT, len for 256x192 total
	*(u32 *)0x400412C = 0x00000200;                // NDMA1WCNT, len for 256x4 blocks
	*(u32 *)0x4004130 = 0x00000002;                // NDMA1BCNT, timing interval or so
	*(u32 *)0x4004138 = 0x8B044000;                // NDMA1CNT, start camera DMA

	bool inner = false;
	while(1) {
		// dmaFillHalfWords(0xfc1f, bgGetGfxPtr(bg3Main), 256 * 192 * 2);
		swiWaitForVBlank();
		scanKeys();
		if(keysDown() & KEY_A) {
			*(u16 *)0x4004202 &= ~0x8000;
			if(inner) {
				fifoSendValue32(FIFO_USER_01, 2);
				while(!fifoCheckValue32(FIFO_USER_02))
					swiWaitForVBlank();
				printf("Disable: 0x%lx\n", fifoGetValue32(FIFO_USER_02));

				fifoSendValue32(FIFO_USER_01, 3);
				while(!fifoCheckValue32(FIFO_USER_02))
					swiWaitForVBlank();
				printf("Enable: 0x%lx\n", fifoGetValue32(FIFO_USER_02));
			} else {
				fifoSendValue32(FIFO_USER_01, 4);
				while(!fifoCheckValue32(FIFO_USER_02))
					swiWaitForVBlank();
				printf("Disable: 0x%lx\n", fifoGetValue32(FIFO_USER_02));

				fifoSendValue32(FIFO_USER_01, 1);
				while(!fifoCheckValue32(FIFO_USER_02))
					swiWaitForVBlank();
				printf("Enable: 0x%lx\n", fifoGetValue32(FIFO_USER_02));
			}
			inner = !inner;
			*(u16 *)0x4004202 |= ~0x8000;
		} else if(keysDown() & KEY_START) {
			return 0;
		}
	}
}
