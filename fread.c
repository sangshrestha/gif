#include <stdio.h>
#include <stdlib.h>

#define SignatureLength 3
#define VersionLength 3
#define SEPARATOR 0x2C
#define TRAILER 0x3B
#define EXTENSION 0x21
#define GRAPHICS_CONTROL_LABEL 0xF9
#define PLAIN_TEXT_LABEL 0x01
#define APPLICATION_LABEL 0xFF
#define COMMENT_LABEL 0xFE

typedef unsigned char BYTE;
typedef unsigned int WORD;

struct ColourTable
{
  BYTE Red;
  BYTE Green;
  BYTE Blue;
};

struct GIF
{
  BYTE Signature[SignatureLength];
  BYTE Version[VersionLength];
  WORD ScreenWidth;
  WORD ScreenHeight;
  BYTE Packed;
  BYTE BackgroundColour;
  BYTE AspectRatio;
  struct ColourTable *GlobalColourTable;

  // TODO: extension + image data

  int BytesCount;
};

struct Image
{
  WORD Left;
  WORD Top;
  WORD Width;
  WORD Height;
  BYTE Packed;
  struct ColourTable LocalColourTable[256];
  BYTE LZWMinimumCodeSize;
};

BYTE read_byte(FILE *file, struct GIF *gif);
WORD read_word(FILE *file, struct GIF *gif);
void read_n_bytes(FILE *file, struct GIF *gif, BYTE *dest, int size);
BYTE skip_byte(FILE *file, struct GIF *gif);
WORD skip_word(FILE *file, struct GIF *gif);
void skip_n_bytes(FILE *file, struct GIF *gif, BYTE *dest, int size);
void peek_chars(FILE *file, struct GIF *gif, int n);
int binary(int n);


BYTE *copy;

int main(int argc, char** argv)
{
  if (argc < 2) {
    printf("Usage: fread PEPEGA.gif");
    return 1;
  }

  FILE *file;
  errno_t fopen_err;

  fopen_err = fopen_s(&file, argv[1], "rb");

  if (fopen_err != 0) {
    printf("fopen_s error: %d\n", fopen_err);
    return fopen_err;
  }

  // TODO: malloc dynamic
  copy = malloc(300000);

  struct GIF gif = {0};
  read_n_bytes(file, &gif, gif.Signature, sizeof(gif.Signature));
  read_n_bytes(file, &gif, gif.Version, sizeof(gif.Version));
  gif.ScreenWidth = read_word(file, &gif);
  gif.ScreenHeight = read_word(file, &gif);
  gif.Packed = read_byte(file, &gif);
  gif.BackgroundColour = read_byte(file, &gif);
  gif.AspectRatio = read_byte(file, &gif);
  printf("haha: %d\n", gif.BytesCount);

  printf("Signature: %c%c%c", gif.Signature[0], gif.Signature[1], gif.Signature[2]);
  printf(" | Version: %c%c%c", gif.Version[0], gif.Version[1], gif.Version[2]);
  printf(" | ScreenWidth: %d", gif.ScreenWidth);
  printf(" | ScreenHeight: %d", gif.ScreenHeight);
  printf(" | Packed: %.8d", binary(gif.Packed));
  printf(" | BackgroundColour: %d", gif.BackgroundColour);
  printf(" | AspectRatio: %d\n", gif.AspectRatio);

  int global_colour_table_count = 1L << ((gif.Packed  & 0x07) + 1);
  gif.GlobalColourTable = malloc(global_colour_table_count * sizeof(struct ColourTable));


  printf("\nGLOBAL COLOUR TABLE: ");
  for (int i = 0; i < global_colour_table_count; i++) {
    gif.GlobalColourTable[i].Red = read_byte(file, &gif);
    gif.GlobalColourTable[i].Green = read_byte(file, &gif);
    gif.GlobalColourTable[i].Blue = read_byte(file, &gif);

    printf("#%.2X%.2X%.2X ", gif.GlobalColourTable[i].Red, gif.GlobalColourTable[i].Green, gif.GlobalColourTable[i].Blue);
  }
  printf("\n\n");

  struct Image images[256]; // TODO: assuming 256 frames based on 1MB gif size and 64x64 pixels
  int image_count = 0;

  while (1) {
    WORD (*act_word)(FILE *file, struct GIF *gif);
    BYTE (*act_byte)(FILE *file, struct GIF *gif);
    void (*act_n_bytes)(FILE *file, struct GIF *gif, BYTE *dest, int size);

    if (1) {
      act_word = read_word;
      act_byte = read_byte;
      act_n_bytes = read_n_bytes;
    }
    else {
      act_word = skip_word;
      act_byte = skip_byte;
      act_n_bytes = skip_n_bytes;
    }

    BYTE byte = act_byte(file, &gif);
    while (byte == EXTENSION) {
      BYTE extension_label = act_byte(file, &gif);

      switch (extension_label) {
        case APPLICATION_LABEL:
          // just skipping the bytes for now
          BYTE identifier[8];
          BYTE authentcode[3];
          act_byte(file, &gif); // blocksize
          act_n_bytes(file, &gif, identifier, 8); // identifier
          act_n_bytes(file, &gif, authentcode, 3); // authentcode

          // Sub-block
          BYTE c = act_byte(file, &gif);
          while (c != 0) {
            for (int i = 0; i < c; i++) {
              act_byte(file, &gif);
            }

            c = act_byte(file, &gif);
          }
          break;

        case COMMENT_LABEL:
          // Sub-block
          c = act_byte(file, &gif);
          while (c != 0) {
            for (int i = 0; i < c; i++) {
              act_byte(file, &gif);
            }
            c = act_byte(file, &gif);
          }
          break;

        case GRAPHICS_CONTROL_LABEL:
          // just skipping the bytes for now
          act_byte(file, &gif); // blocksize
          act_byte(file, &gif); // packed
          printf("gfx packed: %.8u\n", binary(copy[gif.BytesCount-1]));
          act_word(file, &gif); // delaytime
          act_byte(file, &gif); // colorindex
          act_byte(file, &gif); // terminator
          break;
      }

      byte = act_byte(file, &gif);
    }
    
    if (byte == TRAILER) {
      break;
    }

    images[image_count].Left = act_word(file, &gif);
    images[image_count].Top = act_word(file, &gif);
    images[image_count].Width = act_word(file, &gif);
    images[image_count].Height = act_word(file, &gif);
    images[image_count].Packed = act_byte(file, &gif);

    printf("Frame #%.3d: left = %.3d, top = %.3d, width = %.3d, height = %.3d, packed = %.8d\n",
        image_count, images[image_count].Left, images[image_count].Top, images[image_count].Width, images[image_count].Height, binary(images[image_count].Packed));

    if ((images[image_count].Packed >> 7) & 0x1) {
      int local_colour_table_count = 1L << ((images[image_count].Packed & 0x07) + 1);

      printf("LOCAL COLOUR TABLE: ");
      for (int i = 0; i < local_colour_table_count; i++) {
        images[image_count].LocalColourTable[i].Red = act_byte(file, &gif);
        images[image_count].LocalColourTable[i].Green = act_byte(file, &gif);
        images[image_count].LocalColourTable[i].Blue = act_byte(file, &gif);

        if (i < 3 || i > local_colour_table_count - 3) {
          printf("%d. #%.2X%.2X%.2X ", i, images[image_count].LocalColourTable[i].Red, images[image_count].LocalColourTable[i].Green, images[image_count].LocalColourTable[i].Blue);
        }
      }
    }
    printf("\n");

    images[image_count].LZWMinimumCodeSize = act_byte(file, &gif);

    // IMAGE DATA
    BYTE b = act_byte(file, &gif); // count
    while ( b != 0) {
      // TODO: decode image data
      for (int i = 0; i < b; i++) {
        act_byte(file, &gif);
      }

      b = act_byte(file, &gif);
    }

    image_count++;
  }

  fclose(file);

  FILE *copy;
  errno_t copy_err;

  copy_err = fopen_s(&copy, "copy.gif", "wb");
  fwrite(copy, 1, gif.BytesCount, copy);
  fclose(copy);
  free(copy);
  free(gif.GlobalColourTable);
  printf("\nbytes count: %d\n", gif.BytesCount);
  printf("\nfile closed\n");
  return 0;
}

BYTE read_byte(FILE *file, struct GIF *gif)
{
  BYTE tmp;
  fread_s(&tmp, 1, 1, 1, file);
  copy[gif->BytesCount] = tmp;
  gif->BytesCount++;
  return tmp;
}

BYTE skip_byte(FILE *file, struct GIF *gif)
{
  BYTE tmp;
  fread_s(&tmp, 1, 1, 1, file);
  return tmp;
}

WORD read_word(FILE *file, struct GIF *gif)
{
  return (read_byte(file, gif) | (read_byte(file, gif) << 8));
}

WORD skip_word(FILE *file, struct GIF *gif)
{
  return (skip_byte(file, gif) | (skip_byte(file, gif) << 8));
}

void read_n_bytes(FILE *file, struct GIF *gif, BYTE *dest, int size)
{
  for (int i = 0; i < size; i++) {
    dest[i] = read_byte(file, gif);
  }
}

void skip_n_bytes(FILE *file, struct GIF *gif, BYTE *dest, int size)
{
  for (int i = 0; i < size; i++) {
    dest[i] = skip_byte(file, gif);
  }
}

// TODO: make this independent
void peek_chars(FILE *file, struct GIF *gif, int n)
{
  for (int i = 0; i < n; i++) {
    printf("%x ", read_byte(file, gif));
  }
}

// note that this isnt actually binary value
// i am representing what the binary looks like with an actual integer
int binary(int n)
{
  int b = 0;
  int power10 = 10000000;

  for (int i = 7; i >= 0; i--) {
    if ((n / (1 << i)) == 1) {
      b = b + power10;
      n = n - (1 << i);
    }
    power10 = power10 / 10;
  }

  return b;
}
