/*

Copyright 2015-2022 Igor Petrovic

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

#ifdef UART_SUPPORTED

#include <map>
#include "board/Board.h"
#include "board/Internal.h"
#include "nrfx_uarte.h"
#include <Target.h>

namespace
{
    std::map<uint32_t, nrf_uarte_baudrate_t> _baudRateMap = {
        {
            1200,
            NRF_UARTE_BAUDRATE_1200,
        },
        {
            2400,
            NRF_UARTE_BAUDRATE_2400,
        },
        {
            4800,
            NRF_UARTE_BAUDRATE_4800,
        },
        {
            9600,
            NRF_UARTE_BAUDRATE_9600,
        },
        {
            14400,
            NRF_UARTE_BAUDRATE_14400,
        },
        {
            19200,
            NRF_UARTE_BAUDRATE_19200,
        },
        {
            28800,
            NRF_UARTE_BAUDRATE_28800,
        },
        {
            31250,
            NRF_UARTE_BAUDRATE_31250,
        },
        {
            38400,
            NRF_UARTE_BAUDRATE_38400,
        },
        {
            56000,
            NRF_UARTE_BAUDRATE_56000,
        },
        {
            57600,
            NRF_UARTE_BAUDRATE_57600,
        },
        {
            76800,
            NRF_UARTE_BAUDRATE_76800,
        },
        {
            115200,
            NRF_UARTE_BAUDRATE_115200,
        },
        {
            230400,
            NRF_UARTE_BAUDRATE_230400,
        },
        {
            250000,
            NRF_UARTE_BAUDRATE_250000,
        },
        {
            460800,
            NRF_UARTE_BAUDRATE_460800,
        },
        {
            921600,
            NRF_UARTE_BAUDRATE_921600,
        },
        {
            1000000,
            NRF_UARTE_BAUDRATE_1000000,
        },
    };

    uint8_t       _nrfTxBuffer[core::mcu::peripherals::MAX_UART_INTERFACES];
    uint8_t       _nrfRxBuffer[core::mcu::peripherals::MAX_UART_INTERFACES];
    volatile bool _transmitting[core::mcu::peripherals::MAX_UART_INTERFACES];
    nrfx_uarte_t  _uartInstance[core::mcu::peripherals::MAX_UART_INTERFACES] = {
         NRFX_UARTE_INSTANCE(0),
         NRFX_UARTE_INSTANCE(1),
    };

    inline void checkTx(uint8_t channel)
    {
        size_t  remainingBytes;
        uint8_t value;

        if (Board::detail::UART::getNextByteToSend(channel, value, remainingBytes))
        {
            _nrfTxBuffer[channel] = value;
            nrf_uarte_event_clear(_uartInstance[channel].p_reg, NRF_UARTE_EVENT_ENDTX);
            nrf_uarte_event_clear(_uartInstance[channel].p_reg, NRF_UARTE_EVENT_TXSTOPPED);
            nrf_uarte_tx_buffer_set(_uartInstance[channel].p_reg, &_nrfTxBuffer[channel], 1);
            _transmitting[channel] = true;
            nrf_uarte_task_trigger(_uartInstance[channel].p_reg, NRF_UARTE_TASK_STARTTX);
        }
        else
        {
            // Transmitter has to be stopped by triggering STOPTX task to achieve
            // the lowest possible level of the UARTE power consumption.

            _transmitting[channel] = false;

            nrf_uarte_event_clear(_uartInstance[channel].p_reg, NRF_UARTE_EVENT_TXSTOPPED);
            nrf_uarte_task_trigger(_uartInstance[channel].p_reg, NRF_UARTE_TASK_STOPTX);
        }
    }
}    // namespace

namespace Board::detail::UART::MCU
{
    void startTx(uint8_t channel)
    {
        // On NRF52, there is no TX Empty interrupt - only TX ready
        // interrupt, which occurs after the data has been sent.
        // In order to trigger the interrupt initially, STARTTX task must be triggered
        // and data needs to be written to internal NRF buffer.
        // Do this only if current data transfer has completed.

        if (!_transmitting[channel])
        {
            checkTx(channel);
        }
    }

    bool isTxComplete(uint8_t channel)
    {
        return !_transmitting[channel];
    }

    bool deInit(uint8_t channel)
    {
        nrf_uarte_int_disable(_uartInstance[channel].p_reg,
                              NRF_UARTE_INT_ENDRX_MASK |
                                  NRF_UARTE_INT_ENDTX_MASK |
                                  NRF_UARTE_INT_ERROR_MASK |
                                  NRF_UARTE_INT_RXTO_MASK |
                                  NRF_UARTE_INT_TXSTOPPED_MASK);

        NRFX_IRQ_DISABLE(nrfx_get_irq_number((void*)_uartInstance[channel].p_reg));

        nrf_uarte_disable(_uartInstance[channel].p_reg);

        CORE_MCU_IO_DEINIT(Board::detail::map::uartPins(channel).tx);
        CORE_MCU_IO_DEINIT(Board::detail::map::uartPins(channel).rx);

        return true;
    }

    bool init(uint8_t channel, Board::UART::config_t& config)
    {
        // unsupported by NRF52
        if (config.parity == Board::UART::parity_t::ODD)
        {
            return false;
        }

        if (!_baudRateMap[config.baudRate])
        {
            return false;
        }

        CORE_MCU_IO_SET_HIGH(Board::detail::map::uartPins(channel).tx.port, Board::detail::map::uartPins(channel).tx.index);

        CORE_MCU_IO_INIT(Board::detail::map::uartPins(channel).tx.port,
                         Board::detail::map::uartPins(channel).tx.index,
                         core::mcu::io::pinMode_t::OUTPUT_PP,
                         core::mcu::io::pullMode_t::NONE);

        CORE_MCU_IO_INIT(Board::detail::map::uartPins(channel).rx.port,
                         Board::detail::map::uartPins(channel).rx.index,
                         core::mcu::io::pinMode_t::INPUT,
                         core::mcu::io::pullMode_t::NONE);

        nrfx_uarte_config_t nrfUartConfig = {};
        nrfUartConfig.hal_cfg             = {
                        .hwfc   = NRF_UARTE_HWFC_DISABLED,
                        .parity = config.parity ? NRF_UARTE_PARITY_INCLUDED : NRF_UARTE_PARITY_EXCLUDED
        };

        nrf_uarte_baudrate_set(_uartInstance[channel].p_reg, _baudRateMap[config.baudRate]);
        nrf_uarte_configure(_uartInstance[channel].p_reg, &nrfUartConfig.hal_cfg);
        nrf_uarte_txrx_pins_set(_uartInstance[channel].p_reg,
                                CORE_NRF_GPIO_PIN_MAP(Board::detail::map::uartPins(channel).tx.port,
                                                      Board::detail::map::uartPins(channel).tx.index),
                                CORE_NRF_GPIO_PIN_MAP(Board::detail::map::uartPins(channel).rx.port,
                                                      Board::detail::map::uartPins(channel).rx.index));

        nrf_uarte_int_enable(_uartInstance[channel].p_reg,
                             NRF_UARTE_INT_ENDRX_MASK |
                                 NRF_UARTE_INT_ENDTX_MASK |
                                 NRF_UARTE_INT_TXSTOPPED_MASK);

        NRFX_IRQ_PRIORITY_SET(nrfx_get_irq_number((void*)_uartInstance[channel].p_reg), IRQ_PRIORITY_UART);
        NVIC_ClearPendingIRQ(nrfx_get_irq_number((void*)_uartInstance[channel].p_reg));
        NRFX_IRQ_ENABLE(nrfx_get_irq_number((void*)_uartInstance[channel].p_reg));

        nrf_uarte_rx_buffer_set(_uartInstance[channel].p_reg, &_nrfRxBuffer[channel], 1);
        nrf_uarte_enable(_uartInstance[channel].p_reg);
        nrf_uarte_task_trigger(_uartInstance[channel].p_reg, NRF_UARTE_TASK_STARTRX);

        return true;
    }
}    // namespace Board::detail::UART::MCU

void core::mcu::isr::uart(uint8_t channel)
{
    if (nrf_uarte_event_check(_uartInstance[channel].p_reg, NRF_UARTE_EVENT_ERROR))
    {
        nrf_uarte_event_clear(_uartInstance[channel].p_reg, NRF_UARTE_EVENT_ERROR);
        nrf_uarte_errorsrc_get_and_clear(_uartInstance[channel].p_reg);
    }
    else if (nrf_uarte_event_check(_uartInstance[channel].p_reg, NRF_UARTE_EVENT_ENDRX))
    {
        // receive buffer is filled up

        nrf_uarte_event_clear(_uartInstance[channel].p_reg, NRF_UARTE_EVENT_ENDRX);
        Board::detail::UART::storeIncomingData(channel, _nrfRxBuffer[channel]);
        nrf_uarte_task_trigger(_uartInstance[channel].p_reg, NRF_UARTE_TASK_STARTRX);
    }

    // Receiver timeout
    if (nrf_uarte_event_check(_uartInstance[channel].p_reg, NRF_UARTE_EVENT_RXTO))
    {
        nrf_uarte_event_clear(_uartInstance[channel].p_reg, NRF_UARTE_EVENT_RXTO);
    }

    //  last TX byte transmitted from internal NRF buffer
    if (nrf_uarte_event_check(_uartInstance[channel].p_reg, NRF_UARTE_EVENT_ENDTX))
    {
        nrf_uarte_event_clear(_uartInstance[channel].p_reg, NRF_UARTE_EVENT_ENDTX);
        checkTx(channel);
    }

    // transmitting stopped
    if (nrf_uarte_event_check(_uartInstance[channel].p_reg, NRF_UARTE_EVENT_TXSTOPPED))
    {
        nrf_uarte_event_clear(_uartInstance[channel].p_reg, NRF_UARTE_EVENT_TXSTOPPED);
    }
}

#endif