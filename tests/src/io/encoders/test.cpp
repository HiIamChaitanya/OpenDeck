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

#include "tests/common.h"
#include "tests/helpers/listener.h"
#include "application/io/encoders/builder_test.h"
#include "application/util/configurable/configurable.h"

#ifdef ENCODERS_SUPPORTED

using namespace io;

namespace
{
    class EncodersTest : public ::testing::Test
    {
        protected:
        void SetUp() override
        {
            ASSERT_TRUE(_encoders._databaseAdmin.init());
            ASSERT_TRUE(_encoders._databaseAdmin.factoryReset());
            ASSERT_EQ(0, _encoders._databaseAdmin.getPreset());

            // set known state
            for (size_t i = 0; i < encoders::Collection::SIZE(); i++)
            {
                ASSERT_TRUE(_encoders._database.update(database::Config::Section::encoder_t::ENABLE, i, 1));
                ASSERT_TRUE(_encoders._database.update(database::Config::Section::encoder_t::INVERT, i, 0));
                ASSERT_TRUE(_encoders._database.update(database::Config::Section::encoder_t::MODE, i, encoders::type_t::CONTROL_CHANGE_7FH01H));
                ASSERT_TRUE(_encoders._database.update(database::Config::Section::encoder_t::PULSES_PER_STEP, i, 1));
            }

            MidiDispatcher.listen(messaging::eventType_t::ENCODER,
                                  [this](const messaging::Event& dispatchMessage)
                                  {
                                      _listener.messageListener(dispatchMessage);
                                  });
        }

        void TearDown() override
        {
            ConfigHandler.clear();
            MidiDispatcher.clear();
            _listener._event.clear();
        }

        void stateChangeRegister(uint8_t state)
        {
            _listener._event.clear();

            EXPECT_CALL(_encoders._hwa, state(_, _, _))
                .WillRepeatedly(DoAll(SetArgReferee<1>(1),
                                      SetArgReferee<2>(state),
                                      Return(true)));

            _encoders._instance.updateAll();
        }

        static constexpr std::array<uint8_t, 4> ENCODER_STATE = {
            0b00,
            0b10,
            0b11,
            0b01
        };

        test::Listener        _listener;
        encoders::BuilderTest _encoders;
    };
}    // namespace

TEST_F(EncodersTest, StateDecoding)
{
    if (!encoders::Collection::SIZE())
    {
        return;
    }

    auto verifyValue = [&](midi::messageType_t message, uint16_t value)
    {
        for (size_t i = 0; i < encoders::Collection::SIZE(); i++)
        {
            ASSERT_EQ(message, _listener._event.at(i).message);
            ASSERT_EQ(value, _listener._event.at(i).value);
        }
    };

    // test expected permutations for ccw and cw directions

    // clockwise: 00, 10, 11, 01

    stateChangeRegister(0b00);
    ASSERT_EQ(0, _listener._event.size());

    stateChangeRegister(0b10);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);

    stateChangeRegister(0b11);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);

    stateChangeRegister(0b01);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);

    stateChangeRegister(0b10);
    ASSERT_EQ(0, _listener._event.size());

    stateChangeRegister(0b11);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);

    stateChangeRegister(0b01);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);

    stateChangeRegister(0b00);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);

    stateChangeRegister(0b11);
    ASSERT_EQ(0, _listener._event.size());

    stateChangeRegister(0b01);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);

    stateChangeRegister(0b00);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);

    stateChangeRegister(0b10);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);

    stateChangeRegister(0b01);
    ASSERT_EQ(0, _listener._event.size());

    stateChangeRegister(0b00);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);

    stateChangeRegister(0b10);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);

    stateChangeRegister(0b11);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);

    // counter-clockwise: 00, 01, 11, 10

    stateChangeRegister(0b00);
    ASSERT_EQ(0, _listener._event.size());

    stateChangeRegister(0b01);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 127);

    stateChangeRegister(0b11);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 127);

    stateChangeRegister(0b10);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 127);

    stateChangeRegister(0b01);
    ASSERT_EQ(0, _listener._event.size());

    stateChangeRegister(0b11);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 127);

    stateChangeRegister(0b10);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 127);

    stateChangeRegister(0b00);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 127);

    stateChangeRegister(0b11);
    ASSERT_EQ(0, _listener._event.size());

    stateChangeRegister(0b10);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 127);

    stateChangeRegister(0b00);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 127);

    stateChangeRegister(0b01);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 127);

    stateChangeRegister(0b10);
    ASSERT_EQ(0, _listener._event.size());

    stateChangeRegister(0b00);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 127);

    stateChangeRegister(0b01);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 127);

    stateChangeRegister(0b11);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 127);

    // this time configure 4 pulses per step
    for (size_t i = 0; i < encoders::Collection::SIZE(); i++)
    {
        ASSERT_TRUE(_encoders._database.update(database::Config::Section::encoder_t::PULSES_PER_STEP, i, 4));
        _encoders._instance.reset(i);
    }

    // clockwise: 00, 10, 11, 01

    // initial state doesn't count as pulse, 4 more needed
    stateChangeRegister(0b00);
    ASSERT_EQ(0, _listener._event.size());

    // 1
    stateChangeRegister(0b10);
    ASSERT_EQ(0, _listener._event.size());

    // 2
    stateChangeRegister(0b11);
    ASSERT_EQ(0, _listener._event.size());

    // 3
    stateChangeRegister(0b01);
    ASSERT_EQ(0, _listener._event.size());

    // 4
    // pulse should be registered
    stateChangeRegister(0b00);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);

    // 1
    stateChangeRegister(0b10);
    ASSERT_EQ(0, _listener._event.size());

    // 2
    stateChangeRegister(0b11);
    ASSERT_EQ(0, _listener._event.size());

    // 3
    stateChangeRegister(0b01);
    ASSERT_EQ(0, _listener._event.size());

    // 4
    stateChangeRegister(0b00);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);

    // now move to opposite direction
    // don't start from 0b00 state again
    // counter-clockwise: 01, 11, 10, 00

    stateChangeRegister(0b01);
    ASSERT_EQ(0, _listener._event.size());

    stateChangeRegister(0b11);
    ASSERT_EQ(0, _listener._event.size());

    stateChangeRegister(0b10);
    ASSERT_EQ(0, _listener._event.size());

    stateChangeRegister(0b00);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 127);
}

TEST_F(EncodersTest, Messages)
{
    if (!encoders::Collection::SIZE())
    {
        return;
    }

    constexpr size_t PULSES_PER_STEP = 4;

    auto setup = [&](encoders::type_t type)
    {
        for (size_t i = 0; i < encoders::Collection::SIZE(); i++)
        {
            ASSERT_TRUE(_encoders._database.update(database::Config::Section::encoder_t::PULSES_PER_STEP, i, PULSES_PER_STEP));
            ASSERT_TRUE(_encoders._database.update(database::Config::Section::encoder_t::MODE, i, type));
            _encoders._instance.reset(i);

            // simulate initial reading
            stateChangeRegister(0b00);
        }
    };

    auto verifyValue = [&](midi::messageType_t message, uint16_t value)
    {
        for (size_t i = 0; i < encoders::Collection::SIZE(); i++)
        {
            ASSERT_EQ(message, _listener._event.at(i).message);
            ASSERT_EQ(value, _listener._event.at(i).value);
        }
    };

    auto rotate = [this](bool clockwise)
    {
        // skip the initial state
        size_t stateIndex = 0;

        auto nextValue = [&]()
        {
            if (clockwise)
            {
                stateIndex++;
            }
            else
            {
                stateIndex--;
            }

            stateIndex %= ENCODER_STATE.size();
            return ENCODER_STATE.at(stateIndex);
        };

        for (size_t pulse = 0; pulse < PULSES_PER_STEP; pulse++)
        {
            stateChangeRegister(nextValue());
        }
    };

    setup(encoders::type_t::CONTROL_CHANGE_7FH01H);
    rotate(true);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);
    rotate(false);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 127);

    setup(encoders::type_t::CONTROL_CHANGE_3FH41H);
    rotate(true);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 65);
    rotate(false);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 63);

    setup(encoders::type_t::CONTROL_CHANGE_41H01H);
    rotate(true);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 1);
    rotate(false);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::CONTROL_CHANGE, 65);

    setup(encoders::type_t::SINGLE_NOTE_VARIABLE_VAL);
    rotate(true);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::NOTE_ON, 1);
    rotate(false);
    ASSERT_EQ(encoders::Collection::SIZE(), _listener._event.size());
    verifyValue(midi::messageType_t::NOTE_ON, 0);
}

#endif