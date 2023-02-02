// MIT License
// 
// Copyright (c) 2023 Daniel Robertson
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef HX711_MULTI_H_253BF37A_8356_462B_B8F9_39E09A7193E6
#define HX711_MULTI_H_253BF37A_8356_462B_B8F9_39E09A7193E6

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/mutex.h"
#include "pico/platform.h"
#include "hx711.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HX711_MULTI_ASSERT_INITD(hxm) \
    UTIL_ASSERT_NOT_NULL(hxm) \
    UTIL_ASSERT_NOT_NULL(hxm->_pio) \
    assert(pio_sm_is_claimed(hxm->_pio, hxm->_awaiter_sm)); \
    assert(pio_sm_is_claimed(hxm->_pio, hxm->_reader_sm)); \
    assert(dma_channel_is_claimed(hxm->_dma_channel)); \
    assert(mutex_is_initialized(&hxm->_mut));

#define HX711_MULTI_ASSERT_STATE_MACHINES_ENABLED(hxm) \
    assert(util_pio_sm_is_enabled(hxm->_pio, hxm->_awaiter_sm)); \
    assert(util_pio_sm_is_enabled(hxm->_pio, hxm->_reader_sm));

#define HX711_MULTI_CONVERSION_RUNNING_IRQ_NUM  1
#define HX711_MULTI_DATA_READY_IRQ_NUM          4

#define HX711_MULTI_MIN_CHIPS                   1
#define HX711_MULTI_MAX_CHIPS                   32

typedef struct {

    uint _clock_pin;
    uint _data_pin_base;
    size_t _chips_len;

    PIO _pio;

    const pio_program_t* _awaiter_prog;
    pio_sm_config _awaiter_default_config;
    uint _awaiter_sm;
    uint _awaiter_offset;

    const pio_program_t* _reader_prog;
    pio_sm_config _reader_default_config;
    uint _reader_sm;
    uint _reader_offset;

    int _dma_channel;

    mutex_t _mut;

} hx711_multi_t;

typedef void (*hx711_multi_pio_init_t)(hx711_multi_t* const);
typedef void (*hx711_multi_program_init_t)(hx711_multi_t* const);

typedef struct {

    uint clock_pin;
    uint data_pin_base;
    size_t chips_len;

    PIO pio;
    hx711_multi_pio_init_t pio_init;

    const pio_program_t* awaiter_prog;
    hx711_multi_program_init_t awaiter_prog_init;

    const pio_program_t* reader_prog;
    hx711_multi_program_init_t reader_prog_init;

} hx711_multi_config_t;

typedef struct {
    hx711_multi_t* _hxm;
    uint32_t _buffer[HX711_READ_BITS];
} hx711_multi_async_request_t;

extern hx711_multi_async_request_t* hx711_multi_irq_map[NUM_PIOS];

/**
 * @brief Convert an array of pinvals to regular HX711
 * values.
 * 
 * @param pinvals 
 * @param values 
 * @param len number of values to convert
 */
static void hx711_multi__pinvals_to_values(
    const uint32_t* const pinvals,
    int32_t* const values,
    const size_t len);

/**
 * @brief Reads pinvals into an array.
 * 
 * @param hxm 
 */
void hx711_multi__get_values_raw(
    hx711_multi_t* const hxm,
    uint32_t* const pinvals);

/**
 * @brief Reads pinvals into an array, timing out
 * if not possible within the given period.
 * 
 * @param hxm 
 * @param timeout microseconds
 * @return true 
 * @return false 
 */
bool hx711_multi__get_values_timeout_raw(
    hx711_multi_t* const hxm,
    uint32_t* const pinvals,
    const absolute_time_t* const end);

void hx711_multi_init(
    hx711_multi_t* const hxm,
    const hx711_multi_config_t* const config);

/**
 * @brief Stop communication with all HX711s.
 * 
 * @param hxm 
 */
void hx711_multi_close(hx711_multi_t* const hxm);

/**
 * @brief Sets the HX711s gain.
 * 
 * @param hxm 
 * @param gain 
 */
void hx711_multi_set_gain(
    hx711_multi_t* const hxm,
    const hx711_gain_t gain);

/**
 * @brief Fill an array with one value from each HX711. Blocks
 * until values are obtained.
 * 
 * @param hxm 
 * @param values 
 */
void hx711_multi_get_values(
    hx711_multi_t* const hxm,
    int32_t* const values);

/**
 * @brief Fill an array with one value from each HX711,
 * timing out if failing to obtain values within the
 * timeout period.
 * 
 * @param hxm 
 * @param values 
 * @param timeout microseconds
 * @return true if values obtained within the timeout period
 * @return false if values not obtained within the timeout period
 */
bool hx711_multi_get_values_timeout(
    hx711_multi_t* const hxm,
    int32_t* const values,
    const uint timeout);

void hx711_multi_async_open(
    hx711_multi_t* const hxm,
    hx711_multi_async_request_t* const req);

bool hx711_multi_async_is_done(
    const hx711_multi_async_request_t* const req);

void hx711_multi_async_get(
    hx711_multi_async_request_t* const req,
    int32_t* const values);

void hx711_multi_async_close(
    hx711_multi_async_request_t* const req);

static void hx711_multi__irq_handler() {

    hx711_multi_async_request_t* req = NULL;

    //either going to be pio0 or pio1

    for(uint i = 0; i < NUM_PIOS; ++i) {

        if(hx711_multi_irq_map[i] != NULL) {
            continue;
        }

        if(pio_interrupt_get(
            hx711_multi_irq_map[i]->_hxm->_pio,
            HX711_MULTI_CONVERSION_RUNNING_IRQ_NUM)) {
                req = hx711_multi_irq_map[i];
                hx711_multi_irq_map[i] = NULL;
                break;
        }

    }

    UTIL_ASSERT_NOT_NULL(req);

    const uint pioIrq = util_pion_get_irqn(
        req->_hxm->_pio,
        0);

    irq_set_enabled(
        pioIrq,
        false);

    pio_set_irqn_source_enabled(
        req->_hxm->_pio,
        pioIrq,
        util_pio_get_pis(HX711_MULTI_CONVERSION_RUNNING_IRQ_NUM),
        false);

    irq_remove_handler(
        pioIrq,
        hx711_multi__irq_handler);

    irq_clear(pioIrq);

    //clear any residual data
    util_pio_sm_clear_rx_fifo(
        req->_hxm->_pio,
        req->_hxm->_reader_sm);

    //then start reading
    dma_channel_set_write_addr(
        req->_hxm->_dma_channel,
        req->_buffer,
        true);

}

/**
 * @brief Power up each HX711 and start the internal read/write
 * functionality.
 * 
 * @related hx711_wait_settle
 * @param hxm 
 * @param gain hx711_gain_t initial gain
 */
void hx711_multi_power_up(
    hx711_multi_t* const hxm,
    const hx711_gain_t gain);

/**
 * @brief Power down each HX711 and stop the internal read/write
 * functionlity.
 * 
 * @related hx711_wait_power_down()
 * @param hxm 
 */
void hx711_multi_power_down(hx711_multi_t* const hxm);

/**
 * @brief Attempt to synchronise all connected chips. This
 * does not include a settling time.
 * 
 * @param hxm 
 * @param gain initial gain to set to all chips
 */
inline void hx711_multi_sync(
    hx711_multi_t* const hxm,
    const hx711_gain_t gain) {
        HX711_MULTI_ASSERT_INITD(hxm)
        hx711_multi_power_down(hxm);
        hx711_wait_power_down();
        hx711_multi_power_up(hxm, gain);
}

#ifdef __cplusplus
}
#endif

#endif
