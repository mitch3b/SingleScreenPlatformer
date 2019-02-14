#include "Graphics/level1.h"

const unsigned char* LEVELS[] = {level1};

const unsigned char PALETTE[]={
  0x0F, 0x2A, 0x09, 0x07, 0x0F, 0x16, 0x05, 0x05, 0x0F, 0x06, 0x16, 0x26, 0x0F, 0x09, 0x2C, 0x03,
  0x0F, 0x2A, 0x09, 0x07, 0x0F, 0x16, 0x05, 0x05, 0x0F, 0x06, 0x16, 0x26, 0x0F, 0x09, 0x2C, 0x03
};

void loadPalette (void) {
	PPU_ADDRESS = 0x3f;
	PPU_ADDRESS = 0x00;
	PPU_ADDRESS = 0x00;
	for( index = 0; index < sizeof(PALETTE); ++index ){
		PPU_DATA = PALETTE[index];
	}
}

void loadLevel(void) {
	PPU_ADDRESS = 0x20; // address of nametable #0 = 0x2000
	PPU_ADDRESS = 0x00;
	UnRLE(LEVELS[levelNum]);	// uncompresses our data

  //loadCollisionFromNametables();
}
/*
//Would be better to do in asm (like in UnCollision) but haven't figured out a good way yet
void loadCollisionFromNametables(void)
{
  PPU_ADDRESS = 0x20; // address of nametable #0
  PPU_ADDRESS = 0x00;

  //First read is always invalid
  tempInt = *((unsigned char*)0x2007);

  for(tempInt = 0 ; tempInt < 960 ; tempInt++) {
    collision[tempInt] = tempInt;//(*((unsigned char*)0x2007) == 0x00) ? 0x00 : 0x01;
  }
}
*/
