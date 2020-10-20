/*
 *  © 2020, Chris Harlow. All rights reserved.
 *  
 *  This file is part of DCC-EX CommandStation-EX
 *
 *  This is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  It is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CommandStation.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "RingStream.h"
#include "DIAG.h"

RingStream::RingStream( const uint16_t len)
{
  _len=len;
  _buffer=new byte[len];
  _pos_write=0;
  _pos_read=0;
  _buffer[0]=0;
  _overflow=false;
}

size_t RingStream::write(uint8_t byte) {
  if (_overflow) return 0;
  _buffer[_pos_write] = byte;
  ++_pos_write;
  if (_pos_write>=_len) _pos_write=0;
  if (_pos_write==_pos_read) {
    _overflow=true; 
    DIAG(F("\nRingStream(%d) OVERFLOW  %d %d \n"),_len, _pos_write, _pos_read);
    return 0;
  }
  return 1;
}

int RingStream::read() {
  if (_pos_read==_pos_write) return -1; 
  byte b=_buffer[_pos_read];
  _pos_read++;
  if (_pos_read>=_len) _pos_read=0;
  _overflow=false;
  return b;
}


int RingStream::count() {
  int peek=_pos_read;
  int counter=0;
  while(_buffer[peek]) {
    counter++;
    peek++;
    if (peek >= _len) peek=0;
  }
  return counter;
  }
