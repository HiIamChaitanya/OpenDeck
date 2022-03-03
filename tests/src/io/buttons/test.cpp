#include "unity/Framework.h"
#include "io/buttons/Buttons.h"
#include "io/leds/LEDs.h"
#include "core/src/general/Timing.h"
#include "database/Database.h"
#include "stubs/database/DB_ReadWrite.h"
#include "stubs/ButtonsFilter.h"
#include "stubs/HWALEDs.h"
#include "stubs/Listener.h"

#ifdef BUTTONS_SUPPORTED

namespace
{
    class HWAButtons : public IO::Buttons::HWA
    {
        public:
        HWAButtons() {}

        bool state(size_t index, uint8_t& numberOfReadings, uint32_t& states) override
        {
            numberOfReadings = 1;
            states           = _state[index];
            return true;
        }

        size_t buttonToEncoderIndex(size_t index) override
        {
            return index / 2;
        }

        bool _state[IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS)] = {};
    } _hwaButtons;

    Listener          _listener;
    DBstorageMock     _dbStorageMock;
    Database          _database = Database(_dbStorageMock, true);
    HWALEDsStub       _hwaLEDs;
    IO::LEDs          _leds(_hwaLEDs, _database);
    ButtonsFilterStub _buttonsFilter;
    IO::Buttons       _buttons = IO::Buttons(_hwaButtons, _buttonsFilter, _database);

    void stateChangeRegister(bool state)
    {
        _listener._event.clear();

        for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
            _hwaButtons._state[i] = state;

        _buttons.updateAll();
    }
}    // namespace

TEST_SETUP()
{
    // init checks - no point in running further tests if these conditions fail
    TEST_ASSERT(_database.init() == true);

    // always start from known state
    _database.factoryReset();

    static bool listenerActive = false;

    if (!listenerActive)
    {
        MIDIDispatcher.listen(Messaging::eventSource_t::buttons,
                              Messaging::listenType_t::nonFwd,
                              [](const Messaging::event_t& dispatchMessage) {
                                  _listener.messageListener(dispatchMessage);
                              });

        listenerActive = true;
        _leds.init();
    }

    _listener._event.clear();
}

TEST_CASE(Note)
{
    if (IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS))
    {
        using namespace IO;

        auto test = [&](uint8_t channel, uint8_t velocityValue) {
            // set known state
            for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
            {
                // configure all buttons as momentary
                TEST_ASSERT(_database.update(Database::Section::button_t::type, i, Buttons::type_t::momentary) == true);

                // make all buttons send note messages
                TEST_ASSERT(_database.update(Database::Section::button_t::midiMessage, i, Buttons::messageType_t::note) == true);

                // set passed velocity value
                TEST_ASSERT(_database.update(Database::Section::button_t::velocity, i, velocityValue) == true);

                _database.update(Database::Section::button_t::midiChannel, i, channel);
                _buttons.reset(i);
            }

            auto verifyValue = [&](bool state) {
                // verify all received messages
                for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
                {
                    if (state)
                    {
                        TEST_ASSERT_EQUAL_UINT32(MIDI::messageType_t::noteOn, _listener._event.at(i).message);
                    }
                    else
                    {
                        TEST_ASSERT_EQUAL_UINT32(MIDI::messageType_t::noteOff, _listener._event.at(i).message);
                    }

                    TEST_ASSERT_EQUAL_UINT32(state ? velocityValue : 0, _listener._event.at(i).midiValue);
                    TEST_ASSERT_EQUAL_UINT32(channel, _listener._event.at(i).midiChannel);

                    // also verify MIDI ID
                    // it should be equal to button ID by default
                    TEST_ASSERT_EQUAL_UINT32(i, _listener._event.at(i).midiIndex);
                }
            };

            // simulate button press
            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());
            verifyValue(true);

            // simulate button release
            stateChangeRegister(false);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());
            verifyValue(false);

            // try with the latching mode
            for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
                TEST_ASSERT(_database.update(Database::Section::button_t::type, i, Buttons::type_t::latching) == true);

            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());
            verifyValue(true);

            // nothing should happen on release
            stateChangeRegister(false);
            TEST_ASSERT_EQUAL_UINT32(0, _listener._event.size());

            // press again, new messages should arrive
            stateChangeRegister(true);
            verifyValue(false);
        };

        // test for all velocity/channel values
        for (int channel = 1; channel <= 16; channel++)
        {
            for (int velocity = 1; velocity <= 127; velocity++)
            {
                test(channel, velocity);
            }
        }
    }
}

TEST_CASE(ProgramChange)
{
    if (IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS))
    {
        using namespace IO;

        auto tesprogramChange = [&](uint8_t channel) {
            // set known state
            for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
            {
                // configure all buttons as momentary
                TEST_ASSERT(_database.update(Database::Section::button_t::type, i, Buttons::type_t::momentary) == true);

                // make all buttons send program change messages
                TEST_ASSERT(_database.update(Database::Section::button_t::midiMessage, i, Buttons::messageType_t::programChange) == true);

                // configure the specifed midi channel
                TEST_ASSERT(_database.update(Database::Section::button_t::midiChannel, i, channel) == true);

                _buttons.reset(i);
            }

            auto verifyMessage = [&]() {
                // verify all received messages are program change
                for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
                {
                    TEST_ASSERT_EQUAL_UINT32(MIDI::messageType_t::programChange, _listener._event.at(i).message);

                    // program change value should always be set to 0
                    TEST_ASSERT_EQUAL_UINT32(0, _listener._event.at(i).midiValue);

                    // verify channel
                    TEST_ASSERT_EQUAL_UINT32(channel, _listener._event.at(i).midiChannel);

                    // also verify MIDI ID/program
                    // it should be equal to button ID by default
                    TEST_ASSERT_EQUAL_UINT32(i, _listener._event.at(i).midiIndex);
                }
            };

            // simulate button press
            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());
            verifyMessage();

            // program change shouldn't be sent on release
            stateChangeRegister(false);
            TEST_ASSERT_EQUAL_UINT32(0, _listener._event.size());

            // repeat the entire test again, but with buttons configured as latching types
            // behaviour should be the same
            for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
                TEST_ASSERT(_database.update(Database::Section::button_t::type, i, Buttons::type_t::latching) == true);

            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());
            verifyMessage();

            stateChangeRegister(false);
            TEST_ASSERT_EQUAL_UINT32(0, _listener._event.size());
        };

        // test for all channels
        for (int i = 1; i <= 16; i++)
            tesprogramChange(i);

        // test programChangeInc/programChangeDec
        _database.factoryReset();
        stateChangeRegister(false);

        auto configurePCbutton = [&](size_t index, uint8_t channel, bool increase) {
            TEST_ASSERT(_database.update(Database::Section::button_t::type, index, Buttons::type_t::momentary) == true);
            TEST_ASSERT(_database.update(Database::Section::button_t::midiMessage, index, increase ? Buttons::messageType_t::programChangeInc : Buttons::messageType_t::programChangeDec) == true);
            TEST_ASSERT(_database.update(Database::Section::button_t::midiChannel, index, channel) == true);

            for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
                _buttons.reset(i);
        };

        auto verifyProgramChange = [&](size_t index, uint8_t channel, uint8_t program) {
            TEST_ASSERT_EQUAL_UINT32(MIDI::messageType_t::programChange, _listener._event.at(index).message);

            // program change value should always be set to 0
            TEST_ASSERT_EQUAL_UINT32(0, _listener._event.at(index).midiValue);

            // verify channel
            TEST_ASSERT_EQUAL_UINT32(channel, _listener._event.at(index).midiChannel);

            TEST_ASSERT_EQUAL_UINT32(program, _listener._event.at(index).midiIndex);
        };

        const uint8_t CHANNEL = 1;

        configurePCbutton(0, CHANNEL, true);

        // verify that the received program change was 1 for button 0
        stateChangeRegister(true);
        verifyProgramChange(0, CHANNEL, 1);
        stateChangeRegister(false);

        // after this, verify that the received program change was 2 for button 0
        stateChangeRegister(true);
        verifyProgramChange(0, CHANNEL, 2);
        stateChangeRegister(false);

        // after this, verify that the received program change was 3 for button 0
        stateChangeRegister(true);
        verifyProgramChange(0, CHANNEL, 3);
        stateChangeRegister(false);

        // now, revert all buttons back to default
        _database.factoryReset();

        if (IO::Buttons::Collection::size() > 1)
        {
            // configure some other button to programChangeInc
            configurePCbutton(4, CHANNEL, true);

            // verify that the program change is continuing to increase
            stateChangeRegister(true);
            verifyProgramChange(4, CHANNEL, 4);
            stateChangeRegister(false);

            stateChangeRegister(true);
            verifyProgramChange(4, CHANNEL, 5);
            stateChangeRegister(false);

            // now configure two buttons to send program change/inc
            configurePCbutton(0, CHANNEL, true);

            stateChangeRegister(true);
            // program change should be increased by 1, first by button 0
            verifyProgramChange(0, CHANNEL, 6);
            // then by button 4
            verifyProgramChange(4, CHANNEL, 7);
            stateChangeRegister(false);

            // configure another button to programChangeInc, but on other channel
            configurePCbutton(1, 4, true);

            stateChangeRegister(true);
            // program change should be increased by 1, first by button 0
            verifyProgramChange(0, CHANNEL, 8);
            // then by button 4
            verifyProgramChange(4, CHANNEL, 9);
            // program change should be sent on channel 4 by button 1
            verifyProgramChange(1, 4, 1);
            stateChangeRegister(false);

            // revert to default again
            _database.factoryReset();

            // now configure button 0 for programChangeDec
            configurePCbutton(0, CHANNEL, false);

            stateChangeRegister(true);
            // program change should decrease by 1
            verifyProgramChange(0, CHANNEL, 8);
            stateChangeRegister(false);

            stateChangeRegister(true);
            // program change should decrease by 1 again
            verifyProgramChange(0, CHANNEL, 7);
            stateChangeRegister(false);

            // configure another button for programChangeDec
            configurePCbutton(1, CHANNEL, false);

            stateChangeRegister(true);
            // button 0 should decrease the value by 1
            verifyProgramChange(0, CHANNEL, 6);
            // button 1 should decrease it again
            verifyProgramChange(1, CHANNEL, 5);
            stateChangeRegister(false);

            // configure another button for programChangeDec
            configurePCbutton(2, CHANNEL, false);

            stateChangeRegister(true);
            // button 0 should decrease the value by 1
            verifyProgramChange(0, CHANNEL, 4);
            // button 1 should decrease it again
            verifyProgramChange(1, CHANNEL, 3);
            // button 2 should decrease it again
            verifyProgramChange(2, CHANNEL, 2);
            stateChangeRegister(false);

            // reset all received messages first
            _listener._event.clear();

            // only two program change messages should be sent
            // program change value is 0 after the second button decreases it
            // once the value is 0 no further messages should be sent in dec mode

            stateChangeRegister(true);
            // button 0 should decrease the value by 1
            verifyProgramChange(0, CHANNEL, 1);
            // button 1 should decrease it again
            verifyProgramChange(1, CHANNEL, 0);

            // verify that only two program change messages have been received
            uint8_t pcCounter = 0;

            for (int i = 0; i < _listener._event.size(); i++)
            {
                if (_listener._event.at(i).message == MIDI::messageType_t::programChange)
                    pcCounter++;
            }

            TEST_ASSERT(pcCounter == 2);

            stateChangeRegister(false);

            // revert all buttons to default
            _database.factoryReset();

            configurePCbutton(0, CHANNEL, true);

            stateChangeRegister(true);
            // button 0 should increase the last value by 1
            verifyProgramChange(0, CHANNEL, 1);
            stateChangeRegister(false);
        }
    }
}

TEST_CASE(ControlChange)
{
    if (IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS))
    {
        using namespace IO;

        auto controlChangeTest = [&](uint8_t controlValue) {
            // set known state
            for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
            {
                // configure all buttons as momentary
                TEST_ASSERT(_database.update(Database::Section::button_t::type, i, Buttons::type_t::momentary) == true);

                // make all buttons send control change messages
                TEST_ASSERT(_database.update(Database::Section::button_t::midiMessage, i, Buttons::messageType_t::controlChange) == true);

                // set passed control value
                TEST_ASSERT(_database.update(Database::Section::button_t::velocity, i, controlValue) == true);

                _buttons.reset(i);
            }

            auto verifyMessage = [&](uint8_t midiValue) {
                // verify all received messages are control change
                for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
                {
                    TEST_ASSERT_EQUAL_UINT32(MIDI::messageType_t::controlChange, _listener._event.at(i).message);
                    TEST_ASSERT_EQUAL_UINT32(midiValue, _listener._event.at(i).midiValue);
                    TEST_ASSERT_EQUAL_UINT32(1, _listener._event.at(i).midiChannel);
                    TEST_ASSERT_EQUAL_UINT32(i, _listener._event.at(i).midiIndex);
                }
            };

            // simulate button press
            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());

            verifyMessage(controlValue);

            // no messages should be sent on release
            stateChangeRegister(false);
            TEST_ASSERT_EQUAL_UINT32(0, _listener._event.size());

            // change to latching type
            // behaviour should be the same

            for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
                TEST_ASSERT(_database.update(Database::Section::button_t::type, i, Buttons::type_t::latching) == true);

            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());

            verifyMessage(controlValue);

            stateChangeRegister(false);
            TEST_ASSERT_EQUAL_UINT32(0, _listener._event.size());

            // change type to control change with 0 on reset, momentary mode
            // this means CC value 0 should be sent on release

            for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
                TEST_ASSERT(_database.update(Database::Section::button_t::type, i, Buttons::type_t::momentary) == true);

            for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
                TEST_ASSERT(_database.update(Database::Section::button_t::midiMessage, i, Buttons::messageType_t::controlChangeReset) == true);

            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());

            verifyMessage(controlValue);

            stateChangeRegister(false);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());

            verifyMessage(0);

            // same test again, but in latching mode
            // now, on press, messages should be sent
            // on release, nothing should happen
            // on second press reset should be sent (CC with value 0)

            for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
                TEST_ASSERT(_database.update(Database::Section::button_t::type, i, Buttons::type_t::latching) == true);

            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());

            verifyMessage(controlValue);

            stateChangeRegister(false);
            TEST_ASSERT_EQUAL_UINT32(0, _listener._event.size());

            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());

            verifyMessage(0);
        };

        // verify with all control values
        // value 0 is normally blocked to configure for users (via sysex)
        for (int i = 1; i < 128; i++)
            controlChangeTest(i);
    }
}

TEST_CASE(NoMessages)
{
    if (IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS))
    {
        using namespace IO;

        // configure all buttons to messageType_t::none so that messages aren't sent

        for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
        {
            // configure all buttons as momentary
            TEST_ASSERT(_database.update(Database::Section::button_t::type, i, Buttons::type_t::momentary) == true);

            // don't send any message
            TEST_ASSERT(_database.update(Database::Section::button_t::midiMessage, i, Buttons::messageType_t::none) == true);

            _buttons.reset(i);
        }

        stateChangeRegister(true);
        TEST_ASSERT_EQUAL_UINT32(0, _listener._event.size());

        stateChangeRegister(false);
        TEST_ASSERT_EQUAL_UINT32(0, _listener._event.size());

        for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
            TEST_ASSERT(_database.update(Database::Section::button_t::type, i, Buttons::type_t::latching) == true);

        stateChangeRegister(true);
        TEST_ASSERT_EQUAL_UINT32(0, _listener._event.size());

        stateChangeRegister(false);
        TEST_ASSERT_EQUAL_UINT32(0, _listener._event.size());

        stateChangeRegister(true);
        TEST_ASSERT_EQUAL_UINT32(0, _listener._event.size());

        stateChangeRegister(false);
        TEST_ASSERT_EQUAL_UINT32(0, _listener._event.size());
    }
}

#ifdef LEDS_SUPPORTED
TEST_CASE(LocalLEDcontrol)
{
    if (IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS))
    {
        if (IO::LEDs::Collection::size())
        {
            using namespace IO;

            for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
            {
                // configure all buttons as momentary
                TEST_ASSERT(_database.update(Database::Section::button_t::type, i, Buttons::type_t::momentary) == true);

                // send note messages
                TEST_ASSERT(_database.update(Database::Section::button_t::midiMessage, i, Buttons::messageType_t::note) == true);

                TEST_ASSERT(_database.update(Database::Section::button_t::velocity, i, 127) == true);

                _buttons.reset(i);
            }

            // configure one of the leds in local control mode
            TEST_ASSERT(_database.update(Database::Section::leds_t::controlType, 0, LEDs::controlType_t::localNoteSingleVal) == true);
            // set 127 as activation value, 0 as activation ID
            TEST_ASSERT(_database.update(Database::Section::leds_t::activationValue, 0, 127) == true);
            TEST_ASSERT(_database.update(Database::Section::leds_t::activationID, 0, 0) == true);

            // all leds should be off initially
            for (int i = 0; i < IO::LEDs::Collection::size(IO::LEDs::GROUP_DIGITAL_OUTPUTS); i++)
                TEST_ASSERT(_leds.color(i) == LEDs::color_t::off);

            // simulate the press of all buttons
            // since led 0 is configured in local control mode, it should be on now
            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());

            TEST_ASSERT(_leds.color(0) != LEDs::color_t::off);

            // all other leds should remain off
            for (int i = 1; i < IO::LEDs::Collection::size(IO::LEDs::GROUP_DIGITAL_OUTPUTS); i++)
                TEST_ASSERT(_leds.color(i) == LEDs::color_t::off);

            // now release the button and verify that the led is off again
            stateChangeRegister(false);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());

            for (int i = 0; i < IO::LEDs::Collection::size(IO::LEDs::GROUP_DIGITAL_OUTPUTS); i++)
                TEST_ASSERT(_leds.color(i) == LEDs::color_t::off);

            // test again in latching mode
            for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
                TEST_ASSERT(_database.update(Database::Section::button_t::type, i, Buttons::type_t::latching) == true);

            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());

            TEST_ASSERT(_leds.color(0) != LEDs::color_t::off);

            stateChangeRegister(false);
            TEST_ASSERT_EQUAL_UINT32(0, _listener._event.size());

            // led should remain on
            TEST_ASSERT(_leds.color(0) != LEDs::color_t::off);

            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());

            TEST_ASSERT(_leds.color(0) == LEDs::color_t::off);

            // test again in control change mode
            for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
            {
                // configure all buttons as momentary
                TEST_ASSERT(_database.update(Database::Section::button_t::type, i, Buttons::type_t::momentary) == true);

                // send cc messages
                TEST_ASSERT(_database.update(Database::Section::button_t::midiMessage, i, Buttons::messageType_t::controlChange) == true);

                _buttons.reset(i);
            }

            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());

            // led should be off since it's configure to react on note messages and not on control change
            TEST_ASSERT(_leds.color(0) == LEDs::color_t::off);

            TEST_ASSERT(_database.update(Database::Section::leds_t::controlType, 0, LEDs::controlType_t::localCCSingleVal) == true);

            // no messages being sent on release in CC mode
            stateChangeRegister(false);
            TEST_ASSERT_EQUAL_UINT32(0, _listener._event.size());

            // nothing should happen on release yet
            TEST_ASSERT(_leds.color(0) == LEDs::color_t::off);

            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());

            // led should be on now
            TEST_ASSERT(_leds.color(0) != LEDs::color_t::off);

            stateChangeRegister(false);
            TEST_ASSERT_EQUAL_UINT32(0, _listener._event.size());

            // no messages sent - led must remain on
            TEST_ASSERT(_leds.color(0) != LEDs::color_t::off);

            // change the control value for button 0 to something else
            TEST_ASSERT(_database.update(Database::Section::button_t::velocity, 0, 126) == true);

            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());

            // led should be off now - it has received velocity 126 differing from activating one which is 127
            TEST_ASSERT(_leds.color(0) == LEDs::color_t::off);

            // try similar thing - cc with reset 0
            for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
            {
                // configure all buttons as momentary
                TEST_ASSERT(_database.update(Database::Section::button_t::type, i, Buttons::type_t::momentary) == true);

                // send cc messages with reset
                TEST_ASSERT(_database.update(Database::Section::button_t::midiMessage, i, Buttons::messageType_t::controlChangeReset) == true);

                TEST_ASSERT(_database.update(Database::Section::button_t::velocity, i, 127) == true);

                _buttons.reset(i);
            }

            stateChangeRegister(true);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());

            TEST_ASSERT(_leds.color(0) != LEDs::color_t::off);

            stateChangeRegister(false);
            TEST_ASSERT_EQUAL_UINT32(IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS), _listener._event.size());

            // regression test
            // configure LED 0 in midiInNoteMultiVal mode, activation value to 127, activation ID to 0
            // press button 0 (momentary mode, midi notes)
            // verify that the state of led hasn't been changed
            TEST_ASSERT(_database.update(Database::Section::leds_t::controlType, 0, LEDs::controlType_t::midiInNoteMultiVal) == true);
            TEST_ASSERT(_database.update(Database::Section::leds_t::activationValue, 0, 127) == true);
            TEST_ASSERT(_database.update(Database::Section::leds_t::activationID, 0, 0) == true);

            for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
            {
                TEST_ASSERT(_database.update(Database::Section::button_t::type, i, Buttons::type_t::momentary) == true);
                TEST_ASSERT(_database.update(Database::Section::button_t::midiMessage, i, Buttons::messageType_t::note) == true);
                TEST_ASSERT(_database.update(Database::Section::button_t::velocity, i, 127) == true);

                _buttons.reset(i);
            }

            TEST_ASSERT(_leds.color(0) == LEDs::color_t::off);
            stateChangeRegister(true);
            TEST_ASSERT(_leds.color(0) == LEDs::color_t::off);
            stateChangeRegister(false);
            TEST_ASSERT(_leds.color(0) == LEDs::color_t::off);

            // again with midiInCCMultiVal
            TEST_ASSERT(_database.update(Database::Section::leds_t::controlType, 0, LEDs::controlType_t::midiInCCMultiVal) == true);
            stateChangeRegister(true);
            TEST_ASSERT(_leds.color(0) == LEDs::color_t::off);
            stateChangeRegister(false);
            TEST_ASSERT(_leds.color(0) == LEDs::color_t::off);

            if (IO::LEDs::Collection::size() > 1)
            {
                for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
                    _buttons.reset(i);

                // test program change
                TEST_ASSERT(_database.update(Database::Section::leds_t::controlType, 0, LEDs::controlType_t::pcSingleVal) == true);
                stateChangeRegister(true);
                // led should remain in off state since none of the buttons use program change for message
                TEST_ASSERT(_leds.color(0) == LEDs::color_t::off);

                for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
                    _buttons.reset(i);

                // configure button in program change mode - this should trigger on state for LED 0
                TEST_ASSERT(_database.update(Database::Section::button_t::midiMessage, 0, Buttons::messageType_t::programChange) == true);
                stateChangeRegister(true);
                TEST_ASSERT(_leds.color(0) == LEDs::color_t::red);

                for (int i = 0; i < IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS); i++)
                    _buttons.reset(i);

                // configure second LED and button in program change mode
                TEST_ASSERT(_database.update(Database::Section::leds_t::controlType, 1, LEDs::controlType_t::pcSingleVal) == true);
                TEST_ASSERT(_database.update(Database::Section::button_t::midiMessage, 1, Buttons::messageType_t::programChange) == true);

                stateChangeRegister(true);

                // program change 0 and 1 are triggered: first 0, then 1
                // initially, led 0 is on, but then program change 1 is received
                // program change message should just turn the LEDs with cooresponding activation ID, the rest should be off
                TEST_ASSERT(_leds.color(0) == LEDs::color_t::off);
                TEST_ASSERT(_leds.color(1) == LEDs::color_t::red);
            }
        }
    }
}
#endif

TEST_CASE(PresetChange)
{
    if (_database.getSupportedPresets() <= 1)
        return;

    if (IO::Buttons::Collection::size(IO::Buttons::GROUP_DIGITAL_INPUTS))
    {
        using namespace IO;

        class DBhandlers : public Database::Handlers
        {
            public:
            DBhandlers() = default;

            void presetChange(uint8_t preset) override
            {
                _preset = preset;
            }

            void factoryResetStart() override
            {
            }

            void factoryResetDone() override
            {
            }

            void initialized() override
            {
            }

            uint8_t _preset = 0;
        } _dbHandlers;

        _database.registerHandlers(_dbHandlers);

        // configure one button to change preset and set its index to 1
        const size_t index = 0;
        TEST_ASSERT(_database.update(Database::Section::button_t::midiID, index, 1) == true);
        TEST_ASSERT(_database.update(Database::Section::button_t::midiMessage, index, Buttons::messageType_t::presetOpenDeck) == true);
        _buttons.reset(index);

        // simulate button press
        stateChangeRegister(true);
        TEST_ASSERT_EQUAL_UINT32(1, _dbHandlers._preset);
        _dbHandlers._preset = 0;

        // verify that preset is unchanged after the button is released
        stateChangeRegister(false);
        TEST_ASSERT_EQUAL_UINT32(0, _dbHandlers._preset);
    }
}

#endif