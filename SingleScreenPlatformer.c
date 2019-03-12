#include "DEFINE.c"
#include "LevelData.c"

// Physics
#define GRAVITY 1
#define JUMP_VELOCITY -8
#define VELOCITY_FACTOR 4 //No decimals so only use 1/VELOCITY_FACTOR'th of the velocity
#define MAX_JUMP_COUNT 10 //Number of frames you can hold jump and keep traveling up
#define MAX_VELOCITY_X 1
#define MAX_VELOCITY_WITH_B_X 2

#define ENEMY_MOVING_LEFT 0
#define ENEMY_MOVING_RIGHT 1
#define ENEMY_MOVE_EVERY_X_FRAMES 3
#define ENEMY_TURN_AROUND_TIME 60

#define WALKER_WALKING 0
#define WALKER_EXPLODING_START 1
#define WALKER_EXPLODING_END 5
#define WALKER_DEAD 6

#define DROP_OFFSET_X 2
#define DROP_OFFSET_Y 0
#define DROP_HEIGHT 4
#define DROP_WIDTH 3

#define DROP_WAITING_TO_DROP 0
#define DROP_DROPPING 1
#define DROP_ON_GROUND 2
#define DROP_MAX_WAIT_TO_DROP 60

#pragma bss-name (push, "OAM")
unsigned char SPRITES[256];
#pragma bss-name (pop)

#pragma bss-name (push, "ZEROPAGE")
unsigned char x1;
unsigned char y1;
unsigned char x2;
unsigned char y2;
unsigned char width1;
unsigned char height1;
unsigned char width2;
unsigned char height2;

// Character vars
unsigned char spawnX;
unsigned char spawnY;
signed char yVelocity;
unsigned char jumpCount;
unsigned char isFalling;
signed char isWalking;
unsigned char isExploding;

#define ENEMY_ID_WALKER 1
#define ENEMY_ID_DROP 2

//Enemy info
typedef struct {
  unsigned char id;
  unsigned char startX;
  unsigned char startY;
  unsigned char enemyState;
  unsigned char enemyTimer;
  unsigned char state; // 0 is alive, 6 is fully dead, in between is for explode animation TODO make this part of enemyState
} enemy_struct;

#define NUM_ENEMIES 6
enemy_struct enemies[NUM_ENEMIES] = {
  {ENEMY_ID_WALKER, 0x98, 0xA0, 0x00, 0x00, 0x00},
  {ENEMY_ID_DROP,   0x58, 0x48, 0x00, 0x00, 0x00},
  {ENEMY_ID_WALKER, 0x10, 0x70, 0x00, 0x00, 0x00},
  {ENEMY_ID_WALKER, 0x30, 0xD8, 0x00, 0x00, 0x00},
  {ENEMY_ID_WALKER, 0x38, 0xD8, 0x00, 0x00, 0x00},
  {ENEMY_ID_WALKER, 0x48, 0xD8, 0x00, 0x00, 0x00},
};

//POWERUPS
unsigned char powerUpState;

//Shots
#define SHOT_TIMER_TIMEOUT 15
#define BULLET_VELOCITY 2
#define BULLET_OFFSET_X 0
#define BULLET_OFFSET_X_LEFT 5
#define BULLET_OFFSET_Y 3
#define BULLET_WIDTH 2
#define BULLET_HEIGHT 2

//Need this per bullet?
unsigned char reloadShotTimer;
unsigned char isBulletInFlight;

#pragma bss-name (pop)
#pragma bss-name (push, "BSS")

void loadEnemies(void) {
  return;
  /*
  for(temp1 = 0 ; temp1 < NUM_ENEMIES ; ++temp1) {
      enemies[temp1] = enemies2[temp1];
  }*/
}

// 32 x 30
// TODO should use a bit per block instead of byte, but this is a lot easier at the moment
unsigned char collision[960];

void main (void) {
  allOff(); // turn off screen
	song = 0;
  gameState = 1;
  levelNum = 0;
  powerUpState = 1;

	loadPalette();
	resetScroll();

	Wait_Vblank();
	allOn(); // turn on screen
	while (1){ // infinite loop
    while (NMI_flag == 0); // wait till NMI

		//every_frame();	// moved this to the nmi code in reset.s for greater stability
		Get_Input();

    if (gameState == 1) {
      Reset_Music(); // note, this is famitone init, and I added the music data address. see famitone2.s
      Play_Music(song); // song = 0

			allOff();

			loadLevel();
      loadCollisionFromNametables();
      loadEnemies();
      isExploding = 0;

      initSprites();
			Wait_Vblank();
			allOn();
			resetScroll();
      gameState = 2;
    }
    else if (gameState == 2) {
      //In game
      newX = SPRITES[MAIN_CHAR_SPRITE_INDEX + 3];
      newY = SPRITES[MAIN_CHAR_SPRITE_INDEX];

      if(isExploding == 0) {
        preMovementUpdates();
        applyX();
        applyY();
        postMovementUpdates();
        enemyCollision();
        updateSprites();
      }
      else if (isExploding < 6) {
        SPRITES[MAIN_CHAR_SPRITE_INDEX + 1] = 0x0A + isExploding;

        //Hide weapon
        SPRITES[POWERUP_SPRITE_INDEX] = 0x00;
        SPRITES[POWERUP_SPRITE_INDEX + 3] = 0x00;

        if(Frame_Count % 8 == 0) {
          ++isExploding;
        }
      }
      else {
        //Reset level
        gameState = 1;
      }
    }

    Music_Update();

    NMI_flag = 0;
  }
}

//Would be better to do in asm (like in UnCollision) but haven't figured out a good way yet
void loadCollisionFromNametables(void) {
  PPU_ADDRESS = 0x20; // address of nametable #0
  PPU_ADDRESS = 0x00;

  //First read is always invalid
  tempInt = *((unsigned char*)0x2007);

  for(tempInt = 0 ; tempInt < 960 ; tempInt++) {
    temp1 = *((unsigned char*)0x2007);

    switch(temp1) {
      case 0x00: // Empty
      case 0x20: // Door
      case 0x21:
        collision[tempInt] = BACKGROUND_EMPTY;
        break;
      case 0x02: // Fire
        collision[tempInt] = BACKGROUND_DEATH;
        break;
      default:
        collision[tempInt] = BACKGROUND_SOLID;
    }

    if(temp1 == 0x20) {
      spawnX = 8*(tempInt % 32);
      spawnY = 8*(tempInt/32);
    }
  }
}

void preMovementUpdates(void) {
  //Powerup update
  if((joypad1 & SELECT) != 0 && (joypad1old & SELECT) == 0) {
    //Turn off current powerup
    if(powerUpState == POWERUP_SHOOT) {
      SPRITES[POWERUP_SPRITE_INDEX + 4] = 0;
      SPRITES[POWERUP_SPRITE_INDEX + 7] = 0;
      isBulletInFlight = 0;
      reloadShotTimer = 0;
    }

    powerUpState = (powerUpState + 1) % NUM_POWERUPS;
  }

  if(powerUpState == POWERUP_SHOOT && isBulletInFlight == 1) {
    if(SPRITES[POWERUP_SPRITE_INDEX + 6] == 0) {
      //Moving right
      SPRITES[POWERUP_SPRITE_INDEX + 7] += BULLET_VELOCITY;
    }
    else {
      //Moving Left
      SPRITES[POWERUP_SPRITE_INDEX + 7] -= BULLET_VELOCITY;
    }

    //Collision or offscreen??
  }
  //Enemies update
  // if enemy is platform walker
  temp4 = ENEMIES_SPRITE_INDEX;
  for(temp3 = 0 ; temp3 < NUM_ENEMIES ; ++temp3) {
    if(enemies[temp3].id == ENEMY_ID_WALKER) {
      if(enemies[temp3].state == WALKER_DEAD) {
          SPRITES[temp4 + 3]  = 0;
          SPRITES[temp4]  = 0;
      }
      else if(enemies[temp3].state > WALKER_WALKING) { // 1 - 5 is for exploding
        SPRITES[temp4 + 1] = 0x0A + enemies[temp3].state;
        if(Frame_Count % 8 == 0) {
           enemies[temp3].state += 1;
        }
      }
      else if(enemies[temp3].enemyTimer > 0) { // Else is walking
        enemies[temp3].enemyTimer -= 1;

        if(enemies[temp3].enemyTimer == 0) {
          //Once enemy has stopped "waiting at an edge", face the direction that we're gonna start moving in
          SPRITES[temp4 + 2] = (enemies[temp3].enemyState == ENEMY_MOVING_LEFT) ? 0x43 : 0x03;
        }
      }
      else if(enemies[temp3].enemyState == ENEMY_MOVING_RIGHT) {
        if(Frame_Count % ENEMY_MOVE_EVERY_X_FRAMES == 0) {
          SPRITES[temp4 + 3] += 1;
        }

        temp1 = (SPRITES[temp4 + 3] + 4) >> 3;
        temp2 = (SPRITES[temp4] + 9) >> 3;
        tempInt = 32*temp2 + temp1;

        // If we're hoving over an edge, turn around
        if(collision[tempInt] != BACKGROUND_SOLID || (SPRITES[temp4 + 3] % 8 == 0 && collision[tempInt - 31] == BACKGROUND_SOLID)) {
          enemies[temp3].enemyTimer = ENEMY_TURN_AROUND_TIME;
          enemies[temp3].enemyState = ENEMY_MOVING_LEFT;
        }
      }
      else if(enemies[temp3].enemyState == ENEMY_MOVING_LEFT) {
        if(Frame_Count % ENEMY_MOVE_EVERY_X_FRAMES == 0) {
          SPRITES[temp4 + 3] -= 1;
        }

        temp1 = (SPRITES[temp4 + 3] + 3) >> 3;
        temp2 = (SPRITES[temp4] + 9) >> 3;
        tempInt = 32*temp2 + temp1;

        // If we're hoving over an edge, turn around
        if(collision[tempInt] != BACKGROUND_SOLID || (SPRITES[temp4 + 3] % 8 == 0 && collision[tempInt - 33] == BACKGROUND_SOLID)) {
          enemies[temp3].enemyTimer = ENEMY_TURN_AROUND_TIME;
          enemies[temp3].enemyState = ENEMY_MOVING_RIGHT;
        }
      }

      if(enemies[temp3].state == WALKER_WALKING) {
        SPRITES[temp4 + 1] = 0x30 + (Frame_Count/4 % 2);
      }
    }
    else if(enemies[temp3].id == ENEMY_ID_DROP) {
      if(enemies[temp3].state == DROP_WAITING_TO_DROP) {
        ++enemies[temp3].enemyState;

        if(enemies[temp3].enemyState == DROP_MAX_WAIT_TO_DROP) {
          SPRITES[temp4 + 3] = enemies[temp3].startX;
          SPRITES[temp4] = enemies[temp3].startY;
          enemies[temp3].state = DROP_DROPPING;
          enemies[temp3].enemyState = 0;
        }
      }
      else if (enemies[temp3].state == DROP_DROPPING) {
        SPRITES[temp4 + 1] = 0x40;
        //Drop a pixel a frame TODO this should have accelleration, etc
        SPRITES[temp4] += 1; //If you change this, need to change where DROP_ON_GROUND gets placed below

        temp1 = (SPRITES[temp4 + 3]) >> 3; // x tile
        temp2 = (SPRITES[temp4] + 7) >> 3; // bottom pixel's y tile
        tempInt = 32*temp2 + temp1;

        // If we're hoving over an edge, turn around
        if(collision[tempInt] == BACKGROUND_SOLID) {
          enemies[temp3].state = DROP_ON_GROUND;
          SPRITES[temp4] += 2; //Adjust so sprite is on tile below.
        }
      }
      else if (enemies[temp3].state == DROP_ON_GROUND) {
        SPRITES[temp4 + 1] = 0x43;
        ++enemies[temp3].enemyState;

        //TODO use a dif constant
        if(enemies[temp3].enemyState == DROP_MAX_WAIT_TO_DROP) {
          enemies[temp3].state = DROP_WAITING_TO_DROP;
          enemies[temp3].enemyState = 0;
          //Remove for now
          SPRITES[temp4 + 3] = 0;
          SPRITES[temp4] = 0;
          SPRITES[temp4 + 1] = 0x02; //Blank
        }
      }
    }

    temp4 += 4;
  }
}

void applyX(void) {
  temp1 = (powerUpState == POWERUP_RUN && ((joypad1 & B_BUTTON) != 0)) ? MAX_VELOCITY_WITH_B_X : MAX_VELOCITY_X;
  if((joypad1 & LEFT) != 0) {
    isWalking = -1;
    SPRITES[MAIN_CHAR_SPRITE_INDEX + 2] = 0x40; //attribute
      //SPRITES[MAIN_CHAR_SPRITE_INDEX + 1] = MAIN_CHAR_FIRST_SPRITE; //sprite
    newX = SPRITES[MAIN_CHAR_SPRITE_INDEX + 3] - temp1;
  }
  else if((joypad1 & RIGHT) != 0) {
    isWalking = 1;
    SPRITES[MAIN_CHAR_SPRITE_INDEX + 2] = 0x00; //attribute
      //SPRITES[MAIN_CHAR_SPRITE_INDEX + 1] = MAIN_CHAR_FIRST_SPRITE; //sprite
    newX = SPRITES[MAIN_CHAR_SPRITE_INDEX + 3] + temp1;
  }
  else {
    isWalking = 0;
  }

  //Test X collision
  if(isBackgroundCollisionMainChar() == 0) {
    SPRITES[MAIN_CHAR_SPRITE_INDEX + 3] = newX;
  }
  else {
    newX = SPRITES[MAIN_CHAR_SPRITE_INDEX + 3];
  }
}

void applyY(void) {
  if(yVelocity >=0 && yVelocity < VELOCITY_FACTOR && isFalling == 0 && (joypad1 & A_BUTTON) != 0 && (joypad1old & A_BUTTON) == 0) {
    yVelocity = JUMP_VELOCITY;
    jumpCount = MAX_JUMP_COUNT;
  }
  else if((joypad1 & A_BUTTON) != 0 && jumpCount != 0) {
    --jumpCount;
    //yVelocity should still be JUMP_VELOCITY
  }
  else {
    yVelocity = yVelocity + GRAVITY;
  }

  tempSigned = yVelocity/VELOCITY_FACTOR;
  newY = SPRITES[MAIN_CHAR_SPRITE_INDEX] + tempSigned;

  //Test Y collision
  if(isBackgroundCollisionMainChar() == 0) {
    //Because of subpixels, want to make sure we're actually moving
    if(SPRITES[MAIN_CHAR_SPRITE_INDEX] != newY) {
      SPRITES[MAIN_CHAR_SPRITE_INDEX] = newY;
      isFalling = 1;
    }
  }
  else {
    isFalling = 0;
    //Round up to the block above this
    newY = newY - (newY % 8);
    if(yVelocity > 0) {
      //Falling so round up a block
      newY = newY - 8;
    }
    else {
      //Moving up so round down a block
      newY = newY + 8;
    }

    yVelocity = 0;
  }
}

void postMovementUpdates(void) {
  //Do powerup collision
  if(isBulletInFlight == 1) {
      if(SPRITES[POWERUP_SPRITE_INDEX + 6] == 0) {
        //Facing right
        collisionX = SPRITES[POWERUP_SPRITE_INDEX + 7] + BULLET_OFFSET_X;
      }
      else {
        collisionX = SPRITES[POWERUP_SPRITE_INDEX + 7] + BULLET_OFFSET_X_LEFT;
      }

      collisionY = SPRITES[POWERUP_SPRITE_INDEX + 4] + BULLET_OFFSET_Y;
      collisionWidth = BULLET_WIDTH;
      collisionHeight = BULLET_HEIGHT;

      if(isBackgroundCollision() != 0) {
        isBulletInFlight = 0;
      }
  }
}

void takeHit(void) {
  isExploding = 1;
}

int isCollision(void) {
    return x1 + width1 > x2
        && x2 + width2 > x1
        && y1 + height1 > y2
        && y2 + height2 > y1;
}

void enemyCollision(void) {
  if(isBackgroundDeathMainChar() != 0) {
    takeHit();
  }

  temp4 = ENEMIES_SPRITE_INDEX;
  for(temp3 = 0 ; temp3 < NUM_ENEMIES ; ++temp3) {
    if(enemies[temp3].id == ENEMY_ID_DROP) {
      if(enemies[temp3].state != DROP_DROPPING) {
        //TODO continue should work, but maybe only exiting if loop?
        x1 = 0;
        y1 = 0;
        width1 = 1;
        height1 = 1;
      }
      else {
        x1 = SPRITES[temp4 + 3] + DROP_OFFSET_X;
        y1 = SPRITES[temp4] + DROP_OFFSET_Y;

        width1 = DROP_WIDTH;
        height1 = DROP_HEIGHT;
      }
    }
    else if(enemies[temp3].id == ENEMY_ID_WALKER) {
      // Setup enemy
      x1 = SPRITES[temp4 + 3];
      y1 = SPRITES[temp4];
      //TODO adjust for drops
      width1 = 8;
      height1 = 8;
    }
    else {
      //Unsupported enemy id
      continue;
    }

    // Check against player
    x2 = SPRITES[MAIN_CHAR_SPRITE_INDEX + 3];
    y2 = SPRITES[MAIN_CHAR_SPRITE_INDEX];
    width2 = CHARACTER_WIDTH;
    height2 = CHARACTER_HEIGHT;

    if(isCollision() != 0) { //TODO for drops, only if falling
         takeHit();
    }

    if(enemies[temp3].id == ENEMY_ID_WALKER) {
      //TODO might be able to share some of this with dropper
      if(isBulletInFlight == 1 && enemies[temp3].state == WALKER_WALKING) {
        // Check against bullet
        temp1 = (SPRITES[POWERUP_SPRITE_INDEX + 6] == 0) ? BULLET_OFFSET_X : BULLET_OFFSET_X_LEFT;
        x2 = SPRITES[POWERUP_SPRITE_INDEX + 7] + temp1;
        y2 = SPRITES[POWERUP_SPRITE_INDEX + 4] + BULLET_OFFSET_Y;
        width2 = BULLET_WIDTH;
        height2 = BULLET_HEIGHT;

        if(isCollision() != 0) {
          // Killed enemy so clean it up
          isBulletInFlight = 0;
          enemies[temp3].state = WALKER_EXPLODING_START;
        }
      }
    }
    // Drops don't interact with enemies so nothing to do here

    temp4 += 4;
  }
}

char isBackgroundCollision(void) {
  temp1 = collisionX >> 3;
  temp2 = collisionY >> 3;
  tempInt = 32*temp2 + temp1;

  if(collision[tempInt] == BACKGROUND_EMPTY) {
    //More efficient ways than calculating all these
    temp1 = (collisionX + collisionWidth - 1) >> 3;
    temp2 = (collisionY) >> 3;
    tempInt = 32*temp2 + temp1;
    if(collision[tempInt] != BACKGROUND_EMPTY) {
      return 1;
    }

    temp1 = (collisionX) >> 3;
    temp2 = (collisionY + collisionHeight - 1) >> 3;
    tempInt = 32*temp2 + temp1;
    if(collision[tempInt] != BACKGROUND_EMPTY) {
      return 1;
    }

    temp1 = (collisionX + collisionWidth -1) >> 3;
    temp2 = (collisionY + collisionHeight - 1) >> 3;
    tempInt = 32*temp2 + temp1;
    if(collision[tempInt] != BACKGROUND_EMPTY) {
      return 1;
    }
  }
  else {
    return 1;
  }

  return 0;
}

//Useful bc main char is 8x8
char isBackgroundCollisionMainChar(void) {
  temp1 = newX >> 3;
  temp2 = newY >> 3;
  tempInt = 32*temp2 + temp1;

  if(collision[tempInt] == BACKGROUND_EMPTY) {
    if((newX % 8 != 0) && collision[tempInt + 1] == BACKGROUND_SOLID) {
      return 1;
    }

    if((newY % 8 != 0) && collision[tempInt + 32] == BACKGROUND_SOLID) {
      return 1;
    }

    if((newX % 8 != 0) && (newY % 8 != 0) && collision[tempInt + 33] == BACKGROUND_SOLID) {
      return 1;
    }
  }
  else {
    return 1;
  }

  return 0;
}

//TODO might be a better way to incorporate this above, but it gets really
//     corner casey for if you walk into both a solid AND death bc solid might
//     stop your movement before you can hit the fire.
char isBackgroundDeathMainChar(void) {
  temp1 = newX >> 3;
  temp2 = newY >> 3;
  tempInt = 32*temp2 + temp1;

  if(collision[tempInt] != BACKGROUND_DEATH) {
    if((newX % 8 != 0) && collision[tempInt + 1] == BACKGROUND_DEATH) {
      return 1;
    }

    if((newY % 8 != 0) && collision[tempInt + 32] == BACKGROUND_DEATH) {
      return 1;
    }

    if((newX % 8 != 0) && (newY % 8 != 0) && collision[tempInt + 33] == BACKGROUND_DEATH) {
      return 1;
    }
  }
  else {
    return 1;
  }

  return 0;
}

void initSprites(void) {
  SPRITES[MAIN_CHAR_SPRITE_INDEX]     = spawnY; //Y
  SPRITES[MAIN_CHAR_SPRITE_INDEX + 1] = MAIN_CHAR_FIRST_SPRITE; //sprite
  SPRITES[MAIN_CHAR_SPRITE_INDEX + 2] = 0x00; //attribute
  SPRITES[MAIN_CHAR_SPRITE_INDEX + 3] = spawnX; //X

  isBulletInFlight = 0;

  temp4 = ENEMIES_SPRITE_INDEX;
  for(temp3 = 0 ; temp3 < NUM_ENEMIES ; ++temp3) {
    SPRITES[temp4] = enemies[temp3].startY;
    SPRITES[temp4 + 2] = 0x03; //attribute
    SPRITES[temp4 + 3] = enemies[temp3].startX;

    if(enemies[temp3].id == ENEMY_ID_WALKER) {
      SPRITES[temp4 + 1] = 0x30; //sprite
    }
    else if(enemies[temp3].id == ENEMY_ID_DROP) {
      SPRITES[temp4 + 1] = 0x02; //sprite
    }

    temp4 += 4;
  }
}

void updateSprites(void) {
  if(isWalking != 0 && Frame_Count % 4 == 0) {
    SPRITES[MAIN_CHAR_SPRITE_INDEX + 1] = ((Frame_Count % 8) == 4) ? 0x00 : 0x01;
  }
  else {
    SPRITES[MAIN_CHAR_SPRITE_INDEX + 1] = 0;
  }

  // Do Powerup updates
  if(powerUpState == POWERUP_SHOOT) {
    SPRITES[POWERUP_SPRITE_INDEX] = SPRITES[MAIN_CHAR_SPRITE_INDEX]; //Same Y

    if(SPRITES[MAIN_CHAR_SPRITE_INDEX + 2] == 0x00) {
      //Facing right
      SPRITES[POWERUP_SPRITE_INDEX + 2] = SPRITES[MAIN_CHAR_SPRITE_INDEX + 2];
      SPRITES[POWERUP_SPRITE_INDEX + 3] = SPRITES[MAIN_CHAR_SPRITE_INDEX + 3] + 7;
    }
    else {
      SPRITES[POWERUP_SPRITE_INDEX + 2] = SPRITES[MAIN_CHAR_SPRITE_INDEX + 2];
      SPRITES[POWERUP_SPRITE_INDEX + 3] = SPRITES[MAIN_CHAR_SPRITE_INDEX + 3] - 7;
    }

    if(reloadShotTimer != 0) {
      --reloadShotTimer;
    }

    if(isBulletInFlight == 0) {
      if(reloadShotTimer == 0 && (joypad1 & B_BUTTON) != 0 && (joypad1old & B_BUTTON) == 0) {
        SPRITES[POWERUP_SPRITE_INDEX + 1] = 0x11;
        reloadShotTimer = SHOT_TIMER_TIMEOUT;
        isBulletInFlight = 1;
        SPRITES[POWERUP_SPRITE_INDEX + 4] = SPRITES[POWERUP_SPRITE_INDEX] - 1; //Y
        SPRITES[POWERUP_SPRITE_INDEX + 5] = 0x12; //Sprite
        SPRITES[POWERUP_SPRITE_INDEX + 6] = SPRITES[POWERUP_SPRITE_INDEX + 2]; //Attributes

        //Face the bullet in the same direction as gun.
        SPRITES[POWERUP_SPRITE_INDEX + 6] = SPRITES[POWERUP_SPRITE_INDEX + 2]; //Attributes
        if(SPRITES[POWERUP_SPRITE_INDEX + 2] == 0) {
          //Facing Right
          SPRITES[POWERUP_SPRITE_INDEX + 7] = SPRITES[POWERUP_SPRITE_INDEX + 3] + 4; //X
        }
        else {
          SPRITES[POWERUP_SPRITE_INDEX + 7] = SPRITES[POWERUP_SPRITE_INDEX + 3] - 4; //X
        }
      }
      else {

        SPRITES[POWERUP_SPRITE_INDEX + 1] = 0x10;
        SPRITES[POWERUP_SPRITE_INDEX + 4] = 0;
        SPRITES[POWERUP_SPRITE_INDEX + 7] = 0;
      }
    }
    else if(reloadShotTimer == 0) {
      //Put gun back down
      SPRITES[POWERUP_SPRITE_INDEX + 1] = 0x10;
    }
  }
  else {
    //By default show nothing
    SPRITES[POWERUP_SPRITE_INDEX] = 0;
    SPRITES[POWERUP_SPRITE_INDEX + 3] = 0;
  }
}

/**
 *  DCBA98 76543210
 *  ---------------
 *  0HRRRR CCCCPTTT
 *  |||||| |||||+++- T: Fine Y offset, the row number within a tile
 *  |||||| ||||+---- P: Bit plane (0: "lower"; 1: "upper")
 *  |||||| ++++----- C: Tile column
 *  ||++++---------- R: Tile row
 *  |+-------------- H: Half of sprite table (0: "left"; 1: "right")
 *  +--------------- 0: Pattern table is at $0000-$1FFF
 */
void allOff (void) {
	PPU_CTRL = 0;
	PPU_MASK = 0;
}

void allOn (void) {
	PPU_CTRL = 0x80; // This will turn NMI on, make sure this matches the one in the NMI loop
	PPU_MASK = 0x1e; // enable all rendering
}

void resetScroll (void) {
	PPU_ADDRESS = 0;
	PPU_ADDRESS = 0;
	SCROLL = 0;
	SCROLL = 0xff;
}
