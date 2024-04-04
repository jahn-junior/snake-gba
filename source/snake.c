#include <gba_console.h>
#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160

#define MEM_IO 0x04000000
#define MEM_PAL 0x05000000
#define MEM_VRAM 0x06000000
#define MEM_OAM 0x07000000

#define REG_DISPLAY (*((volatile uint32 *)(MEM_IO)))
#define REG_DISPLAY_VCOUNT (*((volatile uint32 *)(MEM_IO + 0x0006)))
#define REG_KEY_INPUT (*((volatile uint32 *)(MEM_IO + 0x0130)))

#define KEY_UP 0x0040
#define KEY_DOWN 0x0080
#define KEY_LEFT 0x0020
#define KEY_RIGHT 0x0010
#define KEY_ANY 0x03FF

#define OBJECT_ATTR0_Y_MASK 0x0FF
#define OBJECT_ATTR1_X_MASK 0x1FF

#define oam_mem ((volatile obj_attrs *)MEM_OAM)
#define tile_mem ((volatile tile_block *)MEM_VRAM)
#define object_palette_mem ((volatile rgb15 *)(MEM_PAL + 0x200))

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef uint16 rgb15;
typedef uint32 tile_4bpp[8];
typedef tile_4bpp tile_block[512];

typedef struct obj_attrs {
  uint16 attr0;
  uint16 attr1;
  uint16 attr2;
  uint16 pad;
} __attribute__((packed, aligned(4))) obj_attrs;

typedef struct obj_pos {
  uint8 x;
  uint8 y;
  uint8 w;
  uint8 h;
} obj_pos;

/* Initializes the random number generator. */
void initRandom() {
  time_t t;
  srand((unsigned)time(&t));
}

/* Return a random number from [0 - n) (exclusive of n) */
int nextInt(int n) {
  return rand() % n;
}

// Form a 16-bit BGR GBA colour from three component values
static inline rgb15 RGB15(int r, int g, int b) {
  return r | (g << 5) | (b << 10);
}

// Set the position of an object to specified x and y coordinates
static inline void set_object_position(volatile obj_attrs *object, int x, int y) {
  object->attr0 = (object->attr0 & ~OBJECT_ATTR0_Y_MASK) | (y & OBJECT_ATTR0_Y_MASK);
  object->attr1 = (object->attr1 & ~OBJECT_ATTR1_X_MASK) | (x & OBJECT_ATTR1_X_MASK);
}

// Clamp 'value' in the range 'min' to 'max' (inclusive)
static inline uint8 clamp(int value, int min, int max) {
  return (value < min ? min : (value > max ? max : value));
}

int main(void) {

  volatile uint16* head_tile_mem = (uint16*)tile_mem[4][1];
  volatile uint16* target_tile_mem = (uint16*)tile_mem[4][5];
  
  for (int i = 0; i < (sizeof(tile_4bpp) / 2); i++) head_tile_mem[i] = 0x1111;
  for (int i = 0; i < (sizeof(tile_4bpp) / 2); i++) target_tile_mem[i] = 0x2222;
  
  object_palette_mem[1] = RGB15(0x1F, 0x1F, 0x1F); // White
  object_palette_mem[2] = RGB15(0x1F, 0x00, 0x00); // Red
  
  volatile obj_attrs* head_attrs = &oam_mem[0];
  head_attrs->attr0 = 0;
  head_attrs->attr1 = 0;
  head_attrs->attr2 = 1;

  volatile obj_attrs* target_attrs = &oam_mem[1];
  target_attrs -> attr0 = 0;
  target_attrs -> attr1 = 0;
  target_attrs -> attr2 = 5;

  const uint8 CELL_SIZE = 8;
  const uint8 BOARD_SIZE = 10;
  struct obj_pos head = {.x = 4, .y = 4, .w = CELL_SIZE, .h = CELL_SIZE};
  set_object_position(head_attrs, head.x * CELL_SIZE, head.y * CELL_SIZE);

  struct obj_pos target = {.x = 0, .y = 0, .w = CELL_SIZE, .h = CELL_SIZE};
  target.x = nextInt(10);
  target.y = nextInt(10);
  set_object_position(target_attrs, target.x * CELL_SIZE, target.y * CELL_SIZE);

  uint8 dir = 0; // 0 = UP, 1 = RIGHT, 2 = DOWN, 3 = LEFT
  uint16 length = 1;

  // 0 = BLANK, 1 = SNAKE, 2 = TARGET
  uint8 board[10][10] = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}; 
  
  board[head.y][head.x] = 1;
  board[target.y][target.x] = 2;

  REG_DISPLAY = 0x1000 | 0x0040;
  uint32 keyPress = 0;

  int frame_count = 0;
  int speed = 30;
  
  while(1) {
    while (REG_DISPLAY_VCOUNT >= 160);
    while (REG_DISPLAY_VCOUNT < 160);
    
    keyPress = ~REG_KEY_INPUT & KEY_ANY;
     
    if (keyPress & KEY_UP) {
      dir = 0;
    } else if (keyPress & KEY_RIGHT) {
      dir = 1;
    } else if (keyPress & KEY_DOWN) {
      dir = 2;
    } else if (keyPress & KEY_LEFT) {
      dir = 3;
    }

    frame_count++;
    if (frame_count > speed) {
      frame_count = 0;

      board[head.y][head.x] = 0;
      if (dir == 0) {
        head.y = clamp(head.y - 1, 0, BOARD_SIZE - 1);
      } else if (dir == 1) {
        head.x = clamp(head.x + 1, 0, BOARD_SIZE - 1);
      } else if (dir == 2) {
        head.y = clamp(head.y + 1, 0, BOARD_SIZE - 1);
      } else {
        head.x = clamp(head.x - 1, 0, BOARD_SIZE - 1);
      }

      board[head.y][head.x] = 1;
      set_object_position(head_attrs, head.x * CELL_SIZE, head.y * CELL_SIZE);
    
      if (head.x == target.x && head.y == target.y) {
        target.x = nextInt(BOARD_SIZE);
        target.y = nextInt(BOARD_SIZE);
        set_object_position(target_attrs, target.x * CELL_SIZE, target.y * CELL_SIZE);
        length++;
      }   
    }
  }

  return 0;
}
