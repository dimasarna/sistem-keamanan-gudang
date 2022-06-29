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

const uint16_t AVI_HEADER_SIZE = 252; // Size of the AVI file header.
const uint32_t TIMEOUT_DELAY = 15 * 1000; // Time (ms) after motion last detected that we keep recording
uint8_t LAST_SENSOR_STATE = LOW; // Variable for remembering last sensor state

const byte buffer00dc[4]  = {0x30, 0x30, 0x64, 0x63}; // "00dc"
const byte buffer0000[4]  = {0x00, 0x00, 0x00, 0x00}; // 0x00000000
const byte bufferAVI1[4]  = {0x41, 0x56, 0x49, 0x31}; // "AVI1"            
const byte bufferidx1[4]  = {0x69, 0x64, 0x78, 0x31}; // "idx1" 
                               
const byte aviHeader[AVI_HEADER_SIZE] = // This is the AVI file header.  Some of these values get overwritten.
{
  0x52, 0x49, 0x46, 0x46,  // 0x00 "RIFF"
  0x00, 0x00, 0x00, 0x00,  // 0x04            Total file size less 8 bytes [gets updated later] 
  0x41, 0x56, 0x49, 0x20,  // 0x08 "AVI "

  0x4C, 0x49, 0x53, 0x54,  // 0x0C "LIST"
  0x44, 0x00, 0x00, 0x00,  // 0x10 68       Structure length
  0x68, 0x64, 0x72, 0x6C,  // 0x04 "hdrl"

  0x61, 0x76, 0x69, 0x68,  // 0x08 "avih"       fcc
  0x38, 0x00, 0x00, 0x00,  // 0x0C 56            Structure length
  0x90, 0xD0, 0x03, 0x00,  // 0x20 250000    dwMicroSecPerFrame                 [based on FRAME_INTERVAL] 
  0x00, 0x00, 0x00, 0x00,  // 0x24                 dwMaxBytesPerSec                    [gets updated later] 
  0x00, 0x00, 0x00, 0x00,  // 0x28 0              dwPaddingGranularity
  0x10, 0x00, 0x00, 0x00,  // 0x2C 0x10        dwFlags - AVIF_HASINDEX set.
  0x00, 0x00, 0x00, 0x00,  // 0x30                 dwTotalFrames                           [gets updated later]
  0x00, 0x00, 0x00, 0x00,  // 0x34 0              dwInitialFrames (used for interleaved files only)
  0x01, 0x00, 0x00, 0x00,  // 0x38 1              dwStreams (just video)
  0x00, 0x00, 0x00, 0x00,  // 0x3C 0             dwSuggestedBufferSize
  0x40, 0x01, 0x00, 0x00,  // 0x40 320          dwWidth - 320 (QVGA)                [based on FRAMESIZE] 
  0xF0, 0x00, 0x00, 0x00,  // 0x44 240          dwHeight - 240 (VGA)                  [based on FRAMESIZE] 
  0x00, 0x00, 0x00, 0x00,  // 0x48                dwReserved
  0x00, 0x00, 0x00, 0x00,  // 0x4C                dwReserved
  0x00, 0x00, 0x00, 0x00,  // 0x50                dwReserved
  0x00, 0x00, 0x00, 0x00,  // 0x54                dwReserved

  0x4C, 0x49, 0x53, 0x54,  // 0x58 "LIST"
  0x84, 0x00, 0x00, 0x00,  // 0x5C 144
  0x73, 0x74, 0x72, 0x6C,  // 0x60 "strl"

  0x73, 0x74, 0x72, 0x68,  // 0x64 "strh"     Stream header
  0x30, 0x00, 0x00, 0x00,  // 0x68  48         Structure length
  0x76, 0x69, 0x64, 0x73,  // 0x6C "vids"     fccType - video stream
  0x4D, 0x4A, 0x50, 0x47,  // 0x70 "MJPG"   fccHandler - Codec
  0x00, 0x00, 0x00, 0x00,  // 0x74               dwFlags - not set
  0x00, 0x00,                     // 0x78               wPriority - not set
  0x00, 0x00,                     // 0x7A               wLanguage - not set
  0x00, 0x00, 0x00, 0x00,  // 0x7C               dwInitialFrames
  0x01, 0x00, 0x00, 0x00,  // 0x80 1             dwScale
  0x04, 0x00, 0x00, 0x00,  // 0x84 4             dwRate (frames per second)       [based on FRAME_INTERVAL]
  0x00, 0x00, 0x00, 0x00,  // 0x88               dwStart               
  0x00, 0x00, 0x00, 0x00,  // 0x8C               dwLength (frame count)              [gets updated later]
  0x00, 0x00, 0x00, 0x00,  // 0x90               dwSuggestedBufferSize
  0x00, 0x00, 0x00, 0x00,  // 0x94               dwQuality
  0x00, 0x00, 0x00, 0x00,  // 0x98               dwSampleSize

  0x73, 0x74, 0x72, 0x66,  // 0x9C "strf"    Stream format header
  0x28, 0x00, 0x00, 0x00,  // 0xA0 40        Structure length
  0x28, 0x00, 0x00, 0x00,  // 0xA4 40        BITMAPINFOHEADER length (same as above)
  0x40, 0x01, 0x00, 0x00,  // 0xA8 320      Width                  [based on FRAMESIZE] 
  0xF0, 0x00, 0x00, 0x00,  // 0xAC 240      Height                 [based on FRAMESIZE] 
  0x01, 0x00,                     // 0xB0 1          Planes  
  0x18, 0x00,                     // 0xB2 24        Bit count (bit depth once uncompressed)
  0x4D, 0x4A, 0x50, 0x47,  // 0xB4 "MJPG" Compression 
  0x00, 0x00, 0x00, 0x00,  // 0xB8 0          Size image (approx?)                              [what is this?]
  0x00, 0x00, 0x00, 0x00,  // 0xBC             X pixels per metre 
  0x00, 0x00, 0x00, 0x00,  // 0xC0             Y pixels per metre
  0x00, 0x00, 0x00, 0x00,  // 0xC4             Colour indices used  
  0x00, 0x00, 0x00, 0x00,  // 0xC8             Colours considered important (0 all important).


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
  0x00, 0x00, 0x00, 0x00, // 0xF4               Total size of frames        [gets updated later]
  0x6D, 0x6F, 0x76, 0x69  // 0xF8 "movi"
};
                                       
// The following relate to the AVI file that gets created.
uint16_t fileFramesCaptured = 0; // Number of frames captured by camera.
uint16_t fileFramesWritten = 0; // Number of frames written to the AVI file.
uint32_t fileFramesTotalSize = 0; // Total size of frames in file.
uint32_t fileStartTime = 0; // Used to calculate FPS. 
uint32_t filePadding = 0; // Total padding in the file.  

// These 2 variable conrtol the camera, and the actions required each processing loop. 
bool motionDetected = false; // This is set when motion is detected. It stays set for MOTION_DELAY ms after motion stops.
bool fileOpen = false; // This is set when we have an open AVI file.

FILE *aviFile; // AVI file
FILE *idx1File; // Temporary file used to hold the index information
char AVIFilename[40]; // Filename string

FileMessage AFileMessage; // File details wrapper

enum relative // Used when setting position within a file stream.
{
  FROM_START,
  FROM_CURRENT,
  FROM_END
};

// Pointer to frame buffer
camera_fb_t * frame = NULL;

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
    _PL("Unable to write idx1 chunk header to AVI file");
    return;
  }


  // Write the chunk size to the file.
  bytesWritten = writeLittleEndian((uint32_t)fileFramesWritten * 16, aviFile, 0x00, FROM_CURRENT);
  if (bytesWritten != 4)
  {
    _PL("Unable to write idx1 size to AVI file");
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
      _PL("Unable to read from idx file");
      return;
    }
    
    // Write the subchunk header 00dc
    bytesWritten = fwrite(buffer00dc, 1, 4, aviFile);
    if (bytesWritten != 4)
    {
      _PL("Unable to write 00dc to AVI file idx");
      return;
    }

    // Write the subchunk flags
    bytesWritten = fwrite(buffer0000, 1, 4, aviFile);
    if (bytesWritten != 4)
    {
      _PL("Unable to write flags to AVI file idx");
      return;
    }

    // Write the offset & size
    bytesWritten = fwrite(readBuffer, 1, 8, aviFile);
    if (bytesWritten != 8)
    {
      _PL("Unable to write offset & size to AVI file idx");
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
  memset(AVIFilename, 0, 40);
//  uint16_t fileNumber = 0;

    
  // Reset file statistics.
  fileFramesCaptured = 0;
  fileFramesTotalSize = 0;  
  fileFramesWritten = 0; 
  filePadding = 0;
  fileStartTime = millis();


  // Get the last file number used from the EEPROM.
//  EEPROM.get(0, fileNumber);

  
  // Increment the file number, and format the new file name.
//  fileNumber++;
  strcat(AVIFilename, "/sdcard/VID_");
  strcat(AVIFilename, rtc.getTime("%Y%m%d_%H%M%S").c_str());
  strcat(AVIFilename, ".avi");
    
  
  // Open the AVI file.
  aviFile = fopen(AVIFilename, "w");
  if (aviFile == NULL)  
  {
    _PP("Unable to open AVI file ");
    _PL(AVIFilename);
    return;  
  }  
  else
  {
    _PP(AVIFilename);
    _PL(" opened.");
  }
  
  
  // Write the AVI header to the file.
  size_t written = fwrite(aviHeader, 1, AVI_HEADER_SIZE, aviFile);
  if (written != AVI_HEADER_SIZE)
  {
    _PL("Unable to write header to AVI file");
    return;
   }


  // Update the EEPROM with the new file number.
//  EEPROM.put(0, fileNumber);
//  EEPROM.commit();


  // Open the idx1 temporary file.  This is read/write because we read back in after writing.
  idx1File = fopen("/sdcard/idx1.tmp", "w+");
  if (idx1File == NULL)  
  {
    _PL("Unable to open idx1 file for read/write");
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
  frame = esp_camera_fb_get();


  // Keep track of the total frames captured and total size of frames (needed to update file header later).
  fileFramesCaptured++;
  fileFramesTotalSize += frame->len;


  // Calculate if a padding byte is required (frame chunks need to be an even number of bytes).
  uint8_t paddingByte = frame->len & 0x00000001;
  

  // Keep track of the current position in the file relative to the start of the movi section.  This is used to update the idx1 file.
  uint32_t frameOffset = ftell(aviFile) - AVI_HEADER_SIZE;

  
  // Add the chunk header "00dc" to the file.
  bytesWritten = fwrite(buffer00dc, 1, 4, aviFile); 
  if (bytesWritten != 4)
  {
    _PL("Unable to write 00dc header to AVI file");
    return;
  }


  // Add the frame size to the file (including padding).
  uint32_t frameSize = frame->len + paddingByte;  
  bytesWritten = writeLittleEndian(frameSize, aviFile, 0x00, FROM_CURRENT);
  if (bytesWritten != 4)
  {
    _PL("Unable to write frame size to AVI file");
    return;
  }
  

  // Write the frame from the camera.
  bytesWritten = fwrite(frame->buf, 1, frame->len, aviFile);
  if (bytesWritten != frame->len)
  {
    _PL("Unable to write frame to AVI file");
    return;
  }

    
  // Release this frame from memory.
  esp_camera_fb_return(frame);   


  // The frame from the camera contains a chunk header of JFIF (bytes 7-10) that we want to replace with AVI1.
  // So we move the write head back to where the frame was just written + 6 bytes. 
  fseek(aviFile, (bytesWritten - 6) * -1, SEEK_END);
  

  // Then overwrite with the new chunk header value of AVI1.
  bytesWritten = fwrite(bufferAVI1, 1, 4, aviFile);
  if (bytesWritten != 4)
  {
    _PL("Unable to write AVI1 to AVI file");
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
      _PL("Unable to write padding to AVI file");
      return;
    }
  }


  // Write the frame offset to the idx1 file for this frame (used later).
  bytesWritten = writeLittleEndian(frameOffset, idx1File, 0x00, FROM_CURRENT);
  if (bytesWritten != 4)
  {
    _PL("Unable to write frame offset to idx1 file");
    return;
  } 


  // Write the frame size to the idx1 file for this frame (used later).
  bytesWritten = writeLittleEndian(frameSize - paddingByte, idx1File, 0x00, FROM_CURRENT);
  if (bytesWritten != 4)
  {
    _PL("Unable to write frame size to idx1 file");
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
  
  
  fclose(aviFile);
  
  _PP("File closed, size: ");
  _PL(AVI_HEADER_SIZE + fileFramesWritten * 8 + fileFramesTotalSize + filePadding + (8 + 16 * fileFramesWritten));

  // Send file to queue
  strcpy(AFileMessage.fileName, AVIFilename);
  AFileMessage.fileSize = AVI_HEADER_SIZE + fileFramesWritten * 8 + fileFramesTotalSize + filePadding + (8 + 16 * fileFramesWritten);
  xQueueSend(xFileQueue, (void *)&AFileMessage, 0);

}
