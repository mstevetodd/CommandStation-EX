/*
 *  © 2021, Chris Harlow, Neil McKechnie. All rights reserved.
 *  © 2023, Harald Barth.
 *
 *  This file is part of CommandStation-EX
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

// CAUTION: the device dependent parts of this class are created in the .ini
// using LCD_Implementation.h

/* The strategy for drawing the screen is as follows.
 *  1) There are up to eight rows of text to be displayed.
 *  2) Blank rows of text are ignored.
 *  3) If there are more non-blank rows than screen lines,
 *     then all of the rows are displayed, with the rest of the
 *     screen being blank.
 *  4) If there are fewer non-blank rows than screen lines,
 *     then a scrolling strategy is adopted so that, on each screen
 *     refresh, a different subset of the rows is presented.
 *  5) On each entry into loop2(), a single operation is sent to the 
 *     screen; this may be a position command or a character for
 *     display.  This spreads the onerous work of updating the screen
 *     and ensures that other loop() functions in the application are
 *     not held up significantly.  The exception to this is when 
 *     the loop2() function is called with force=true, where 
 *     a screen update is executed to completion.  This is normally
 *     only noMoreRowsToDisplay during start-up.
 *  The scroll mode is selected by defining SCROLLMODE as 0, 1 or 2
 *  in the config.h.
 *  #define SCROLLMODE 0 is scroll continuous (fill screen if poss),
 *  #define SCROLLMODE 1 is by page (alternate between pages),
 *  #define SCROLLMODE 2 is by row (move up 1 row at a time).

 */

#include "Display.h"

// Constructor - allocates device driver.
Display::Display(DisplayDevice *deviceDriver) {
  _deviceDriver = deviceDriver;
  // Get device dimensions in characters (e.g. 16x2).
  numCharacterColumns = _deviceDriver->getNumCols();
  numCharacterRows = _deviceDriver->getNumRows();
  for (uint8_t row = 0; row < MAX_CHARACTER_ROWS; row++) 
    rowBuffer[row][0] = '\0';
  topRow = ROW_INITIAL;  // loop2 will fill from row 0
  
  addDisplay(0);  // Add this display as display number 0
};

void Display::begin() {
  _deviceDriver->begin();
  _deviceDriver->clearNative();
}

void Display::_clear() {
  _deviceDriver->clearNative();
  for (uint8_t row = 0; row < MAX_CHARACTER_ROWS; row++) 
    rowBuffer[row][0] = '\0';
  topRow = ROW_INITIAL;  // loop2 will fill from row 0
}

void Display::_setRow(uint8_t line) {
  hotRow = line;
  hotCol = 0;
  rowBuffer[hotRow][0] = 0;  // Clear existing text
}

size_t Display::_write(uint8_t b) {
  if (hotRow >= MAX_CHARACTER_ROWS || hotCol >= MAX_CHARACTER_COLS) return -1;
  rowBuffer[hotRow][hotCol] = b;
  hotCol++;
  rowBuffer[hotRow][hotCol] = 0;
  return 1;
}

// Refresh screen completely (will block until complete). Used
// during start-up.
void Display::_refresh() {
  loop2(true);
}

// On normal loop entries, loop will only make one output request on each
// entry, to avoid blocking while waiting for the I2C.
void Display::_displayLoop() {
  // If output device is busy, don't do anything on this loop
  // This avoids blocking while waiting for the device to complete.
  if (!_deviceDriver->isBusy()) loop2(false);
}

Display *Display::loop2(bool force) {
  unsigned long currentMillis = millis();

  if (!force) {
    // See if we're in the time between updates
    if ((currentMillis - lastScrollTime) < DISPLAY_SCROLL_TIME)
      return NULL;
  } else {
    // force full screen update from the beginning.
    rowFirst = ROW_INITIAL;
    rowNext = ROW_INITIAL;
    bufferPointer = 0;
    noMoreRowsToDisplay = false;
    slot = 0;
  }

  do {
    if (bufferPointer == 0) {
      // Find a line of data to write to the screen.
      if (rowFirst == ROW_INITIAL) rowFirst = rowNext;
      if (findNextNonBlankRow()) {
        // Non-blank line found, so copy it (including terminator)
        for (uint8_t i = 0; i <= MAX_CHARACTER_COLS; i++)
          buffer[i] = rowBuffer[rowNext][i];
      } else {
        // No non-blank lines left, so draw a blank line
        buffer[0] = 0;
      }
      _deviceDriver->setRowNative(slot);  // Set position for display
      charIndex = 0;
      bufferPointer = &buffer[0];
    } else {
      // Write next character, or a space to erase current position.
      char ch = *bufferPointer;
      if (ch) {
        _deviceDriver->writeNative(ch);
        bufferPointer++;
      } else {
        _deviceDriver->writeNative(' ');
      }

      if (++charIndex >= MAX_CHARACTER_COLS) {
        // Screen slot completed, move to next slot on screen
        bufferPointer = 0;
        slot++;
        if (slot >= numCharacterRows) {
          // Last slot on screen written, reset ready for next screen update.
#if SCROLLMODE==2
          if (!noMoreRowsToDisplay) {
            // On next refresh, restart one row on from previous start.
            rowNext = rowFirst;
            findNextNonBlankRow();
          }
#endif
          noMoreRowsToDisplay = false;
          slot = 0;
          rowFirst = ROW_INITIAL;
          lastScrollTime = currentMillis;
          return NULL;
        }
      }
    }
  } while (force);

  return NULL;
}

bool Display::findNextNonBlankRow() {
  while (!noMoreRowsToDisplay) {
    if (rowNext == ROW_INITIAL)
      rowNext = 0;
    else
      rowNext = rowNext + 1;
#if SCROLLMODE == 1
    if (rowNext >= MAX_CHARACTER_ROWS) {
      // Finished if we've looped back to start
      rowNext = ROW_INITIAL;
      noMoreRowsToDisplay = true;
      return false;
    }
#else
    if (rowNext >= MAX_CHARACTER_ROWS)
      rowNext = 0;
    if (rowNext == rowFirst) {
      // Finished if we're back to the first one shown
      noMoreRowsToDisplay = true;
      return false;
    }
#endif
    if (rowBuffer[rowNext][0] != 0) {
      // Found non-blank row
      return true;
    }
  }
  return false;
}
