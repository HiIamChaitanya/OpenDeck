/*

Copyright Igor Petrovic

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#pragma once

#include "deps.h"
#include "board/board.h"

#include "core/util/util.h"

namespace io::buttons
{
    class HwaHw : public Hwa
    {
        public:
        HwaHw() = default;

#ifdef BUTTONS_SUPPORTED
        bool state(size_t index, uint8_t& numberOfReadings, uint16_t& states)
        {
            if (!board::io::digital_in::state(index, _dInRead))
            {
                return false;
            }

            numberOfReadings = _dInRead.count;
            states           = _dInRead.readings;

            return true;
        }

        size_t buttonToEncoderIndex(size_t index) override
        {
            return board::io::digital_in::encoderFromInput(index);
        }
#endif

        private:
        board::io::digital_in::Readings _dInRead;
    };
}    // namespace io::buttons
