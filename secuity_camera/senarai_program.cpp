/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////// security_camera.ino /////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_http_server.h"
#include "FS.h"
#include "SD_MMC.h"
#include "defines.h"
#include <BlynkSimpleEsp32.h>
#include <UniversalTelegramBot.h>

// Timezone definition
#include <time.h>
#define ASIA_JAKARTA_TZ "WIB-7"

const uint8_t cSENSOR_PIN = 3;  // GPIO3 connected to the PIR sensor.
const uint8_t cBUZZER_PIN = 12; // GPIO12 connected to buzzer.

// PWM config for alarm buzzer
uint32_t lFrequencies   = 2000;
uint8_t cPWMChannel = 15, cResolutionBits = 8;

char cBlynkAuth[] = "y4DOipTTARN4jWDCzijtaA1fA54_y5Jq";
char cBlynkHost[] = "18.141.174.184";
uint16_t usBlynkPort = 8080;

// Telegram token
#define TELEGRAM_TOKEN "5593489325:AAHkoog4XIb2hLRbuLtPXWIjIORKMjrfuw4"
// Telegram channel
#define NOTIF_CHANNEL "@pusat_notifikasi"
#define FILE_CHANNEL "@penyimpanan_file"

WiFiClientSecure xSecureClient;
UniversalTelegramBot bot(TELEGRAM_TOKEN, xSecureClient);

//SemaphoreHandle_t xFrameSync;
QueueHandle_t xMessageQueue, xFileQueue;

#include "sd_module.h"
#include "camera_module.h"
#include "server_module.h"
#include "wifi_module.h"
#include "avi_program.h"
#include "rtos_callback.h"

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  pinMode(cSENSOR_PIN, INPUT);
  ledcSetup(cPWMChannel, lFrequencies, cResolutionBits);
  ledcAttachPin(cBUZZER_PIN, cPWMChannel);
  
  Serial.begin(115200);
  Serial.setDebugOutput(false);

  delay(20); Serial.println();

  //xFrameSync = xSemaphoreCreateBinary();
  //xSemaphoreGive(xFrameSync);
  
  xMessageQueue = xQueueCreate( 20, sizeof( char ) * 100 );
  xFileQueue = xQueueCreate( 10, sizeof( file_t ) );

  vMountSDCard();
  vStartCamera();
  vWiFiSetup();
  
  // Get time
  Serial.println("Retrieving time...");
  configTzTime(ASIA_JAKARTA_TZ, "time.google.com", "time.windows.com", "pool.ntp.org");
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
  
  // Start streaming server
  vStartServer();

  // Add root certificate for api.telegram.org
  xSecureClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  // Initiate blynk configuration
  Blynk.config(cBlynkAuth, cBlynkHost, usBlynkPort);

  // It needs for PIR to acclimatize
  Serial.print("Sensor is acclimatizing... ");
  delay(30000);
  Serial.println("done");

  xTaskCreatePinnedToCore( vBlynkRun, "BlynkRun", 4096,  NULL, tskIDLE_PRIORITY+15, NULL, 1 );
  xTaskCreatePinnedToCore( vBotTask, "BotTask", 6144, NULL, tskIDLE_PRIORITY+2, NULL, 1 );
  xTaskCreatePinnedToCore( vSensorTask, "SensorTask", 2048, NULL, tskIDLE_PRIORITY+3, NULL, 0 );
  xTaskCreatePinnedToCore( vRecordTask, "RecordTask", 10240, NULL, tskIDLE_PRIORITY+3, NULL, 1 );
}

void loop() {
  delay(60000);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////// avi_program.h ////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//AVI RIFF Form
//
//RIFF ('AVI '
//      LIST ('hdrl'
//            'avih'(<Main AVI Header>)
//            LIST ('strl'
//                  'strh'(<Stream header>)
//                  'strf'(<Stream format>)
//                  [ 'strd'(<Additional header data>) ]
//                  [ 'strn'(<Stream name>) ]
//                  ...
//                 )
//             ...
//           )
//      LIST ('movi'
//            {SubChunk | LIST ('rec '
//                              SubChunk1
//                              SubChunk2
//                              ...
//                             )
//               ...
//            }
//            ...
//           )
//      ['idx1' (<AVI Index>) ]
//     )

const uint16_t      AVI_HEADER_SIZE = 252;            // Size of the AVI file header.
const long unsigned TIMEOUT_DELAY   = 15000;          // Time (ms) after motion last detected that we keep recording

const byte buffer00dc[4]  = {0x30, 0x30, 0x64, 0x63}; // "00dc"
const byte buffer0000[4]  = {0x00, 0x00, 0x00, 0x00}; // 0x00000000
const byte bufferAVI1[4]  = {0x41, 0x56, 0x49, 0x31}; // "AVI1"            
const byte bufferidx1[4]  = {0x69, 0x64, 0x78, 0x31}; // "idx1" 
                               
const byte aviHeader[AVI_HEADER_SIZE] =               // This is the AVI file header.  Some of these values get overwritten.
{
  0x52, 0x49, 0x46, 0x46,  // 0x00 "RIFF"
  0x00, 0x00, 0x00, 0x00,  // 0x04            Total file size less 8 bytes [gets updated later] 
  0x41, 0x56, 0x49, 0x20,  // 0x08 "AVI "

  0x4C, 0x49, 0x53, 0x54,  // 0x0C "LIST"
  0x44, 0x00, 0x00, 0x00,  // 0x10 68         Structure length
  0x68, 0x64, 0x72, 0x6C,  // 0x04 "hdrl"

  0x61, 0x76, 0x69, 0x68,  // 0x08 "avih"     fcc
  0x38, 0x00, 0x00, 0x00,  // 0x0C 56         Structure length
  0x90, 0xD0, 0x03, 0x00,  // 0x20 200000     dwMicroSecPerFrame                  [based on FRAME_INTERVAL] 
  0x00, 0x00, 0x00, 0x00,  // 0x24            dwMaxBytesPerSec                    [gets updated later] 
  0x00, 0x00, 0x00, 0x00,  // 0x28 0          dwPaddingGranularity
  0x10, 0x00, 0x00, 0x00,  // 0x2C 0x10       dwFlags - AVIF_HASINDEX set.
  0x00, 0x00, 0x00, 0x00,  // 0x30            dwTotalFrames                       [gets updated later]
  0x00, 0x00, 0x00, 0x00,  // 0x34 0          dwInitialFrames (used for interleaved files only)
  0x01, 0x00, 0x00, 0x00,  // 0x38 1          dwStreams (just video)
  0x00, 0x00, 0x00, 0x00,  // 0x3C 0          dwSuggestedBufferSize
  0x40, 0x01, 0x00, 0x00,  // 0x40 320        dwWidth - 320 (QVGA)                [based on FRAMESIZE] 
  0xF0, 0x00, 0x00, 0x00,  // 0x44 240        dwHeight - 240 (VGA)                [based on FRAMESIZE] 
  0x00, 0x00, 0x00, 0x00,  // 0x48            dwReserved
  0x00, 0x00, 0x00, 0x00,  // 0x4C            dwReserved
  0x00, 0x00, 0x00, 0x00,  // 0x50            dwReserved
  0x00, 0x00, 0x00, 0x00,  // 0x54            dwReserved

  0x4C, 0x49, 0x53, 0x54,  // 0x58 "LIST"
  0x84, 0x00, 0x00, 0x00,  // 0x5C 144
  0x73, 0x74, 0x72, 0x6C,  // 0x60 "strl"

  0x73, 0x74, 0x72, 0x68,  // 0x64 "strh"        Stream header
  0x30, 0x00, 0x00, 0x00,  // 0x68  48           Structure length
  0x76, 0x69, 0x64, 0x73,  // 0x6C "vids"        fccType - video stream
  0x4D, 0x4A, 0x50, 0x47,  // 0x70 "MJPG"        fccHandler - Codec
  0x00, 0x00, 0x00, 0x00,  // 0x74               dwFlags - not set
  0x00, 0x00,              // 0x78               wPriority - not set
  0x00, 0x00,              // 0x7A               wLanguage - not set
  0x00, 0x00, 0x00, 0x00,  // 0x7C               dwInitialFrames
  0x01, 0x00, 0x00, 0x00,  // 0x80 1             dwScale
  0x04, 0x00, 0x00, 0x00,  // 0x84 4             dwRate (frames per second)       [based on FRAME_INTERVAL]
  0x00, 0x00, 0x00, 0x00,  // 0x88               dwStart               
  0x00, 0x00, 0x00, 0x00,  // 0x8C               dwLength (frame count)           [gets updated later]
  0x00, 0x00, 0x00, 0x00,  // 0x90               dwSuggestedBufferSize
  0x00, 0x00, 0x00, 0x00,  // 0x94               dwQuality
  0x00, 0x00, 0x00, 0x00,  // 0x98               dwSampleSize

  0x73, 0x74, 0x72, 0x66,  // 0x9C "strf"    Stream format header
  0x28, 0x00, 0x00, 0x00,  // 0xA0 40        Structure length
  0x28, 0x00, 0x00, 0x00,  // 0xA4 40        BITMAPINFOHEADER length (same as above)
  0x40, 0x01, 0x00, 0x00,  // 0xA8 320       Width                  [based on FRAMESIZE] 
  0xF0, 0x00, 0x00, 0x00,  // 0xAC 240       Height                 [based on FRAMESIZE] 
  0x01, 0x00,              // 0xB0 1         Planes  
  0x18, 0x00,              // 0xB2 24        Bit count (bit depth once uncompressed)
  0x4D, 0x4A, 0x50, 0x47,  // 0xB4 "MJPG"    Compression 
  0x00, 0x00, 0x00, 0x00,  // 0xB8 0         Size image (approx?)   [what is this?]
  0x00, 0x00, 0x00, 0x00,  // 0xBC           X pixels per metre 
  0x00, 0x00, 0x00, 0x00,  // 0xC0           Y pixels per metre
  0x00, 0x00, 0x00, 0x00,  // 0xC4           Colour indices used  
  0x00, 0x00, 0x00, 0x00,  // 0xC8           Colours considered important (0 all important).


  0x49, 0x4E, 0x46, 0x4F, // 0xCB "INFO"
  0x1C, 0x00, 0x00, 0x00, // 0xD0 28         Structure length
  0x70, 0x61, 0x75, 0x6c, // 0xD4 
  0x2e, 0x77, 0x2e, 0x69, // 0xD8 
  0x62, 0x62, 0x6f, 0x74, // 0xDC 
  0x73, 0x6f, 0x6e, 0x40, // 0xE0 
  0x67, 0x6d, 0x61, 0x69, // 0xE4 
  0x6c, 0x2e, 0x63, 0x6f, // 0xE8 
  0x6d, 0x00, 0x00, 0x00, // 0xEC 

  0x4C, 0x49, 0x53, 0x54, // 0xF0 "LIST"
  0x00, 0x00, 0x00, 0x00, // 0xF4            Total size of frames        [gets updated later]
  0x6D, 0x6F, 0x76, 0x69  // 0xF8 "movi"
};

camera_fb_t *frameBuffer;               // This is where we hold references to the captured frames in a circular buffer.
                                        // typedef struct 
                                        // {
                                        //   uint8_t *buf;         Pointer to the pixel data 
                                        //   size_t len;           Length of the buffer in bytes 
                                        //   size_t width;         Width of the buffer in pixels 
                                        //   size_t height;        Height of the buffer in pixels 
                                        //   pixformat_t format;   Format of the pixel data 
                                        // } camera_fb_t;
                                       
                                        // The following relate to the AVI file that gets created.
uint16_t fileFramesCaptured  = 0;       // Number of frames captured by camera.
uint16_t fileFramesWritten   = 0;       // Number of frames written to the AVI file.
uint32_t fileFramesTotalSize = 0;       // Total size of frames in file.
uint32_t fileStartTime       = 0;       // Used to calculate FPS. 
uint32_t filePadding         = 0;       // Total padding in the file.  

                                        // These 2 variable conrtol the camera, and the actions required each processing loop. 
bool motionDetected  = false;           // This is set when motion is detected.  It stays set for TIMEOUT_DELAY ms after motion stops.
bool fileOpen        = false;           // This is set when we have an open AVI file.

FILE   *aviFile;                        // AVI file
FILE   *idx1File;                       // Temporary file used to hold the index information 
char   AVIFilename[30];                 // Filename string
size_t AVIFilesize = 0;                 // Size of file in bytes

enum relative                           // Used when setting position within a file stream.
{
  FROM_START,
  FROM_CURRENT,
  FROM_END
};

file_t xFile;                           // Used for sending file to queue

// ------------------------------------------------------------------------------------------
// Write a 32 bit value in little endian format (LSB first) to file at specific location.
// ------------------------------------------------------------------------------------------

uint8_t writeLittleEndian(uint32_t value, FILE *file, int32_t offset, relative position)
{
  uint8_t digit[1];
  uint8_t writeCount = 0;

  
  // Set position within file.  Either relative to: SOF, current position, or EOF.
  if (position == FROM_START)
    fseek(file, offset, SEEK_SET);    // offset >= 0
  else if (position == FROM_CURRENT)
    fseek(file, offset, SEEK_CUR);    // Offset > 0, < 0, or 0
  else if (position == FROM_END)
    fseek(file, offset, SEEK_END);    // offset <= 0 ??
  else
    return 0;  


  // Write the value to the file a byte at a time (LSB first).
  for (uint8_t x = 0; x < 4; x++)
  {
    digit[0] = value % 0x100;
    writeCount = writeCount + fwrite(digit, 1, 1, file);
    value = value >> 8;
  }


  // Return the number of bytes written to the file.
  return writeCount;
}

// ----------------------------------------------------------------------------------
// Routine to add the idx1 (frame index) chunk to the end of the file.  
// ----------------------------------------------------------------------------------

void writeIdx1Chunk()
{
  // The idx1 chunk consists of:
  //  typedef struct {
  //  fcc         FOURCC 'idx1'
  //  cb          DWORD  length not including first 8 bytes
  //    typedef struct {
  //      dwChunkId DWORD  '00dc' StreamID = 00, Type = dc (compressed video frame)
  //      dwFlags   DWORD  '0000'  dwFlags - none set
  //      dwOffset  DWORD   Offset from movi for this frame
  //      dwSize    DWORD   Size of this frame
  //    } AVIINDEXENTRY;
  //  } CHUNK;
  // The offset & size of each frame are read from the idx1.tmp file that we created
  // earlier when adding each frame to the main file.
  // 
  size_t bytesWritten = 0;


  // Write the idx1 header to the file
  bytesWritten = fwrite(bufferidx1, 1, 4, aviFile);
  if (bytesWritten != 4)
  {
    Serial.println("Unable to write idx1 chunk header to AVI file");
    return;
  }


  // Write the chunk size to the file.
  bytesWritten = writeLittleEndian((uint32_t)fileFramesWritten * 16, aviFile, 0x00, FROM_CURRENT);
  if (bytesWritten != 4)
  {
    Serial.println("Unable to write idx1 size to AVI file");
    return;
  }


  // We need to read the idx1 file back in, so move the read head to the start of the idx1 file.
  fseek(idx1File, 0, SEEK_SET);
  
  
  // For each frame, write a sub chunk to the AVI file (offset & size are read from the idx file)
  char readBuffer[8];
  for (uint32_t x = 0; x < fileFramesWritten; x++)
  {
    // Read the offset & size from the idx file.
    bytesWritten = fread(readBuffer, 1, 8, idx1File);
    if (bytesWritten != 8)
    {
      Serial.println("Unable to read from idx file");
      return;
    }
    
    // Write the subchunk header 00dc
    bytesWritten = fwrite(buffer00dc, 1, 4, aviFile);
    if (bytesWritten != 4)
    {
      Serial.println("Unable to write 00dc to AVI file idx");
      return;
    }

    // Write the subchunk flags
    bytesWritten = fwrite(buffer0000, 1, 4, aviFile);
    if (bytesWritten != 4)
    {
      Serial.println("Unable to write flags to AVI file idx");
      return;
    }

    // Write the offset & size
    bytesWritten = fwrite(readBuffer, 1, 8, aviFile);
    if (bytesWritten != 8)
    {
      Serial.println("Unable to write offset & size to AVI file idx");
      return;
    }
  }


  // Close the idx1 file.
  fclose(idx1File);
  
}

// ------------------------------------------------------------------------------------------
// Routine to create a new AVI file each time motion is detected.
// ------------------------------------------------------------------------------------------

void startFile()
{   
  // Reset file statistics.
  fileFramesCaptured  = 0;
  fileFramesTotalSize = 0;  
  fileFramesWritten   = 0; 
  filePadding         = 0;
  fileStartTime       = millis();

  
  // Format the new file name.
  char now[16];
  time_t t = time(nullptr);
  strftime(now, sizeof(now), "%Y%m%d_%H%M%S", localtime(&t));
  strcpy(AVIFilename, "VID_");
  strcat(AVIFilename, now);
  strcat(AVIFilename, ".avi");
    
  
  // Open the AVI file.
  char path[40];
  strcpy(path, "/sdcard/");
  strcat(path, AVIFilename);
  aviFile = fopen(path, "w");
  if (aviFile == NULL)  
  {
    Serial.print("Unable to open AVI file ");
    Serial.println(AVIFilename);
    return;  
  }
  
  
  // Write the AVI header to the file.
  size_t written = fwrite(aviHeader, 1, AVI_HEADER_SIZE, aviFile);
  if (written != AVI_HEADER_SIZE)
  {
    Serial.println("Unable to write header to AVI file");
    return;
   }


  // Open the idx1 temporary file.  This is read/write because we read back in after writing.
  idx1File = fopen("/sdcard/idx1.tmp", "w+");
  if (idx1File == NULL)  
  {
    Serial.println("Unable to open idx1 file for read/write");
    return;  
  }  


  // Set the flag to indicate we are ready to start recording.  
  fileOpen = true;
}

// ------------------------------------------------------------------------------------------
// Routine to add a frame to the AVI file.  Should only be called when framesInBuffer() > 0, 
// and there is already a file open.
// ------------------------------------------------------------------------------------------

void addToFile()
{
  // For each frame we write a chunk to the AVI file made up of:
  //  "00dc" - chunk header.  Stream ID (00) & type (dc = compressed video)
  //  The size of the chunk (frame size + padding)
  //  The frame from camera frame buffer
  //  Padding (0x00) to ensure an even number of bytes in the chunk.  
  // 
  // We then update the FOURCC in the frame from JFIF to AVI1  
  //
  // We also write to the temporary idx file.  This keeps track of the offset & size of each frame.
  // This is read back later (before we close the AVI file) to update the idx1 chunk.
  
  size_t bytesWritten;
  frameBuffer = esp_camera_fb_get();
  // xSemaphoreTake(xFrameSync, portMAX_DELAY);


  // Keep track of the total frames captured and total size of frames (needed to update file header later).
  fileFramesCaptured++;
  fileFramesTotalSize += frameBuffer->len;


  // Calculate if a padding byte is required (frame chunks need to be an even number of bytes).
  uint8_t paddingByte = frameBuffer->len & 0x00000001;
  

  // Keep track of the current position in the file relative to the start of the movi section.  This is used to update the idx1 file.
  uint32_t frameOffset = ftell(aviFile) - AVI_HEADER_SIZE;

  
  // Add the chunk header "00dc" to the file.
  bytesWritten = fwrite(buffer00dc, 1, 4, aviFile); 
  if (bytesWritten != 4)
  {
    Serial.println("Unable to write 00dc header to AVI file");
    return;
  }


  // Add the frame size to the file (including padding).
  uint32_t frameSize = frameBuffer->len + paddingByte;  
  bytesWritten = writeLittleEndian(frameSize, aviFile, 0x00, FROM_CURRENT);
  if (bytesWritten != 4)
  {
    Serial.println("Unable to write frame size to AVI file");
    return;
  }
  

  // Write the frame from the camera.
  bytesWritten = fwrite(frameBuffer->buf, 1, frameBuffer->len, aviFile);
  if (bytesWritten != frameBuffer->len)
  {
    Serial.println("Unable to write frame to AVI file");
    return;
  }

    
  // Release this frame from memory.
  esp_camera_fb_return(frameBuffer);
  // xSemaphoreGive(xFrameSync);


  // The frame from the camera contains a chunk header of JFIF (bytes 7-10) that we want to replace with AVI1.
  // So we move the write head back to where the frame was just written + 6 bytes. 
  fseek(aviFile, (bytesWritten - 6) * -1, SEEK_END);
  

  // Then overwrite with the new chunk header value of AVI1.
  bytesWritten = fwrite(bufferAVI1, 1, 4, aviFile);
  if (bytesWritten != 4)
  {
    Serial.println("Unable to write AVI1 to AVI file");
    return;
  }

 
  // Move the write head back to the end of the file.
  fseek(aviFile, 0, SEEK_END);

    
  // If required, add the padding to the file.
  if(paddingByte > 0)
  {
    bytesWritten = fwrite(buffer0000, 1, paddingByte, aviFile); 
    if (bytesWritten != paddingByte)
    {
      Serial.println("Unable to write padding to AVI file");
      return;
    }
  }


  // Write the frame offset to the idx1 file for this frame (used later).
  bytesWritten = writeLittleEndian(frameOffset, idx1File, 0x00, FROM_CURRENT);
  if (bytesWritten != 4)
  {
    Serial.println("Unable to write frame offset to idx1 file");
    return;
  }


  // Write the frame size to the idx1 file for this frame (used later).
  bytesWritten = writeLittleEndian(frameSize - paddingByte, idx1File, 0x00, FROM_CURRENT);
  if (bytesWritten != 4)
  {
    Serial.println("Unable to write frame size to idx1 file");
    return;
  }

  
  // Increment the frames written count, and keep track of total padding.
  fileFramesWritten++;
  filePadding = filePadding + paddingByte;
}

// ------------------------------------------------------------------------------------------
// Once motion stops we update the file totals, write the idx1 chunk, and close the file.
// ------------------------------------------------------------------------------------------

void closeFile()
{
  // Update the flag immediately to prevent any further frames getting written to the buffer.
  fileOpen = false;

  
  // Calculate how long the AVI file runs for.
  unsigned long fileDuration = (millis() - fileStartTime) / 1000UL;

 
  // Update AVI header with total file size. This is the sum of:
  //   AVI header (252 bytes less the first 8 bytes)
  //   fileFramesWritten * 8 (extra chunk bytes for each frame)
  //   fileFramesTotalSize (frames from the camera)
  //   filePadding
  //   idx1 section (8 + 16 * fileFramesWritten)
  writeLittleEndian((AVI_HEADER_SIZE - 8) + fileFramesWritten * 8 + fileFramesTotalSize + filePadding + (8 + 16 * fileFramesWritten), aviFile, 0x04, FROM_START);


  // Update the AVI header with maximum bytes per second.
  uint32_t maxBytes = fileFramesTotalSize / fileDuration;  
  writeLittleEndian(maxBytes, aviFile, 0x24, FROM_START);
  

  // Update AVI header with total number of frames.
  writeLittleEndian(fileFramesWritten, aviFile, 0x30, FROM_START);
  
  
  // Update stream header with total number of frames.
  writeLittleEndian(fileFramesWritten, aviFile, 0x8C, FROM_START);


  // Update movi section with total size of frames.  This is the sum of:
  //   fileFramesWritten * 8 (extra chunk bytes for each frame)
  //   fileFramesTotalSize (frames from the camera)
  //   filePadding
  writeLittleEndian(fileFramesWritten * 8 + fileFramesTotalSize + filePadding, aviFile, 0xF4, FROM_START);


  // Move the write head back to the end of the AVI file.
  fseek(aviFile, 0, SEEK_END);

   
  // Add the idx1 section to the end of the AVI file
  writeIdx1Chunk();
  
  
  // Close file
  fclose(aviFile);
  AVIFilesize = AVI_HEADER_SIZE + fileFramesWritten * 8 + fileFramesTotalSize + filePadding + (8 + 16 * fileFramesWritten);

  
  // Send filename and filesize to queue
  strcpy(xFile.filename, AVIFilename);
  xFile.filesize = AVIFilesize;
  xQueueSend(xFileQueue, (void *)&xFile, 0);
  
  // Print file information
  Serial.printf("File created: %s\n", AVIFilename);
  Serial.printf("Size: %d bytes\n", AVIFilesize);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////// blynk_event.h ////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Helper function to convert IP address to string
String xIPToString(IPAddress ip) {
  String s = "";
  for (int i = 0; i < 4; i++) {
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  }
  return s;
};

void vSendIPToBlynk() {
  String url = "http://" + xIPToString(WiFi.localIP());
  Blynk.setProperty(V0, "url", url.c_str());
};

// Send IP address after connected to blynk
BLYNK_CONNECTED() {
  Blynk.syncAll();
  vSendIPToBlynk();
}

BLYNK_APP_CONNECTED() {
  vSendIPToBlynk();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////// camera_module.h ///////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void vStartCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 10;
  config.grab_mode = CAMERA_GRAB_LATEST;
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(5000);
    ESP.restart();
  }

  // Setting camera
  sensor_t * s = esp_camera_sensor_get();
  if (s == NULL) {
    Serial.println("Camera setting failed");
    delay(5000);
    ESP.restart();
  } else {
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////// defines.h //////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////// rtos_callback.h ///////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void vBlynkRun( void * pvParameters )
{
#define BLYNK_RUN_INTERVAL_MS    250L

  for (;;)
  {
    Blynk.run();
    vTaskDelay(BLYNK_RUN_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

void vBotTask( void * pvParameters )
{
  char RcMessage[100];
  file_t RxFile;

#define BOT_TASK_INTERVAL_MS    20L

  for (;;)
  {
    if ( xQueueReceive( xMessageQueue,
                        (void *)&RcMessage,
                        ( TickType_t ) 0 ) == pdPASS )
    {
      bot.sendMessage(NOTIF_CHANNEL, RcMessage, "");
      Serial.println("Message sent");
    }
    
    if ( xQueueReceive( xFileQueue,
                        (void *)&RxFile,
                        ( TickType_t ) 0 ) == pdPASS )
    {
      ulSize = RxFile.filesize;
      
      char path[40];
      strcpy(path, "/sdcard/");
      strcat(path, RxFile.filename);
      pxFile = fopen(path, "rb");
      
      if (pxFile == NULL) {
        Serial.println("Open file error");
        continue;
      }
      
      uint32_t ulStart = millis();
      bot.sendMultipartFormDataToTelegram("sendDocument", "document",
                                          RxFile.filename, "video/x-msvideo",
                                          FILE_CHANNEL, RxFile.filesize,
                                          isMoreDataAvailable,
                                          getNextByte, nullptr, nullptr);
      uint32_t ulEnd = millis();
      
      Serial.printf("File sent: %s\n", RxFile.filename);
      Serial.printf("Upload time: %d ms\n", ulEnd - ulStart);
      fclose(pxFile);
    }
    
    vTaskDelay(BOT_TASK_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

void vSensorTask( void *pvParameters )
{
  char TcMessage[100];
  uint32_t ulCurrentMS = 0; // Current time
  uint32_t ulLastMotionMS = 0; // Last time we detected movement
  uint8_t ucSensorCurrentState = LOW; // Keep recent sensor state
  uint8_t ucSensorLastState = LOW; // Variable for remembering last sensor state
  uint16_t usMotionCounter = 0; // Keep track of total motion detected

#define SENSOR_TASK_INTERVAL_MS    500L

  for (;;) {
    ulCurrentMS = millis();
    ucSensorCurrentState  = digitalRead(cSENSOR_PIN);
    
    if (ucSensorLastState == LOW && ucSensorCurrentState == HIGH) {
        ucSensorLastState = HIGH;
        
        // Motion is detected
        Serial.println("Motion detected");
        motionDetected = true;
        ulLastMotionMS = ulCurrentMS;
        usMotionCounter++;

        // Send message to queue
        if (usMotionCounter >= 5) {
          // Turn on alarm
          ledcWrite(cPWMChannel, 128);
          
          strcpy(TcMessage, "Bahaya!!\nTerdapat penyelundup.\nJumlah gerakan: ");
          strcat(TcMessage, String(usMotionCounter).c_str());
          xQueueSend(xMessageQueue, (void *)&TcMessage, 0);
        }
        else {
          strcpy(TcMessage, "Terdeteksi gerakan!\nMungkin hanya gangguan eksternal.\nJumlah gerakan: ");
          strcat(TcMessage, String(usMotionCounter).c_str());
          xQueueSend(xMessageQueue, (void *)&TcMessage, 0);
        }
        
    } else if (ucSensorLastState == HIGH && ucSensorCurrentState == LOW) {
        ucSensorLastState = LOW;
    }
    // Never any movement at startup
    else if (ulLastMotionMS == 0) {
      motionDetected = false;
    }
    // Recent movement
    else if (ulCurrentMS - ulLastMotionMS < TIMEOUT_DELAY) {
      motionDetected = true;
    } else {
      motionDetected = false;
      usMotionCounter = 0;
      
      // Turn off alarm
      ledcWrite(cPWMChannel, 0);
    }

    vTaskDelay(SENSOR_TASK_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

void vRecordTask( void *pvParameters )
{
#define RECORD_TASK_INTERVAL_MS    200L
  
  for (;;)
  {     
    // Once motion is detected we open a new file.
    if (motionDetected && !fileOpen)
      startFile();

    // If there are frames waiting to be processed add these to the file.
    if (motionDetected && fileOpen)
      addToFile();

    // Once motion stops, add any remaining frames to the file, and close the file.
    if (!motionDetected && fileOpen)
      closeFile();

    vTaskDelay(RECORD_TASK_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////// sd_module.h /////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void vMountSDCard()
{
  Serial.println("Mount SD Card file system...");
 
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Card mount failed");
    delay(5000);
    ESP.restart();
  }
  
  // Turn off flash
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  
  uint8_t cCardType = SD_MMC.cardType();
  if (cCardType == CARD_NONE) {
    Serial.println("No SD Card attached");
    delay(5000);
    ESP.restart();
  } else Serial.println("SD Card mounted");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////// server_module.h ///////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define PART_BOUNDARY "123456789000000000000987654321"
#define STREAM_INTERVAL_MS 20L

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;

static esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;
    char * part_buf[64];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    while(true){
        fb = esp_camera_fb_get();
        // xSemaphoreTake(xFrameSync, portMAX_DELAY);
        if(!fb){
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }
        if(fb->format != PIXFORMAT_JPEG){
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            if(!jpeg_converted){
                ESP_LOGE(TAG, "JPEG compression failed");
                esp_camera_fb_return(fb);
                res = ESP_FAIL;
            }
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);

            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(fb->format != PIXFORMAT_JPEG){
            free(_jpg_buf);
        }
        esp_camera_fb_return(fb);
        // xSemaphoreGive(xFrameSync);
        if(res != ESP_OK){
            break;
        }
        vTaskDelay(STREAM_INTERVAL_MS / portTICK_PERIOD_MS);
    }

    return res;
}

void vStartServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  
  Serial.printf("Starting web server on port: '%d'\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &index_uri);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////// wifi_module.h ////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Network authentication
const char cNETWORK_SSID[] = "Enggar";
const char cNETWORK_PASSWORD[] = "drenggarbaik";

void vWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
}

void vWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    Serial.print("WiFi lost connection. Reason: ");
    Serial.println(info.wifi_sta_disconnected.reason);
    WiFi.begin(cNETWORK_SSID, cNETWORK_PASSWORD);
}

void vWiFiSetup() {
    // delete old config
    WiFi.disconnect(true);

    WiFi.onEvent(vWiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(vWiFiDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.begin(cNETWORK_SSID, cNETWORK_PASSWORD);
    Serial.println("Wait for WiFi...");
    while (WiFi.status() != WL_CONNECTED) {}
}
