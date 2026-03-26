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

BYTE read_byte(FILE *file);
WORD read_word(FILE *file);
void read_n_bytes(FILE *file, BYTE *dest, int size);
BYTE skip_byte(FILE *file);
WORD skip_word(FILE *file);
void skip_n_bytes(FILE *file, BYTE *dest, int size);
void peek_chars(FILE *file, int n);
int binary(int n);

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
};

struct Header
{
  BYTE Signature[SignatureLength];
  BYTE Version[VersionLength];

  WORD ScreenWidth;
  WORD ScreenHeight;

  BYTE Packed;
  BYTE BackgroundColour;
  BYTE AspectRatio;
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

int bytes_count = 0;
BYTE *gif;

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
  gif = malloc(300000);

  struct Header header = {0};
  read_n_bytes(file, header.Signature, sizeof(header.Signature));
  read_n_bytes(file, header.Version, sizeof(header.Version));
  header.ScreenWidth = read_word(file);
  header.ScreenHeight = read_word(file);
  header.Packed = read_byte(file);
  header.BackgroundColour = read_byte(file);
  header.AspectRatio = read_byte(file);

  printf("Signature: %c%c%c", header.Signature[0], header.Signature[1], header.Signature[2]);
  printf(" | Version: %c%c%c", header.Version[0], header.Version[1], header.Version[2]);
  printf(" | ScreenWidth: %d", header.ScreenWidth);
  printf(" | ScreenHeight: %d", header.ScreenHeight);
  printf(" | Packed: %.8d", binary(header.Packed));
  printf(" | BackgroundColour: %d", header.BackgroundColour);
  printf(" | AspectRatio: %d\n", header.AspectRatio);

  int global_colour_table_count = 1L << ((header.Packed  & 0x07) + 1);
  struct ColourTable global_colour_table[global_colour_table_count];

  printf("\nGLOBAL COLOUR TABLE: ");
  for (int i = 0; i < global_colour_table_count; i++) {
    global_colour_table[i].Red = read_byte(file);
    global_colour_table[i].Green = read_byte(file);
    global_colour_table[i].Blue = read_byte(file);

    printf("#%.2X%.2X%.2X ", global_colour_table[i].Red, global_colour_table[i].Green, global_colour_table[i].Blue);
  }
  printf("\n\n");

  struct Image images[256]; // TODO: assuming 256 frames based on 1MB gif size and 64x64 pixels
  int image_count = 0;

  while (1) {
    WORD (*act_word)(FILE *file);
    BYTE (*act_byte)(FILE *file);
    void (*act_n_bytes)(FILE *file, BYTE *dest, int size);

    if (image_count % 2 == 0) {
      act_word = read_word;
      act_byte = read_byte;
      act_n_bytes = read_n_bytes;
    }
    else {
      act_word = skip_word;
      act_byte = skip_byte;
      act_n_bytes = skip_n_bytes;
    }

    BYTE byte = act_byte(file);
    while (byte == EXTENSION) {
      BYTE extension_label = act_byte(file);

      switch (extension_label) {
        case APPLICATION_LABEL:
          // just skipping the bytes for now
          BYTE identifier[8];
          BYTE authentcode[3];
          act_byte(file); // blocksize
          act_n_bytes(file, identifier, 8); // identifier
          act_n_bytes(file, authentcode, 3); // authentcode

          // Sub-block
          BYTE c = act_byte(file);
          while (c != 0) {
            for (int i = 0; i < c; i++) {
              act_byte(file);
            }

            c = act_byte(file);
          }
          break;

        case COMMENT_LABEL:
          // Sub-block
          c = act_byte(file);
          while (c != 0) {
            for (int i = 0; i < c; i++) {
              act_byte(file);
            }
            c = act_byte(file);
          }
          break;

        case GRAPHICS_CONTROL_LABEL:
          // just skipping the bytes for now
          act_byte(file); // blocksize
          act_byte(file); // packed
          printf("gfx packed: %.8u\n", binary(gif[bytes_count-1]));
          act_word(file); // delaytime
          act_byte(file); // colorindex
          act_byte(file); // terminator
          break;
      }

      byte = act_byte(file);
    }
    
    if (byte == TRAILER) {
      break;
    }

    images[image_count].Left = act_word(file);
    images[image_count].Top = act_word(file);
    images[image_count].Width = act_word(file);
    images[image_count].Height = act_word(file);
    images[image_count].Packed = act_byte(file);

    printf("Frame #%.3d: left = %.3d, top = %.3d, width = %.3d, height = %.3d, packed = %.8d\n",
        image_count, images[image_count].Left, images[image_count].Top, images[image_count].Width, images[image_count].Height, binary(images[image_count].Packed));

    if ((images[image_count].Packed >> 7) & 0x1) {
      int local_colour_table_count = 1L << ((images[image_count].Packed & 0x07) + 1);

      printf("LOCAL COLOUR TABLE: ");
      for (int i = 0; i < local_colour_table_count; i++) {
        images[image_count].LocalColourTable[i].Red = act_byte(file);
        images[image_count].LocalColourTable[i].Green = act_byte(file);
        images[image_count].LocalColourTable[i].Blue = act_byte(file);

        if (i < 3 || i > local_colour_table_count - 3) {
          printf("%d. #%.2X%.2X%.2X ", i, images[image_count].LocalColourTable[i].Red, images[image_count].LocalColourTable[i].Green, images[image_count].LocalColourTable[i].Blue);
        }
      }
    }
    printf("\n");

    images[image_count].LZWMinimumCodeSize = act_byte(file);

    // IMAGE DATA
    BYTE b = act_byte(file); // count
    while ( b != 0) {
      // TODO: decode image data
      for (int i = 0; i < b; i++) {
        act_byte(file);
      }

      b = act_byte(file);
    }

    image_count++;
  }

  fclose(file);

  FILE *copy;
  errno_t copy_err;

  copy_err = fopen_s(&copy, "copy.gif", "wb");
  fwrite(gif, 1, bytes_count, copy);
  fclose(copy);
  free(gif);
  printf("\nbytes count: %d\n", bytes_count);
  printf("\nfile closed\n");
  return 0;
}

BYTE read_byte(FILE *file)
{
  BYTE tmp;
  fread_s(&tmp, 1, 1, 1, file);
  gif[bytes_count] = tmp;
  bytes_count++;
  return tmp;
}

BYTE skip_byte(FILE *file)
{
  BYTE tmp;
  fread_s(&tmp, 1, 1, 1, file);
  return tmp;
}

WORD read_word(FILE *file)
{
  return (read_byte(file) | (read_byte(file) << 8));
}

WORD skip_word(FILE *file)
{
  return (skip_byte(file) | (skip_byte(file) << 8));
}

void read_n_bytes(FILE *file, BYTE *dest, int size)
{
  for (int i = 0; i < size; i++) {
    dest[i] = read_byte(file);
  }
}

void skip_n_bytes(FILE *file, BYTE *dest, int size)
{
  for (int i = 0; i < size; i++) {
    dest[i] = skip_byte(file);
  }
}


void peek_chars(FILE *file, int n)
{
  for (int i = 0; i < n; i++) {
    printf("%x ", read_byte(file));
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
