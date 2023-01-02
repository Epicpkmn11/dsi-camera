#include "camera.h"
#include "version.h"

#include <dirent.h>
#include <fat.h>
#include <nds.h>
#include <stdio.h>

int getVideoNumber() {
	int highest = -1;

	DIR *pdir = opendir("/DCIM/100DSI00");
	if(pdir == NULL) {
		printf("Unable to open directory");
		return -1;
	} else {
		while(true) {
			struct dirent *pent = readdir(pdir);
			if(pent == NULL)
				break;

			if(strncmp(pent->d_name, "VID_", 4) == 0) {
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

	printf("dsi-camcorder " VER_NUMBER "\n");

	bool fatInited = fatInitDefault();
	if(fatInited) {
		mkdir("/DCIM", 0777);
		mkdir("/DCIM/100DSI00", 0777);
	} else {
		printf("FAT init failed!\n");
		do {
			swiWaitForVBlank();
			scanKeys();
		} while(!(keysDown() & KEY_START));
		return 0;
	}

	printf("Initializing...\n");
	cameraInit();

	Camera camera = CAM_OUTER;
	cameraActivate(camera);

	char vidName[32];
	sprintf(vidName, "/DCIM/100DSI00/VID_%04d.BIN", getVideoNumber());
	FILE *out = fopen(vidName, "wb");

	printf("\nIt's recording!\n");
	printf("START to finish\n\n");
	printf("Output file:\n%s\n", vidName);

	cpuStartTiming(0);

	while(1) {
		swiWaitForVBlank();

		cameraTransferStart(bgGetGfxPtr(bg3Main), CAPTURE_MODE_PREVIEW);
		while(cameraTransferActive())
			swiDelay(100);

		if(fwrite(bgGetGfxPtr(bg3Main), 1, 256 * 192 * 2, out) != 256 * 192 * 2) {
			cameraDeactivate(camera);
			fclose(out);
			return 0;
		}

		u32 time = cpuGetTiming();
		fwrite(&time, 4, 1, out);
		cpuStartTiming(0);

		scanKeys();
		if(keysDown() & KEY_START) {
			// Disable camera so the light turns off
			cameraDeactivate(camera);
			fclose(out);

			return 0;
		}
	}
}
