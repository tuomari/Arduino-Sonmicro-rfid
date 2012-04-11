/*
 * Copyright (C) 2011-2012 Tuomas Riihimäki <tuomas@iudex.fi>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef SONMICRORFID_H_
#define SONMICRORFID_H_

#include "HardwareSerial.h";

// #define DEBUG 1
#undef DEBUG
enum CardType
{
   ULTRALIGHT, STANDARD1K, CLASSIC4K, UNKNOWN, UNDEFINED
};
static const char HEXCHARS[] =
{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E',
      'F' };

class SonmicroRfid
{
public:

   enum AuthKey
   {
      KEYA = 0xAA,
      KEYB = 0xBB,
      DEFAULTKEY = 0xFF, // Key A + FF FF FF FF FF FF

      // Stored keys
      STORE_A1 = 0x10, STORE_A2 = 0x11, STORE_A3 = 0x12, STORE_A4 = 0x13,
      STORE_A5 = 0x14, STORE_A6 = 0x15, STORE_A7 = 0x16, STORE_A8 = 0x17,
      STORE_A9 = 0x18, STORE_A10 = 0x19, STORE_A11 = 0x1a, STORE_A12 = 0x1b,
      STORE_A13 = 0x1c, STORE_A14 = 0x1d, STORE_A15 = 0x1e, STORE_A16 = 0x1f,
      STORE_B1 = 0x20, STORE_B2 = 0x21, STORE_B3 = 0x22, STORE_B4 = 0x23,
      STORE_B5 = 0x24, STORE_B6 = 0x25, STORE_B7 = 0x26, STORE_B8 = 0x27,
      STORE_B9 = 0x28, STORE_B10 = 0x29, STORE_B11 = 0x2a, STORE_B12 = 0x2b,
      STORE_B13 = 0x2c, STORE_B14 = 0x2d, STORE_B15 = 0x2e, STORE_B16 = 0x2f,

   };
   enum AuthResponse
   {
      AUTH_SUCCESS = 0x4C, AUTH_NO_TAG = 0x4E,
      AUTH_LOGIN_FAILED = 0x55, AUTH_INVALID_EPROMKEY = 0x45,
      NO_CARD_SELECTED = 0xFE, AUTH_FAILURE=0xFF
   };

   ~SonmicroRfid();
   SonmicroRfid();

   void init(HardwareSerial * rfidport, long portspeed = 0,HardwareSerial * debugport = NULL);
   void initDebug();

   /**
    * Tries to select a tag.
    *
    * If command was successful card UID can be read with
    * functions getCarduid or getUidAsChar
    *
    * Type of the selected card can be read from the
    *
    * @Return returns true if tag was found. False otherwise.
    *
    */
   bool selectTag();

   /**
    * when select is successfull we can authenticat to the card for
    * reading and writing data...
    *
    */
   bool authenticate();

   /**
    * Can be called only when authenticated
    *
    *
    */

   bool decrement(const int32_t decvalue);

   bool increment(const int32_t decvalue);
   bool readValue();
   bool writeValue(const int32_t & value);
   bool readBlock();
   // Read block value from card.
   // Only for legacy applications. Should not be used!!
   bool readBlockValue();

   /**
    * str array must be 2 x hex array length..
    */
   void hexToString(const byte hex[], char str[], int hexlength)const;

   /**
    * NOTE!!!!
    *  Blocks 0-3 contain internal data and should not be used!!!
    *  On 1k and 4k cards last block of a sector ( 7, 11, 15... )
    *  is permission block. Use with caution!!!
    */
   void setBlock(uint8_t block);
   uint8_t getBlock() const;

   CardType getCardType() const;
   byte getErrorbyte() const;

   /**
    *  str array must be at least 14 bytes
    */
   void getUidAsChar(char* str) const;
   const byte* getCarduid() const;
   bool getFirmware(char str[10]);
   int32_t getValue() const;

   void bytesToInteger(const byte b[4], int32_t& value, int offset = 0) const;
   void bytesToIntegerLSB(const byte b[4], int32_t& value, int offset = 0) const;

   void integerToBytes(const int32_t val, byte b[4], int offset = 0)const;
   void setAuth(AuthKey keytype, const byte key[]);
   AuthResponse getErrorcode() const;

   /**
    * To be deprecated...
    */
   void setTryLegacyvalueFor1K(bool value);

private:
   HardwareSerial * _serial;
   HardwareSerial * _debugSerial;
   CardType _cardtype;
   uint8_t _block;
   byte _carduid[7];
   AuthResponse _errorcode;
   int32_t _slotValue;
   byte _blockData[16];
   AuthKey _keytype;
   byte _key[6];
   byte _errorbyte;

   bool _tryLegacyvalueFor1K;

   // debug(str) macro should be used to not compile
   // extra debug strings to binary...
   void debugVal(const char str[], bool newline) const;
   void debugVal(const long str, bool newline,const int type) const;
   void debugVal(const long str, bool newline) const;

   bool read1kValueBlock();
   bool writeValue(const byte cmdbyte, const int32_t & value);

   void fail();
   void flushSerial() const;

   /**
    * sends commands to the RFID reader.
    *
    * @param cmd The command to be sent to the reader
    * @param cmdlength Length of the command array
    * @param response Array which the response data ( length, cmd, data ) is stored
    * @param reponselength The length of the response array
    */
   bool sendCommand(const byte* cmd, const uint8_t cmdlength, byte* response, int reponselength)const;

   // Copy constructor is forbidden
   // Assignment operator is forbidden
   SonmicroRfid& operator=(const SonmicroRfid&);
   SonmicroRfid(const SonmicroRfid&);

};

extern SonmicroRfid RFID;

#endif /* SONMICRORFID_H_ */
