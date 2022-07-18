typedef struct {
  char filename[40];
  size_t filesize;
} file_t;

FILE * pxFile;
size_t ulSize;

bool isMoreDataAvailable()
{
  return ulSize - ftell(pxFile);
};

byte getNextByte()
{
  byte cBuffer;
  fread(&cBuffer, 1, 1, pxFile);
  return cBuffer;
};
