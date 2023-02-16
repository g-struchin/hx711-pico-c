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
    do { \
        UTIL_ASSERT_NOT_NULL(hxm); \
        UTIL_ASSERT_NOT_NULL(hxm->_pio); \
        assert(pio_sm_is_claimed(hxm->_pio, hxm->_awaiter_sm)); \
        assert(pio_sm_is_claimed(hxm->_pio, hxm->_reader_sm)); \
        assert(dma_channel_is_claimed(hxm->_dma_channel)); \
        assert(mutex_is_initialized(&hxm->_mut)); \
    } while(0)

#define HX711_MULTI_ASSERT_STATE_MACHINES_ENABLED(hxm) \
    do { \
        assert(util_pio_sm_is_enabled(hxm->_pio, hxm->_awaiter_sm)); \
        assert(util_pio_sm_is_enabled(hxm->_pio, hxm->_reader_sm)); \
    } while(0)

#define HX711_MULTI_CONVERSION_DONE_IRQ_NUM     0
#define HX711_MULTI_DATA_READY_IRQ_NUM          4

#define HX711_MULTI_ASYNC_REQ_COUNT             NUM_PIOS
#define HX711_MULTI_ASYNC_PIO_IRQ_IDX           0
#define HX711_MULTI_ASYNC_DMA_IRQ_IDX           0

#define HX711_MULTI_MIN_CHIPS                   1u
#define HX711_MULTI_MAX_CHIPS                   32u

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

typedef enum {
    HX711_MULTI_ASYNC_STATE_NONE = 0,
    HX711_MULTI_ASYNC_STATE_WAITING,
    HX711_MULTI_ASYNC_STATE_READING,
    HX711_MULTI_ASYNC_STATE_DONE
} hx711_multi_async_state_t;

typedef struct {

    /**
     * @brief Pointer to underlying hx711_multi_t.
     */
    hx711_multi_t* _hxm;

    /**
     * @brief Which PIOX_IRQ_N interrupt to use, where N
     * is either 0 or 1. 
     */
    uint pio_irq_index;

    /**
     * @brief Which DMA_IRQ_N interrupt to use, where N
     * is either 0 or 1.
     */
    uint dma_irq_index;

    /**
     * @brief The state of the request as it moves through the
     * read process.
     */
    volatile hx711_multi_async_state_t _state;

    /**
     * @brief Array of pin values read from PIO through DMA.
     */
    uint32_t _buffer[HX711_READ_BITS];

} hx711_multi_async_request_t;

/**
 * @brief Array of requests for ISR to access.
 */
extern hx711_multi_async_request_t* hx711_multi__async_request_map[HX711_MULTI_ASYNC_REQ_COUNT];

/**
 * @brief Convert an array of pinvals to regular HX711
 * values.
 * 
 * @param pinvals 
 * @param values 
 * @param len number of values to convert
 */
void hx711_multi_pinvals_to_values(
    const uint32_t* const pinvals,
    int32_t* const values,
    const size_t len);

/**
 * @brief Reads pinvals into an array.
 * 
 * @param hxm 
 */
static void hx711_multi__get_values_raw(
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
static bool hx711_multi__get_values_timeout_raw(
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
 * @brief Sets the HX711s' gain.
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

/**
 * @brief Whether a given request is the cause of the current
 * DMA IRQ.
 * 
 * @param req 
 * @return true 
 * @return false 
 */
static bool hx711_multi__async_dma_irq_is_set(
    hx711_multi_async_request_t* const req);

/**
 * @brief Whether a given request is the cause of the current
 * PIO IRQ.
 * 
 * @param req 
 * @return true 
 * @return false 
 */
static bool hx711_multi__async_pio_irq_is_set(
    hx711_multi_async_request_t* const req);

/**
 * @brief Get the request which caused the current DMA IRQ. Returns
 * NULL if none found.
 * 
 * @return hx711_multi_async_request_t* const 
 */
static hx711_multi_async_request_t* const hx711_multi__async_get_dma_irq_request();

/**
 * @brief Get the request which caused the current PIO IRQ. Returns
 * NULL if none found.
 * 
 * @return hx711_multi_async_request_t* const 
 */
static hx711_multi_async_request_t* const hx711_multi__async_get_pio_irq_request();

/**
 * @brief Triggers DMA reading; moves request state from WAITING to READING.
 * 
 * @param req 
 */
static void hx711_multi__async_start_dma(
    volatile hx711_multi_async_request_t* volatile const req);

/**
 * @brief ISR handler for PIO IRQs.
 */
static void __isr __not_in_flash_func(hx711_multi__async_pio_irq_handler)();

/**
 * @brief ISR handler for DMA IRQs.
 */
static void __isr __not_in_flash_func(hx711_multi__async_dma_irq_handler)();

/**
 * @brief Adds request to the request map for ISR access. Returns false
 * if no space.
 * 
 * @param req 
 * @return true 
 * @return false 
 */
static bool hx711_multi__async_add_request(
    hx711_multi_async_request_t* const req);

/**
 * @brief Removes the given request from the request.
 * 
 * @param req 
 */
static void hx711_multi__async_remove_request(
    const hx711_multi_async_request_t* const req);

/**
 * @brief Sets the given request to default settings.
 * 
 * @param hxm 
 * @param req 
 */
void hx711_multi_async_get_request_defaults(
    hx711_multi_t* const hxm,
    hx711_multi_async_request_t* const req);

/**
 * @brief Sets up asynchronous request functions. PIO and DMA IRQs
 * are claimed.
 * 
 * @param hxm 
 * @param req 
 */
void hx711_multi_async_open(
    hx711_multi_t* const hxm,
    hx711_multi_async_request_t* const req);

/**
 * @brief Begins an asynchronous request.
 * 
 * @param req 
 */
void hx711_multi_async_start(
    hx711_multi_async_request_t* const req);

/**
 * @brief Returns true if the given request is complete and
 * values can be read from it.
 * 
 * @param req 
 * @return true 
 * @return false 
 */
bool hx711_multi_async_is_done(
    hx711_multi_async_request_t* const req);

/**
 * @brief Obtains HX711 values in chip-order from the internal
 * request.
 * 
 * @param req 
 * @param values 
 */
void hx711_multi_async_get_values(
    hx711_multi_async_request_t* const req,
    int32_t* const values);

/**
 * @brief Closes asychronous functionality. PIO and DMA IRQs are
 * released.
 * 
 * @param hxm 
 * @param req 
 */
void hx711_multi_async_close(
    hx711_multi_t* const hxm,
    hx711_multi_async_request_t* const req);

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
void hx711_multi_sync(
    hx711_multi_t* const hxm,
    const hx711_gain_t gain);

/**
 * @brief Returns the state of each chip as a bitmask. The 0th
 * bit is the first chip, 1th bit is the second, and so on.
 * 
 * @param hxm 
 * @return uint32_t 
 */
uint32_t hx711_multi_sync_state(
    hx711_multi_t* const hxm);

/**
 * @brief Determines whether all chips are in sync.
 * 
 * @param hxm 
 * @return true 
 * @return false 
 */
bool hx711_multi_is_syncd(
    hx711_multi_t* const hxm);

#ifdef __cplusplus
}
#endif

#endif
