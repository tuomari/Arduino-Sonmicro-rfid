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
 
#include <SPI.h>
#include "sonmicrorfid.h"

SonmicroRfid::SonmicroRfid() :
      _serial(NULL), _debugSerial(NULL), _cardtype(UNDEFINED), _block(), _carduid(), _errorcode(
            NO_CARD_SELECTED), _slotValue(0), _blockData(), _keytype(
            DEFAULTKEY), _key(), _errorbyte(), _tryLegacyvalueFor1K(false)
{
}

SonmicroRfid::~SonmicroRfid()
{

}

#ifdef DEBUG
#define debug(str) debugVal(str, false)
#define debugn(str) debugVal(str, true)
#define debug(val) debugVal(val, false)
#define debugHex(val) debugVal(val, false, HEX)
#define debugn(val) debugVal(val, true)
#define debugnHex(val) debugVal(val, true, HEX)

void SonmicroRfid::debugVal(const char str[], bool newline) const
{
   if (_debugSerial != NULL)
   {
      if (newline)
      {
         _debugSerial->println(str);
      }
      else
      {
         _debugSerial->print(str);
      }
   }
}

void SonmicroRfid::debugVal(long val, bool newline) const
{
   debugVal(val, newline, DEC);
}
void SonmicroRfid::debugVal(long val, bool newline, int type) const
{
   if (_debugSerial != NULL)
   {
      if (newline)
      {
         _debugSerial->println(val, type);
      }
      else
      {
         _debugSerial->print(val, type);

      }
   }
}

#else
#define debug(str)
#define debugn(str)
#define debug(val)
#define debugHex(val)
#define debugn(val)
#define debugnHex(val)

#endif

void SonmicroRfid::bytesToInteger(const byte b[4], int32_t& value,
      int offset) const
{
   value = ((int32_t) b[3 + offset]) << 24;
   value |= ((int32_t) b[2 + offset]) << 16;
   value |= ((int32_t) b[1 + offset]) << 8;
   value |= ((int32_t) b[offset]);

}

void SonmicroRfid::bytesToIntegerLSB(const byte b[4], int32_t& value,
      int offset) const
{
   value = ((int32_t) b[3 + offset]);
   value |= ((int32_t) b[2 + offset]) << 8;
   value |= ((int32_t) b[1 + offset]) << 16;
   value |= ((int32_t) b[offset]) << 24;

}

void SonmicroRfid::integerToBytes(const int32_t val, byte b[4],
      int offset) const
{
   b[3 + offset] = (byte) ((val >> 24) & 0xff);
   b[2 + offset] = (byte) ((val >> 16) & 0xff);
   b[1 + offset] = (byte) ((val >> 8) & 0xff);
   b[offset] = (byte) ((val) & 0xff);

}

static const byte MSG_PREFIX[] =
{ 0xFF, 0x00 };

bool SonmicroRfid::sendCommand(const byte cmd[], const uint8_t cmdlength,
      byte response[], int reponselength) const
{
   uint8_t sendchecksum = cmdlength;

   // Calc checksum;
   for (int i = 0; i < cmdlength; ++i)
   {
      sendchecksum += cmd[i];
   }

   flushSerial();
   _serial->write(MSG_PREFIX, 2);
   _serial->write(cmdlength);
   _serial->write(cmd, cmdlength);
   _serial->write(sendchecksum);

   // wait for data
   delay(20);

   for (int escapeloop = 0; _serial->available() < 4 && escapeloop < 90;
         ++escapeloop)
   {
      delay(10);
      debug("+");
   }

   debug("Out of waitloop Data available: ");
   debugn(_serial->available());
   // Check that there is at least 4 bytes of data available
   // (0xFF, 0x00, length, responseCmd)
   if (_serial->available() < 4)
   {

      debug("Not Enough data ");
      debugn(_serial->available());
      _serial->available();
      flushSerial();

      return false;
   }

   byte head1 = _serial->read();
   byte head2 = _serial->read();
   debug("Got headerdata: ");
   debugHex(head1);
   debug(" ");
   debugHex(head2);
   debug("Data available ");
   debugn(_serial->available());

   if (head1 != 0xFF || head2 != 0x00)
   {
      debug("Wrong headerdata! ");

      flushSerial();

      return false;
   }

   uint8_t datalength = this->_serial->read();
   response[0] = datalength;
   uint8_t respchecksum = datalength;

   // read CMD from stream
   response[1] = _serial->read();
   respchecksum += response[1];

   if (cmd[0] != response[1])
   {
      flushSerial();
      debug("Response command is different than request command! CMD: ");
      debugHex(cmd[0]);
      debug(" Resp ");
      debugnHex(response[1]);
      return false;
   }

   // check for overindexing..
   // datalength is only ( cmd + data )
   // while responselength is ( length + cmd + data )
   if (datalength >= reponselength)
   {
      debugn("Response too long for the response array");
      return false;
   }

   for (int i = 2; i <= datalength; ++i)
   {
      // if no data is available
      if (_serial->available() == 0)
      {
         debugn("No data available...");
         flushSerial();
         return false;
      }
      response[i] = _serial->read();
      respchecksum += response[i];
   }

   if (_serial->available() == 0 || _serial->read() != respchecksum)
   {
      debugn("Checksum not found or does not match");
      delay(200);
      flushSerial();
      return false;
   }

   debugn("Command sent and received successfully.");

   return true;

}

static const byte SELECT_TAG_CMD = 0x83;

bool SonmicroRfid::selectTag()
{

   byte resp[10] =
   { };

   // Clear uid
   memset(_carduid, 0, 7);

   if (sendCommand(&SELECT_TAG_CMD, 1, resp, 10))
   {
      debugn("Received SELECT response");

      if (resp[0] < 6)
      {
         debug("No tag in field. Respsize: ");
         debugn(resp[0]);
         _cardtype = UNDEFINED;
         return false;
      }

      switch (resp[2])
      {
      case 0x01:
         _cardtype = ULTRALIGHT;
         break;
      case 0x02:
         _cardtype = STANDARD1K;
         break;
      case 0x03:
         _cardtype = CLASSIC4K;
         break;
      default:
         _cardtype = UNKNOWN;
         break;
      }

      // how many bytes is in serial number ( Correct from MSB to LSB )
      uint8_t bytes = resp[0] - 2;

      // length is correnct index, because length byte itself is not
      // calculated to length..
      uint8_t respIndex = resp[0];

      for (uint8_t i = 0; i < bytes; ++i)
      {
         _carduid[i] = resp[respIndex];
         --respIndex;
      }
      return true;

   }
   _cardtype = UNDEFINED;
   return false;
}

bool SonmicroRfid::authenticate()
{
   if (_cardtype == UNDEFINED)
   {
      debugn("Undefined cardtype.");
      _errorcode = NO_CARD_SELECTED;
      fail();
      return false;
   }
   else if (_cardtype == ULTRALIGHT)
   {
      _errorcode = AUTH_SUCCESS;
      return true;
   }

   int cmdlength = 3;
   if (_keytype < 0x30)
   {
      cmdlength = 9;
   }

   byte command[9] =
   { 0x85, _block, _keytype };

   if (cmdlength > 3)
   {
      int keyi = 0;

      for (int i = 3; i < cmdlength; ++i)
      {
         command[i] = _key[keyi++];
      }
   }

   debugn("Sending authcommand");
   for (int i = 0; i < cmdlength; ++i)
   {
      debugnHex(command[i]);

   }

   byte resp[3] =
   { };

   if (sendCommand(command, cmdlength, resp, 3))
   {
      _errorcode = AuthResponse(resp[2]);
      debug("Authcommand: ");
      debugnHex(_errorcode);
      return AUTH_SUCCESS == _errorcode;
   }

   debugn("AUTH FAILED!");
   _errorcode = AUTH_FAILURE;
   return false;
}

static const byte FIRMWARECMD = 0x81;

bool SonmicroRfid::getFirmware(char str[10])
{
   // debugString("Trying firmware debug");

   byte ret[10] =
   { };
   if (sendCommand(&FIRMWARECMD, 1, ret, 10))
   {

      debugn("Looping data from byte to str");
      for (int i = 0; i < ret[0]; ++i)
      {
         str[i] = ret[i + 2];
      }

      return true;
   }

   str[ret[0]] = '\0';
   return false;

}

bool SonmicroRfid::decrement(const int32_t decvalue)
{
   byte ret[10] =
   { };

   byte cmd[] =
   { 0x8E, _block, 0x00, 0x00, 0x00, 0x00 };

   integerToBytes(decvalue, cmd, 2);

   if (sendCommand(cmd, 6, ret, 10) && ret[0] == 0x06)
   {
      bytesToInteger(ret, _slotValue, 3);
      debug("Value from decrement: ");
      debugn(_slotValue);

//      // TODO: Poista joskus!
//      if (_slotValue > 0xf0000 || _slotValue < -0xf0000)
//      {
//         increment(decvalue);
//         debugn("Converting deprecated value");
//         bytesToIntegerLSB(_blockData, _slotValue, 0);
//         integerToBytes(_slotValue, _blockData);
//         _slotValue -= decvalue;
//         writeValue(_slotValue);
//      }
      return true;
   }
   else
   {

      _errorbyte = ret[2];
      if (_tryLegacyvalueFor1K && _errorbyte == 0x46)
      {
         if (readBlock())
         {
            bytesToInteger(_blockData, _slotValue);
            return writeValue(--_slotValue);
         }
      }
      debug("Decrementing. FAILURE: ");
      debugnHex(_errorbyte);
      fail();
   }
   return false;
}

bool SonmicroRfid::increment(const int32_t incvalue)
{
   byte ret[10] =
   { };

   byte cmd[] =
   { 0x8D, _block, 0x00, 0x00, 0x00, 0x00 };

   integerToBytes(incvalue, cmd, 2);

   if (sendCommand(cmd, 6, ret, 10) && ret[0] == 0x06)
   {
      bytesToInteger(ret, _slotValue, 3);
      _blockData[0] = ret[3];
      _blockData[1] = ret[4];
      _blockData[2] = ret[5];
      _blockData[3] = ret[6];
      return true;
   }
   else
   {
      _errorbyte = ret[2];
      fail();

   }
   return false;
}

//
//boolean SonmicroRfid::decrement(byte block, uint8_t decrementValue,
//      uint8_t &returnvalue) const
//{
//
//   byte dec[4] =
//   { decrementValue };
//   byte ret[4] =
//   { };
//   if (decrement(block, dec, ret))
//   {
//      returnvalue = ret[0];
//      return true;
//   }
//
//   return false;
//}
//
//boolean SonmicroRfid::decrement(byte block, const byte* decrementValue,
//      byte* returnvalue) const
//{
//
//   byte ret[7] =
//   { };
//   byte cmd[6] =
//   { 0x8E, block };
//
//   {
//      int cmdi = 2;
//      for (int i = 0; i < 4; ++i)
//      {
//         cmd[cmdi] = decrementValue[i];
//      }
//   }
//   if (sendCommand(cmd, 6, ret, 7))
//   {
//      if (ret[0] == 0x06)
//      {
//         int reti = 3;
//         for (int i = 0; i < 4; ++i)
//         {
//            returnvalue[i] = ret[reti++];
//         }
//         return true;
//      }
//      else
//      {
//         // save errorcode to returnvalue...
//         returnvalue[0] = ret[2];
//      }
//   }
//   return false;
//}
//
//void uintToarray(const uint32_t & from, byte* array)
//{
//   // uit32_t
//
//}

void SonmicroRfid::hexToString(const byte* hex, char* str,
      int hexlength) const
{

   for (int hexi, stri = 0; hexi < hexlength; ++hexi)
   {
      int lesserHex = hex[hexi] % 16;

      str[stri++] = HEXCHARS[(hex[hexi] - lesserHex) / 16];
      str[stri++] = HEXCHARS[lesserHex];
   }
}
//
CardType SonmicroRfid::getCardType() const
{
   return _cardtype;
}
const byte* SonmicroRfid::getCarduid() const
{
   return _carduid;
}
//
void SonmicroRfid::getUidAsChar(char* str) const
{
   hexToString(_carduid, str, 7);
}

void SonmicroRfid::setBlock(uint8_t block)
{
   _block = block;
}
uint8_t SonmicroRfid::getBlock() const
{
   return _block;
}

void SonmicroRfid::init(HardwareSerial * rfidport, long portspeed,
      HardwareSerial * debugport)
{
   _serial = rfidport;
   _debugSerial = debugport;

   if (portspeed == 0)
   {
      portspeed = 19200;
   }

   _serial->begin(portspeed);
   debug("Initialized SonmicroRFID: ");
   debug(portspeed);
}

bool SonmicroRfid::read1kValueBlock()
{

   byte req[] =
   { 0x87, _block };
   byte resp[8];
   if (sendCommand(req, 2, resp, 8))
   {
      if (resp[0] != 6)
      {
         _errorbyte = resp[2];
         debug("Reading 1k valueblock failed: ");
         debugnHex(_errorbyte);
         fail();
         return false;
      }

      for (int i = 0; i < 4; ++i)
      {
         _blockData[i] = resp[i + 3];
      }
      return true;
   }
   return false;
}

bool SonmicroRfid::writeValue(const int32_t & value)
{

   bool ret = false;
   switch (_cardtype)
   {
   case ULTRALIGHT:
      debugn("Writing for UL");
      ret = writeValue(0x8B, value);

      break;
   case STANDARD1K:
   case CLASSIC4K:
      debugn("Writing for 1k");
      ret = writeValue(0x8A, value);
      break;
   default:
      debugn("Writing value command failed! Unknown cardtype");
      return false;
      break;
   }

   if (ret)
   {
      bytesToInteger(_blockData, _slotValue);
   }
   return ret;
}

bool SonmicroRfid::writeValue(const byte cmdbyte, const int32_t & value)
{
   byte req[6] =
   { cmdbyte, _block };
   integerToBytes(value, req, 2);
   debug("Writing hex: ");
   for (int i = 0; i < 6; ++i)
   {
      debugnHex(req[i]);
   }

   byte resp[8];
   if (sendCommand(req, 6, resp, 8))
   {
      if (resp[0] != 6)
      {
         _errorbyte = resp[2];
         debugn("Writing valueblock failed.");

         debugnHex(_errorbyte);

         fail();
         return false;
      }

      debugn("Received data from card after write: ");
      for (int i = 0; i < 4; ++i)
      {
         debugnHex(resp[i + 3]);
         _blockData[i] = resp[i + 3];
      }
      return true;
   }

   return false;
}

bool SonmicroRfid::readValue()
{

   bool ret = false;
   switch (_cardtype)
   {
   case ULTRALIGHT:
      ret = readBlock();
      break;
   case STANDARD1K:
   case CLASSIC4K:
      ret = read1kValueBlock();
      debug("Reading valueblock from card ");
      debugnHex(_errorbyte);
      if (!ret && _tryLegacyvalueFor1K && getErrorbyte() == 0x49)
      {
         debugn("Trying to read legacy blockvalue from 1k card");
         ret = readBlock();
         if (!ret)
         {
            debug("Error reading legacyblock ");
            debugnHex(_errorbyte);
         }
      }

      break;
   default:

      debug("Reading value failed for cardtype: ");
      debugnHex(_cardtype);
      return false;
      break;
   }

   if (ret)
   {
      debugn("Received data from card after READ: ");
      for (int i = 0; i < 4; ++i)
      {
         debugnHex(_blockData[i]);
      }
      debugn("");

      bytesToInteger(_blockData, _slotValue);

      if (_slotValue > 0xf0000 || _slotValue < -0xf0000)
      {
         debugn("Converting deprecated value");
         bytesToIntegerLSB(_blockData, _slotValue);
         integerToBytes(_slotValue, _blockData);
      }
   }

   return ret;
}

bool SonmicroRfid::readBlock()
{
   if (_block < 1)
   {
      return false;
   }
   byte cmd[2] =
   { 0x86, _block };
   byte resp[20];
   if (sendCommand(cmd, 2, resp, 20))
   {
      if (resp[0] != 0x12)
      {

         debug("Error reading block. size not 0x12");
         debugnHex(resp[0]);
         fail();
         return false;
      }

      debug("Read bloc data: ");
      for (int i = 0; i < 16; ++i)
      {
         _blockData[i] = resp[i + 3];
      }
      return true;
   }
   return false;
}

void SonmicroRfid::setAuth(AuthKey keytype, const byte key[6])
{
   _keytype = keytype;
   for (int i = 0; i < 6; ++i)
   {
      _key[i] = key[i];
   }
}

void SonmicroRfid::fail()
{
   _cardtype = UNDEFINED;
}

int32_t SonmicroRfid::getValue() const
{
   return _slotValue;
}

SonmicroRfid::AuthResponse SonmicroRfid::getErrorcode() const
{
   return _errorcode;
}
byte SonmicroRfid::getErrorbyte() const
{
   return _errorbyte;
}

void SonmicroRfid::flushSerial() const
{
   while (_serial->available() > 0)
   {
      _serial->read();
   }
}

void SonmicroRfid::setTryLegacyvalueFor1K(bool value)
{
   _tryLegacyvalueFor1K = value;
}

SonmicroRfid RFID;

