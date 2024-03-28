#include <time.h>
#include <stdlib.h>

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

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef uint16 rgb15;
typedef struct obj_attrs {
  uint16 attr0;
  uint16 attr1;
  uint16 attr2;
  uint16 pad;
} __attribute__((packed, aligned(4))) obj_attrs;
typedef uint32 tile_4bpp[8];
typedef tile_4bpp tile_block[512];

#define oam_mem ((volatile obj_attrs *)MEM_OAM)
#define tile_mem ((volatile tile_block *)MEM_VRAM)
#define object_palette_mem ((volatile rgb15 *)(MEM_PAL + 0x200))

/* Initializes the random number generator. */
void initRandom()
{
  time_t t;
  srand((unsigned)time(&t));
}

/* Return a random number from [0 - n) (exclusive of n) */
int nextInt(int n)
{
  return rand() % n;
}

// Form a 16-bit BGR GBA colour from three component values
static inline rgb15 RGB15(int r, int g, int b) {
  return r | (g << 5) | (b << 10);
}

// Set the position of an object to specified x and y coordinates
static inline void set_object_position(volatile obj_attrs *object, int x,
                                       int y) {
  object->attr0 =
      (object->attr0 & ~OBJECT_ATTR0_Y_MASK) | (y & OBJECT_ATTR0_Y_MASK);
  object->attr1 =
      (object->attr1 & ~OBJECT_ATTR1_X_MASK) | (x & OBJECT_ATTR1_X_MASK);
}

// Clamp 'value' in the range 'min' to 'max' (inclusive)
static inline int clamp(int value, int min, int max) {
  return (value < min ? min : (value > max ? max : value));
}

int main(void) {

  volatile uint16* player_tile_mem = (uint16*)tile_mem[4][1];
  volatile uint16* target_tile_mem = (uint16*)tile_mem[4][5];
  
  for (int i = 0; i < (sizeof(tile_4bpp) / 2); i++)
  {
    player_tile_mem[i] = 0x1111;
  }

  for (int i = 0; i < (sizeof(tile_4bpp) / 2); i++)
  {
    target_tile_mem[i] = 0x2222;
  }
  
  object_palette_mem[1] = RGB15(0x1F, 0x1F, 0x1F); // White
  object_palette_mem[2] = RGB15(0x1F, 0x00, 0x00); // Red
  
  volatile obj_attrs* player_attrs = &oam_mem[0];
  player_attrs->attr0 = 0;
  player_attrs->attr1 = 0;
  player_attrs->attr2 = 1;

  volatile obj_attrs* target_attrs = &oam_mem[1];
  target_attrs -> attr0 = 0;
  target_attrs -> attr1 = 0;
  target_attrs -> attr2 = 5;

  const int player_width = 8, player_height = 8;
  const int player_velocity = 2;
  int player_x = 120, player_y = 80;
  set_object_position(player_attrs, player_x, player_y);

  const int target_width = 8, target_height = 8;
  int target_x = nextInt(SCREEN_WIDTH - target_width);
  int target_y = nextInt(SCREEN_HEIGHT - target_height);
  set_object_position(target_attrs, target_x, target_y);

  REG_DISPLAY = 0x1000 | 0x0040;

  uint32 keyPress = 0;
  int score = 0;
  
  while(1)
  {
    while (REG_DISPLAY_VCOUNT >= 160)
      ;
    while (REG_DISPLAY_VCOUNT < 160)
      ;

    int maxX = SCREEN_WIDTH - player_width,
        maxY = SCREEN_HEIGHT - player_height;

    keyPress = ~REG_KEY_INPUT & KEY_ANY;

    if (player_x >= target_x - target_width && player_x <= target_x + target_width &&
	player_y >= target_y - target_height && player_y <= target_y + target_height)
    {
      target_x = nextInt(SCREEN_WIDTH - target_width);
      target_y = nextInt(SCREEN_HEIGHT - target_height);
      set_object_position(target_attrs, target_x, target_y);
      score++;
    }
    
    if (keyPress & KEY_UP) {
      player_y = clamp(player_y - player_velocity, 0, maxY);
      set_object_position(player_attrs, player_x, player_y);
    } if (keyPress & KEY_DOWN) {
      player_y = clamp(player_y + player_velocity, 0, maxY);
      set_object_position(player_attrs, player_x, player_y);
    } if (keyPress & KEY_LEFT) {
      player_x = clamp(player_x - player_velocity, 0, maxX);
      set_object_position(player_attrs, player_x, player_y);
    } if (keyPress & KEY_RIGHT) {
      player_x = clamp(player_x + player_velocity, 0, maxX);
      set_object_position(player_attrs, player_x, player_y);
    }
    
  }
  
  return 0;
}
