#include <stdio.h>
#include <math.h>

#define SignatureLength 3
#define VersionLength 3
#define SEPARATOR 0x2C
#define TRAILER 0x3B
#define EXTENSION 0x21
#define GRAPHICS_CONTROL_LABEL 0xF9
#define PLAIN_TEXT_LABEL 0x01
#define APPLICATION_LABEL 0xFF

typedef unsigned char BYTE;
typedef unsigned int WORD;

BYTE read_byte(FILE *file);
WORD read_word(FILE *file);

struct Packed
{
  int SizeOfGlobalColourTable;
  int ColourTableSortFlag;
  int ColourResolution;
  int GlobalColourTableFlag;
};

struct Header
{
  BYTE Signature[SignatureLength+1];
  BYTE Version[VersionLength+1];

  WORD ScreenWidth;
  WORD ScreenHeight;

  struct Packed packed;
  BYTE BackgroundColour;
  BYTE AspectRatio;
};

struct ColourTable
{
  BYTE Red;
  BYTE Green;
  BYTE Blue;
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

  struct Header header = {0};

  fread_s(header.Signature, sizeof(header.Signature), 1, SignatureLength, file);
  fread_s(header.Version, sizeof(header.Version), 1, VersionLength, file);
  header.ScreenWidth = read_word(file);
  header.ScreenHeight = read_word(file);

  // TODO: keep packed as a byte
  char packed = read_byte(file);
  header.packed.SizeOfGlobalColourTable = (packed >> 5) & 0x07;
  header.packed.ColourTableSortFlag = (packed >> 4) & 0x01;
  header.packed.ColourResolution = (packed >> 1) & 0x07;
  header.packed.GlobalColourTableFlag = packed & 0x01;

  header.BackgroundColour = read_byte(file);
  header.AspectRatio = read_byte(file);

  printf("Signature: %s\n", header.Signature);
  printf("Version: %s\n", header.Version);
  printf("ScreenWidth: %d\n", header.ScreenWidth);
  printf("ScreenHeight: %d\n", header.ScreenHeight);
  printf("\nPACKED:\n");
  printf("SizeOfGlobalColourTable: %d\n", header.packed.SizeOfGlobalColourTable);
  printf("ColourTableSortFlag: %d\n", header.packed.ColourTableSortFlag);
  printf("ColourResolution: %d\n", header.packed.ColourResolution);
  printf("GlobalColourTableFlag: %d\n", header.packed.GlobalColourTableFlag);
  printf("\n");
  printf("BackgroundColour: %d\n", header.BackgroundColour);
  printf("AspectRatio: %d\n", header.AspectRatio);

  int global_colour_table_count = 1L << (header.packed.SizeOfGlobalColourTable + 1);
  struct ColourTable global_colour_table[global_colour_table_count];
  printf("\nGLOBAL COLOUR TABLE:\n");
  for (int i = 0; i < global_colour_table_count; i++) {
    global_colour_table[i].Red = read_byte(file);
    global_colour_table[i].Green = read_byte(file);
    global_colour_table[i].Blue = read_byte(file);

    if (i < 3 || i > global_colour_table_count - 3) {
      printf("%.3d: #%.2X%.2X%.2X\n", i, global_colour_table[i].Red, global_colour_table[i].Green, global_colour_table[i].Blue);
    }
  }

  // TODO: extension block
  while (read_byte(file) != SEPARATOR) {
    //
  }

  struct Image images[256]; // TODO: assuming 256 frames based on 1MB gif size and 64x64 pixels
  int image_count = 0;

  printf("\nLOCAL IMAGE DESCRIPTORS:\n");
  while (image_count > -1) {
    images[image_count].Left = read_word(file);
    images[image_count].Top = read_word(file);
    images[image_count].Width = read_word(file);
    images[image_count].Height = read_word(file);
    images[image_count].Packed = read_byte(file);

    printf("Frame #%.3d: left = %d, top = %d, width = %d, height = %d, packed = %o\n", image_count, images[image_count].Left, images[image_count].Top, images[image_count].Width, images[image_count].Height, images[image_count].Packed);

    if (images[image_count].Packed & 0x128) {
      int local_colour_table_count = 1L << ((images[image_count].Packed & 0x07) + 1);

      printf("LOCAL COLOUR TABLE:\n");
      for (int i = 0; i < local_colour_table_count; i++) {
        images[image_count].LocalColourTable[i].Red = read_byte(file);
        images[image_count].LocalColourTable[i].Green = read_byte(file);
        images[image_count].LocalColourTable[i].Blue = read_byte(file);

        if (i < 3 || i > local_colour_table_count - 3) {
          printf("%d: %d %d %d\n", i, images[image_count].LocalColourTable[i].Red, images[image_count].LocalColourTable[i].Green, images[image_count].LocalColourTable[i].Blue);
        }
      }
    }

    images[image_count].LZWMinimumCodeSize = read_byte(file);

    BYTE b = read_byte(file);

    // IMAGE DATA
    while (b != 0) {
      // TODO: decode image data
      for (int i = 0; i < b; i++) {
        read_byte(file);
      }

      b = read_byte(file);
    }
    b = read_byte(file); // skip 0

    while (b == EXTENSION) {
      BYTE extension_label = read_byte(file);

      switch (extension_label) {
        case GRAPHICS_CONTROL_LABEL:
          // just skipping the bytes for now
          read_byte(file); // blocksize
          read_byte(file); // packed
          read_word(file); // delaytime
          read_byte(file); // colorindex
          read_byte(file); // terminator
          break;
        // TODO: more cases
      }

      b = read_byte(file);
    }

    if (b == SEPARATOR) {
      image_count++;
    }
    else {
      image_count = -1;
    }
  }

  // printf("\npeek chars: ");
  // for (int i = 0; i < 8; i++) {
  //   char c = read_byte(file);
  //   printf("%x ", c);
  // }
  // printf("\n");

  fclose(file);
  printf("file closed\n");
  return 0;
}

BYTE read_byte(FILE *file)
{
  BYTE tmp;
  fread_s(&tmp, 1, 1, 1, file);
  return tmp;
}

WORD read_word(FILE *file)
{
  return (read_byte(file) | (read_byte(file) << 8));
}
