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

#include "application/io/base.h"
#include "application/io/common/common.h"

#include <array>

#ifdef PROJECT_TARGET_SUPPORT_I2C

namespace io::i2c
{
    class I2c : public io::Base
    {
        public:
        I2c() = default;
        ~I2c();

        bool        init() override;
        void        updateSingle(size_t index, bool forceRefresh = false) override;
        void        updateAll(bool forceRefresh = false) override;
        size_t      maxComponentUpdateIndex() override;
        static void registerPeripheral(Peripheral* instance);

        private:
        static constexpr size_t MAX_PERIPHERALS = 5;

        static size_t                                   _peripheralCounter;
        static std::array<Peripheral*, MAX_PERIPHERALS> _peripherals;
    };
}    // namespace io::i2c

#else
#include "stub.h"
#endif