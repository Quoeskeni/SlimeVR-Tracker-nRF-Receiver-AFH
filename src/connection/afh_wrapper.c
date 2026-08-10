/*
	SlimeVR Code is placed under the MIT license
	Copyright (c) 2026 SlimeVR Contributors
*/
#include "afh_wrapper.h"

#include <errno.h>
#include <stddef.h>

#include <esb.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include "afh.h"
#include "globals.h"

LOG_MODULE_REGISTER(afh_wrapper, LOG_LEVEL_INF);

#define AFH_TX_FAILURE_SWITCH_THRESHOLD 4U
#define AFH_RX_REEVALUATE_INTERVAL 32U

static bool initialized;
static atomic_t pending_channel_valid;
static uint8_t pending_channel;
static uint8_t pending_epoch;
static uint8_t consecutive_tx_failures;
static uint8_t rx_packets_since_reevaluate;

void afh_wrapper_init(void)
{
	if (initialized)
		return;
	afh_init();
	initialized = true;
}

int afh_wrapper_apply_channel(uint8_t channel)
{
	int err;

	afh_wrapper_init();
	if (!afh_is_channel_valid(channel)) {
		LOG_WRN("AFH rejected invalid RF channel %u", channel);
		return -EINVAL;
	}

	err = esb_set_rf_channel((uint32_t)channel);
	if (err) {
		LOG_ERR("AFH failed to apply RF channel %u: %d", channel, err);
		return err;
	}

	afh_set_channel(channel);
	LOG_INF("AFH RF channel set to %u", channel);
	return 0;
}

int afh_wrapper_apply_current_channel(void)
{
	afh_wrapper_init();
	return afh_wrapper_apply_channel(afh_get_channel());
}

int afh_wrapper_apply_default_channel(void)
{
	return afh_wrapper_apply_channel(AFH_DEFAULT_CHANNEL);
}

int afh_wrapper_set_channel_state(uint8_t channel, uint8_t epoch)
{
	afh_wrapper_init();
	if (!afh_is_channel_valid(channel))
		return -EINVAL;

	afh_set_channel(channel);
	afh_set_epoch(epoch);
	return 0;
}

uint8_t afh_wrapper_get_channel(void)
{
	afh_wrapper_init();
	return afh_get_channel();
}

uint8_t afh_wrapper_get_epoch(void)
{
	afh_wrapper_init();
	return afh_get_epoch();
}

void afh_wrapper_record_tx_success(void)
{
	afh_wrapper_init();
	consecutive_tx_failures = 0;
	afh_record_tx_success(afh_get_channel());
}

void afh_wrapper_record_tx_failure(void)
{
	afh_wrapper_init();
	if (consecutive_tx_failures < UINT8_MAX)
		consecutive_tx_failures++;
	afh_record_tx_failure(afh_get_channel());
	if (consecutive_tx_failures >= AFH_TX_FAILURE_SWITCH_THRESHOLD)
		afh_wrapper_queue_best_channel_if_needed();
}

void afh_wrapper_record_rx_packet(int8_t rssi)
{
	afh_wrapper_init();
	afh_record_rx_packet(afh_get_channel(), rssi);
	if (++rx_packets_since_reevaluate >= AFH_RX_REEVALUATE_INTERVAL) {
		rx_packets_since_reevaluate = 0;
		afh_wrapper_queue_best_channel_if_needed();
	}
}

bool afh_wrapper_queue_best_channel_if_needed(void)
{
	uint8_t best_channel;

	afh_wrapper_init();
	best_channel = afh_select_best_channel();
	if (best_channel == afh_get_channel())
		return false;

	pending_channel = best_channel;
	pending_epoch = afh_next_epoch();
	atomic_set(&pending_channel_valid, 1);
	consecutive_tx_failures = 0;
	LOG_INF("AFH selected better RF channel %u epoch %u", pending_channel, pending_epoch);
	return true;
}

bool afh_wrapper_handle_sync_packet(const uint8_t *data, uint8_t length,
				    uint8_t *tracker_id, uint8_t *channel,
				    uint8_t *epoch, bool *queued)
{
	uint8_t parsed_tracker_id;
	uint8_t parsed_channel;
	uint8_t parsed_epoch;

	afh_wrapper_init();
	if (queued != NULL)
		*queued = false;
	if (!afh_parse_sync_packet(data, length, &parsed_tracker_id, &parsed_channel, &parsed_epoch))
		return false;

	if (tracker_id != NULL)
		*tracker_id = parsed_tracker_id;
	if (channel != NULL)
		*channel = parsed_channel;
	if (epoch != NULL)
		*epoch = parsed_epoch;

	if (parsed_tracker_id >= stored_trackers) {
		LOG_WRN("AFH sync for unknown tracker id %u", parsed_tracker_id);
		return true;
	}

	pending_channel = parsed_channel;
	pending_epoch = parsed_epoch;
	atomic_set(&pending_channel_valid, 1);
	if (queued != NULL)
		*queued = true;
	LOG_INF("AFH sync queued: tracker %u channel %u epoch %u", parsed_tracker_id, parsed_channel, parsed_epoch);
	return true;
}

/*
 * ESB invokes RX callbacks from radio/event context. Do not change RF channel
 * there: the receiver thread drains this pending request, disables ESB, applies
 * the channel during ESB initialization, then restarts RX.
 */
bool afh_wrapper_take_pending_channel(uint8_t *channel, uint8_t *epoch)
{
	afh_wrapper_init();
	if (channel == NULL || epoch == NULL)
		return false;
	if (!atomic_cas(&pending_channel_valid, 1, 0))
		return false;

	*channel = pending_channel;
	*epoch = pending_epoch;
	return true;
}
