/*
    OpenDeck MIDI platform firmware
    Copyright (C) 2015-2018 Igor Petrovic

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Analog.h"
#include "core/src/general/BitManipulation.h"
#include "core/src/general/Misc.h"
#include "interface/CInfo.h"

//use 1k resistor when connecting FSR between signal and ground

int16_t Analog::calibratePressure(int16_t value, pressureType_t type)
{
    switch(type)
    {
        case velocity:
        return mapRange_uint16(CONSTRAIN(value, FSR_MIN_VALUE, FSR_MAX_VALUE), FSR_MIN_VALUE, FSR_MAX_VALUE, 0, 127);

        case aftertouch:
        return mapRange_uint16(CONSTRAIN(value, FSR_MIN_VALUE, AFTERTOUCH_MAX_VALUE), FSR_MIN_VALUE, AFTERTOUCH_MAX_VALUE, 0, 127);

        default:
        return 0;
    }
}

bool Analog::getFsrPressed(uint8_t fsrID)
{
    return fsrPressed[fsrID];
}

void Analog::setFsrPressed(uint8_t fsrID, bool state)
{
    fsrPressed[fsrID] = state;
}

void Analog::checkFSRvalue(uint8_t analogID, uint16_t pressure)
{
    uint8_t calibratedPressure = calibratePressure(pressure, velocity);

    if (calibratedPressure > 0)
    {
        if (!getFsrPressed(analogID))
        {
            //sensor is really pressed
            setFsrPressed(analogID, true);
            uint8_t note = database.read(DB_BLOCK_ANALOG, dbSection_analog_midiID, analogID);
            uint8_t channel = database.read(DB_BLOCK_ANALOG, dbSection_analog_midiChannel, analogID);
            midi.sendNoteOn(note, calibratedPressure, channel);
            #ifdef DISPLAY_SUPPORTED
            display.displayMIDIevent(displayEventOut, midiMessageNoteOn_display, note, calibratedPressure, channel+1);
            #endif
            #ifdef LEDS_SUPPORTED
            leds.midiToState(midiMessageNoteOn, note, calibratedPressure, channel, true);
            #endif

            if (cinfoHandler != nullptr)
                (*cinfoHandler)(DB_BLOCK_ANALOG, analogID);
        }
    }
    else
    {
        if (getFsrPressed(analogID))
        {
            setFsrPressed(analogID, false);
            uint8_t note = database.read(DB_BLOCK_ANALOG, dbSection_analog_midiID, analogID);
            uint8_t channel = database.read(DB_BLOCK_ANALOG, dbSection_analog_midiChannel, analogID);
            midi.sendNoteOff(note, 0, channel);
            #ifdef DISPLAY_SUPPORTED
            display.displayMIDIevent(displayEventOut, midiMessageNoteOff_display, note, calibratedPressure, channel+1);
            #endif
            #ifdef LEDS_SUPPORTED
            leds.midiToState(midiMessageNoteOff, 0, channel, true);
            #endif

            if (cinfoHandler != nullptr)
                (*cinfoHandler)(DB_BLOCK_ANALOG, analogID);
        }
    }

    //update values
    lastAnalogueValue[analogID] = pressure;
}