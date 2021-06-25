/* 
 *  
 * MIDI System Exclusive Implementation for the Nava
 * Manufacturer ID: 0x7D
 * DeviceID: 0x07, 0x1A (909 shifted 1 bit to the left to fit into the 7 data bits.
 * CommandID: See Sysex.h
 * CommandParameter: byte indicating pattern bank / track/ 0 if not used 
 * 
 */

#if MIDI_HAS_SYSEX
#include <Arduino_CRC32.h>
#include "Sysex.h"

Arduino_CRC32 crc; // See https://github.com/bakercp/CRC32 for info

#define CHECKSUMSIZE 8
/* Convert 8 bit data to 8 bytes containing 7 bits
 * (0 MSB7 MSB6 MSB5 MSB4 MSB3 MSB2 MSB1) 
 * (BYTE1 & 0x7F) (BYTE2 & 0x7F) (BYTE3 & 0x7F) 
 * (BYTE4 & 0x7F) (BYTE5 & 0x7F) (BYTE6 & 0x7F) 
 * (BYTE7 & 0x7F)  
 */
 
uint16_t data_to_sysex(uint8_t *data, uint8_t *sysex, uint16_t len) {
  uint16_t retlen = 0;
  uint16_t cnt;
  uint16_t cnt7 = 0;

  sysex[0] = 0;
  for (cnt = 0; cnt < len; cnt++) {
    uint8_t c = data[cnt] & 0x7F;
    uint8_t msb = data[cnt] >> 7;
    sysex[0] |= msb << cnt7;
    sysex[1 + cnt7] = c;

    if (cnt7++ == 6) {
      sysex += 8;
      retlen += 8;
      sysex[0] = 0;
      cnt7 = 0;
    }
  }
  return retlen + cnt7 + (cnt7 != 0 ? 1 : 0);
}

/* Convert the sysex data back to normal data */
uint16_t sysex_to_data(uint8_t *sysex, uint8_t *data, uint16_t len) {
  uint16_t cnt;
  uint16_t cnt2 = 0;
  uint8_t bits = 0;
  for (cnt = 0; cnt < len; cnt++) {
    if ((cnt % 8) == 0) {
      bits = sysex[cnt];
    } else {
      data[cnt2++] = sysex[cnt] | ((bits & 1) << 7);
      bits >>= 1;
    }
  }
  return cnt2;
}

uint16_t build_sysex(uint8_t *sysex, uint8_t *data, uint16_t datasize, uint8_t sysex_type, uint8_t param)
{     
  char header[]={ START_OF_SYSEX, SYSEX_MANUFACTURER, SYSEX_DEVID_1, SYSEX_DEVID_2, sysex_type, param};
  uint16_t sysexSize = 0;
  uint8_t maxBlocks = 1.0f;
  uint8_t blckCnt = 0;
  uint16_t blockSize = 0;
  uint16_t txSize = 0;
  // Copy the header to the buffer
  memcpy(sysex, header, sizeof(header));

  // Convert data for 7 bit transfers
  uint16_t sysExDataSize = data_to_sysex(data,sysex+sizeof(header),datasize);
#ifdef DEBUG
  Serial.print("sysExDataSize: "); Serial.println(sysExDataSize);
#endif
  if ( sysExDataSize > 256 )
  {
    maxBlocks = sysExDataSize / 256;
    blockSize = 256;
    Serial.print("maxBlocks: "); Serial.println(maxBlocks);
  } else {
    txSize = blockSize = sysExDataSize;
  }
  
  for(int blocks = 0; blocks < maxBlocks; blocks++)
  {  
    if ( sysExDataSize % 256 == 255 )
    {
      blckCnt++;
      sysex[6] = blckCnt;
      if ( txSize + blockSize > sysExDataSize )
      {
        blockSize = sysExDataSize - txSize; 
      }
      txSize += blockSize;     
    }
    Serial.print("Current Block: "); Serial.println(sysex[6]);
    Serial.print("Block Size: "); Serial.println(blockSize);
    
    sysexSize=HEADERSIZE + blockSize;
    
    // Calculate checksum for the sysex
    uint32_t checksum = crc.calc(sysex, blockSize);
    
    // Add the crc and end of sysex at the end
    Serial.print("Checksum pos: "); Serial.println(blockSize);
    sysex[sysexSize++] = (checksum >> 28) & 0xf;
    sysex[sysexSize++] = (checksum >> 24) & 0xf;
    sysex[sysexSize++] = (checksum >> 20) & 0xf;
    sysex[sysexSize++] = (checksum >> 16) & 0xf;
    sysex[sysexSize++] = (checksum >> 12) & 0xf;
    sysex[sysexSize++] = (checksum >> 8) & 0xf;
    sysex[sysexSize++] = (checksum >> 4) & 0xf;
    sysex[sysexSize++] = checksum & 0xf;
    sysex[sysexSize++]=END_OF_SYSEX;
    Serial.print("sysexSize Size: "); Serial.println(sysexSize);
    
#ifdef DEBUG
    Serial.println();
    Serial.println("Checksum: ");Serial.println(checksum,HEX);
#endif
    // Transmit system exclusive block
    PrintSysex(sysex, blockSize);
    MIDI.sendSysEx(blockSize,sysex,true);
  }    
  return(sysexSize+1);
}

void DumpPattern(byte patternNbr)
{
  byte RawData[PTRN_SIZE];
  Serial.print("Pattern Rawdata size: "); Serial.println(PTRN_SIZE);
  memory("Dump Pattern");
  int datacount = 0;

  unsigned long adress = (unsigned long)(PTRN_OFFSET + patternNbr * PTRN_SIZE);
  WireBeginTX(adress); 
  Wire.endTransmission();
  Wire.requestFrom(HRDW_ADDRESS,MAX_PAGE_SIZE); //request a 64 bytes page
  //TRIG-----------------------------------------------
  for(int i =0; i<NBR_INST;i++){
    RawData[datacount] = (unsigned long)((Wire.read() & 0xFF) | (( Wire.read() << 8) & 0xFF00));
    // Serial.println(Wire.read());
    datacount++;
  }
  //SETUP-----------------------------------------------
  RawData[datacount++] = Wire.read();
  RawData[datacount++] = Wire.read();
  RawData[datacount++] = Wire.read();                                                         // [zabox] [1.027] flam
  RawData[datacount++] = Wire.read();                                                            // [zabox] [1.027] flam
  RawData[datacount++] = Wire.read();
  RawData[datacount++] = Wire.read();
  RawData[datacount++] = Wire.read();
  RawData[datacount++] = Wire.read();
  for(int a = 0; a < 24; a++){
    RawData[datacount++]=Wire.read();
  }
  //EXT INST-----------------------------------------------
  for(int nbrPage = 0; nbrPage < 2; nbrPage++){
      MIDI.read();
    adress = (unsigned long)(PTRN_OFFSET + (patternNbr * PTRN_SIZE) + (MAX_PAGE_SIZE * nbrPage) + PTRN_SETUP_OFFSET);
    WireBeginTX(adress);
    Wire.endTransmission();
    Wire.requestFrom(HRDW_ADDRESS,MAX_PAGE_SIZE); //request of  64 bytes

    for (byte j = 0; j < MAX_PAGE_SIZE; j++){
      RawData[datacount++] = Wire.read();
    }
  }
  //VELOCITY-----------------------------------------------
  for(int nbrPage = 0; nbrPage < 4; nbrPage++){
      MIDI.read();
    adress = (unsigned long)(PTRN_OFFSET + (patternNbr * PTRN_SIZE) + (MAX_PAGE_SIZE * nbrPage) + PTRN_EXT_OFFSET);
    WireBeginTX(adress);
    Wire.endTransmission();
    Wire.requestFrom(HRDW_ADDRESS,MAX_PAGE_SIZE); //request of  64 bytes
    for (byte i = 0; i < 4; i++){//loop as many instrument for a page
      for (byte j = 0; j < NBR_STEP; j++){
        RawData[datacount++] = (Wire.read() & 0xFF);
      }
    }
  }

  uint16_t transmit_size=build_sysex(SysEx, RawData, sizeof(RawData), NAVA_PTRN_DMP, patternNbr);
  memory("Dump ptr transmit");
}

void DumpBank(byte selectedBank)
{
  byte *RawData=(byte *)calloc(4*PTRN_SIZE, sizeof(byte)); // Need to do the bank dump in two parts as there isn't enough memory to have the rawdata and the sysex array in memory.
  int datacount = 0;
  
  for ( int BankPart=0; BankPart < 4; BankPart++)
  { 
    int datacount = 0;
    uint8_t patternNbr = selectedBank * 16 + (8 * BankPart);
  
    unsigned long adress = (unsigned long)(PTRN_OFFSET + patternNbr * PTRN_SIZE);
    WireBeginTX(adress); 
    Wire.endTransmission();
    Wire.requestFrom(HRDW_ADDRESS,MAX_PAGE_SIZE); //request a 64 bytes page
    //TRIG-----------------------------------------------
    for(int i =0; i<NBR_INST;i++){
      RawData[datacount] = (unsigned long)((Wire.read() & 0xFF) | (( Wire.read() << 8) & 0xFF00));
      // Serial.println(Wire.read());
      datacount++;
    }
    //SETUP-----------------------------------------------
    RawData[datacount++] = Wire.read();
    RawData[datacount++] = Wire.read();
    RawData[datacount++] = Wire.read();                                                         // [zabox] [1.027] flam
    RawData[datacount++] = Wire.read();                                                            // [zabox] [1.027] flam
    RawData[datacount++] = Wire.read();
    RawData[datacount++] = Wire.read();
    RawData[datacount++] = Wire.read();
    RawData[datacount++] = Wire.read();
    for(int a = 0; a < 24; a++){
      RawData[datacount++]=Wire.read();
    }
    //EXT INST-----------------------------------------------
    for(int nbrPage = 0; nbrPage < 2; nbrPage++){
        MIDI.read();
      adress = (unsigned long)(PTRN_OFFSET + (patternNbr * PTRN_SIZE) + (MAX_PAGE_SIZE * nbrPage) + PTRN_SETUP_OFFSET);
      WireBeginTX(adress);
      Wire.endTransmission();
      Wire.requestFrom(HRDW_ADDRESS,MAX_PAGE_SIZE); //request of  64 bytes
  
      for (byte j = 0; j < MAX_PAGE_SIZE; j++){
        RawData[datacount++] = Wire.read();
      }
    }
    //VELOCITY-----------------------------------------------
    for(int nbrPage = 0; nbrPage < 4; nbrPage++){
        MIDI.read();
      adress = (unsigned long)(PTRN_OFFSET + (patternNbr * PTRN_SIZE) + (MAX_PAGE_SIZE * nbrPage) + PTRN_EXT_OFFSET);
      WireBeginTX(adress);
      Wire.endTransmission();
      Wire.requestFrom(HRDW_ADDRESS,MAX_PAGE_SIZE); //request of  64 bytes
      for (byte i = 0; i < 4; i++){//loop as many instrument for a page
        for (byte j = 0; j < NBR_STEP; j++){
          RawData[datacount++] = (Wire.read() & 0xFF);
        }
      }
    }

    uint16_t transmit_size=build_sysex(SysEx, RawData, 4*PTRN_SIZE, NAVA_BANK_DMP, selectedBank + 16* BankPart);

    free(RawData);
  }
}

void DumpTrack(byte trackNbr)
{
  byte *RawData=(byte *)calloc(TRACK_SIZE, sizeof(byte));
//  byte RawData[TRACK_SIZE];
  
  unsigned long adress;
  for(int nbrPage = 0; nbrPage < TRACK_SIZE/MAX_PAGE_SIZE; nbrPage++){
    adress = (unsigned long)(PTRN_OFFSET + (trackNbr * TRACK_SIZE) + (MAX_PAGE_SIZE * nbrPage) + TRACK_OFFSET);
    WireBeginTX(adress);
    Wire.endTransmission();
    if (adress > 65535) Wire.requestFrom(HRDW_ADDRESS_UP,MAX_PAGE_SIZE); //request of  64 bytes
    else Wire.requestFrom(HRDW_ADDRESS,MAX_PAGE_SIZE); //request of  64 bytes
    for (byte i = 0; i < MAX_PAGE_SIZE; i++){//loop as many instrument for a page
      RawData[i + (MAX_PAGE_SIZE * nbrPage)] = (Wire.read() & 0xFF); 
    }
  }
//  byte lowbyte = (byte)track[trkBuffer].patternNbr[1022];
//  byte highbyte = (byte)track[trkBuffer].patternNbr[1023];
//  track[trkBuffer].length =  (unsigned long)(lowbyte | highbyte << 8);

  uint16_t transmit_size=build_sysex(SysEx, RawData, TRACK_SIZE, NAVA_TRACK_DMP, trackNbr);
  free(RawData);
}

void DumpConfig()
{
  byte *RawData=(byte *)calloc(SETUP_SIZE, sizeof(byte));
  RawData[0]=(byte)(seq.sync); 
  RawData[1]=(byte)(seq.defaultBpm);
  RawData[2]=(byte)(seq.TXchannel);
  RawData[3]=(byte)(seq.RXchannel);
  RawData[4]=(byte)(seq.ptrnChangeSync);
  RawData[5]=(byte)(seq.muteModeHH);                                  // [zabox]
#if MIDI_EXT_CHANNEL
  RawData[6]=(byte)(seq.EXTchannel);  // [Neuromancer]
#endif
#if CONFIG_BOOTMODE
  RawData[7]=(byte)(seq.BootMode); // [Neuromancer]
#endif

  uint16_t transmit_size=build_sysex(SysEx, RawData, SETUP_SIZE, NAVA_CONFIG_DMP,0);

  free(RawData);
}

void GetConfig(byte *sysex)
{
  byte *RawData=(byte *)calloc(SETUP_SIZE, sizeof(byte));

  sysex_to_data(sysex + 6, RawData, 74);
  seq.sync = RawData[0];
  seq.defaultBpm = RawData[1];
  seq.TXchannel = RawData[2];
  seq.RXchannel = RawData[3];
  seq.ptrnChangeSync = RawData[4];
  seq.muteModeHH = RawData[5];
  seq.EXTchannel = RawData[6];
  seq.BootMode  = (SeqMode)RawData[7];

  SaveSeqSetup();
  SetSeqSync();
  needLcdUpdate = true;
}

void MidiSendSysex(byte Type, byte Param)
{
  switch(Type)
  {
    case NAVA_BANK_DMP: // Bank
        DumpBank(Param);
        break;
    case NAVA_PTRN_DMP: // Pattern
        DumpPattern(Param);
        break;
    case NAVA_TRACK_DMP: // Track
        DumpTrack(Param);
        break;
    case NAVA_CONFIG_DMP: // Config
        DumpConfig();
    case NAVA_FULL_DMP: // Full
      break; 
  }
}

#if DEBUG
void PrintSysex(byte *sysex, int size)
{
  Serial.print("Print Sysex size: "); Serial.println(size);
  for(int i=0; i<size; i++)
  {
    if ( i % 8 == 0 ) 
    {
      Serial.println();
      Serial.print(i,HEX);Serial.print(":\t");
    }
    Serial.print("0x");Serial.print(sysex[i], HEX); Serial.print("\t");
  }
  Serial.println();
  Serial.print("Sysex Size: "); Serial.println(size);

}

#endif
void HandleSystemExclusive(byte * RawSysEx, byte RawSize)
{
  char header[]={ START_OF_SYSEX, SYSEX_MANUFACTURER, SYSEX_DEVID_1, SYSEX_DEVID_2 };
  int16_t DataPointer=6;
  // Check if the sysex is for us.
  if ( memcmp(header, RawSysEx, sizeof(header)) != 0 ) return;
  RawSysEx[RawSize -1]= END_OF_SYSEX;

  // Get type and parameter
  byte Type=RawSysEx[4];
  byte Param=RawSysEx[5];
  byte BlckCnt = RawSysEx[6];
  
  // Do calculate checksum and compare to received checksum
  uint32_t checksum = crc.calc(RawSysEx, RawSize - CHECKSUMSIZE -1);
  uint32_t RxChecksum = 0;

  // Reconstruct received checksum
  for(int i=RawSize - CHECKSUMSIZE; i < RawSize; i++)
  {
    RxChecksum = ( RxChecksum << 4 ) | RawSysEx[i-1];
  }

#if DEBUG
  Serial.print("Calculated checksum: "); Serial.println(checksum, HEX);
  Serial.print("Received checksum: "); Serial.println(RxChecksum, HEX);
  PrintSysex(RawSysEx, RawSize);
#endif  

  if ( checksum != RxChecksum )
  {
    Serial.print("CREATE ERROR HANDLING");
    return;
  }

  switch(Type)
  {
  case NAVA_BANK_REQ: 
    {
      if ( Param < 8 )
      { 
        Serial.print("Dumping bank : "); Serial.println(char(Param + 65));
        DumpBank(Param);
      }
      break; 
    }
  case NAVA_PTRN_REQ:
    {
      if ( Param < 128 )
      {
        Serial.print("Dumping Pattern: ");Serial.println(Param);
        DumpPattern(Param);
      }
      break;  
    }
  case NAVA_TRACK_REQ:
    {
      if ( Param < 16 )
      {
        Serial.print("Dumping track: ");Serial.println(Param);
        DumpTrack(Param);
      }
      break;
    }
  case NAVA_CONFIG_REQ:
    { 
      Serial.println("Dumping Config");
      DumpConfig();
      break;
    }
   case NAVA_CONFIG_DMP:
    {
      Serial.println("Received a Config");
      GetConfig(RawSysEx);
      break; 
    }
  }
}

#endif // MIDI_HAS_SYSEX
