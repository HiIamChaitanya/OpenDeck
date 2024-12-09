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

#include "common.h"

#include "application/database/database.h"

namespace io::encoders
{
    using Database = database::User<database::Config::Section::encoder_t,
                                    database::Config::Section::global_t>;

    class Hwa
    {
        public:
        virtual ~Hwa() = default;

        // should return true if the value has been refreshed, false otherwise
        virtual bool state(size_t index, uint8_t& numberOfReadings, uint16_t& states)
        {
            return false;
        }
    };

    class Filter
    {
        public:
        virtual ~Filter() = default;

        virtual bool isFiltered(size_t      index,
                                position_t  position,
                                position_t& filteredPosition,
                                uint32_t    sampleTakenTime) = 0;

        virtual void     reset(size_t index)            = 0;
        virtual uint32_t lastMovementTime(size_t index) = 0;
    };
}    // namespace io::encoders