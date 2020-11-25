#include "camera.h"
#include "lodepng.h"
#include "quirc.h"
#include "version.h"

#include <dirent.h>
#include <fat.h>
#include <math.h>
#include <nds.h>
#include <stdio.h>

#define YUV_TO_R(Y, Cr) clamp(Y + Cr + (Cr >> 2) + (Cr >> 3) + (Cr >> 5), 0, 0xFF)
#define YUV_TO_G(Y, Cb, Cr) \
	clamp(Y - ((Cb >> 2) + (Cb >> 4) + (Cb >> 5)) - ((Cr >> 1) + (Cr >> 3) + (Cr >> 4) + (Cr >> 5)), 0, 0xFF)
#define YUV_TO_B(Y, Cb) clamp(Y + Cb + (Cb >> 1) + (Cb >> 2) + (Cb >> 6), 0, 0xFF)

int clamp(int val, int min, int max) { return val < min ? min : (val > max) ? max : val; }

int getImageNumber() {
	int highest = -1;

	DIR *pdir = opendir("/DCIM/100DSITEST");
	if(pdir == NULL) {
		printf("Unable to open directory");
		return -1;
	} else {
		while(true) {
			struct dirent *pent = readdir(pdir);
			if(pent == NULL)
				break;

			if(strncmp(pent->d_name, "IMG_", 4) == 0) {
				int val = atoi(pent->d_name + 4);
				if(val > highest)
					highest = val;
			}
		}
		closedir(pdir);
	}

	return highest + 1;
}

int main(int argc, char **argv) {
	consoleDemoInit();
	vramSetBankA(VRAM_A_MAIN_BG);
	videoSetMode(MODE_5_2D);
	int bg3Main = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 1, 0);

	printf("dsi-camera " VER_NUMBER "\n");

	bool fatInited = fatInitDefault();
	if(fatInited) {
		mkdir("/DCIM", 0777);
		mkdir("/DCIM/100DSITEST", 0777);
	} else {
		printf("FAT init failed, photos cannot\nbe saved.\n");
	}

	printf("Initializing...\n");
	cameraInit();

	Camera camera = CAM_OUTER;
	cameraActivate(camera);

	if(fatInited)
		printf("\nA to swap, L/R to take picture,\n");
	else
		printf("\nA to swap\n");
	printf("Y to scan QR\n");

	while(1) {
		u16 pressed;
		do {
			swiWaitForVBlank();
			if(!cameraTransferActive())
				cameraTransferStart(bgGetGfxPtr(bg3Main), CAPTURE_MODE_PREVIEW);
			scanKeys();
			pressed = keysDown();
		} while(!pressed);

		if(pressed & KEY_A) {
			// Wait for previous transfer to finish
			while(cameraTransferActive())
				swiWaitForVBlank();
			cameraTransferStop();

			// Switch camera
			camera = camera == CAM_INNER ? CAM_OUTER : CAM_INNER;
			cameraActivate(camera);

			printf("Swapped to %s camera\n", camera == CAM_INNER ? "inner" : "outer");
		} else if(fatInited && pressed & (KEY_L | KEY_R)) {
			printf("Capturing... ");

			// Wait for previous transfer to finish
			while(cameraTransferActive())
				swiWaitForVBlank();

			// Get image
			u16 *yuv = (u16 *)malloc(640 * 480 * sizeof(u16));
			cameraTransferStart(yuv, CAPTURE_MODE_CAPTURE);
			while(cameraTransferActive())
				swiWaitForVBlank();
			cameraTransferStop();

			printf("Done!\nSaving PNG... ");

			// YUV422 -> RGB
			u8 *rgb = (u8 *)malloc(640 * 480 * 3);
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
			free(yuv);

			char imgName[32];
			sprintf(imgName, "/DCIM/100DSITEST/IMG_%04d.PNG", getImageNumber());
			lodepng_encode24_file(imgName, rgb, 640, 480);
			free(rgb);

			printf("Done!\nSaved to:\n%s\n\n", imgName);
		} else if(pressed & KEY_Y) {
			printf("Scanning for QR... ");

			// Wait for previous transfer to finish
			while(cameraTransferActive())
				swiWaitForVBlank();

			// Get image
			u16 *buffer = (u16 *)malloc(640 * 480 * sizeof(u16));
			cameraTransferStart(buffer, CAPTURE_MODE_CAPTURE);
			while(cameraTransferActive())
				swiWaitForVBlank();
			cameraTransferStop();

			struct quirc *q = quirc_new();
			quirc_resize(q, 640, 480);
			uint8_t *qrbuffer = quirc_begin(q, NULL, NULL);
			// Copy Y values to qrbuffer
			for(int i = 0; i < 640 * 480; i++) {
				qrbuffer[i] = buffer[i] & 0xFF;
			}
			quirc_end(q);

			if(quirc_count(q) > 0) {
				struct quirc_code code;
				struct quirc_data data;
				quirc_extract(q, 0, &code);
				if(!quirc_decode(&code, &data)) {
					printf("\n%s\n", data.payload);
				}
			}

			quirc_destroy(q);
			free(buffer);
			printf("Done!\n");
		} else if(pressed & KEY_START) {
			// Disable camera so the light turns off
			cameraDeactivate(camera);

			return 0;
		}
	}
}
