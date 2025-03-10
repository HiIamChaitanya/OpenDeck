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

#include "i2c.h"
#include "hwa_hw.h"
#include "peripherals/builder.h"

namespace io::i2c
{
    class BuilderHw
    {
        public:
        BuilderHw() = default;

        I2c& instance()
        {
            return _instance;
        }

        private:
        HwaHw              _hwa;
        database::Admin&   _database    = database::BuilderHw::instance();
        PeripheralsBuilder _peripherals = PeripheralsBuilder(_hwa, _database);
        I2c                _instance;
    };
}    // namespace io::i2c