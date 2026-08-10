/*
	SlimeVR Code is placed under the MIT license
	Copyright (c) 2026 SlimeVR Contributors
*/
#include "afh_wrapper.h"

#include <errno.h>

#include <esb.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>

#include "afh.h"
#include "globals.h"

LOG_MODULE_REGISTER(afh_wrapper, LOG_LEVEL_INF);

static bool initialized;
static atomic_t pending_channel = ATOMIC_INIT(-1);
static atomic_t pending_epoch = ATOMIC_INIT(0);

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
	afh_record_tx_success(afh_get_channel());
}

void afh_wrapper_record_tx_failure(void)
{
	afh_wrapper_init();
	afh_record_tx_failure(afh_get_channel());
}

void afh_wrapper_record_rx_packet(int8_t rssi)
{
	afh_wrapper_init();
	afh_record_rx_packet(afh_get_channel(), rssi);
}

bool afh_wrapper_handle_sync_packet(const uint8_t *data, uint8_t length)
{
	uint8_t tracker_id;
	uint8_t channel;
	uint8_t epoch;

	afh_wrapper_init();
	if (!afh_parse_sync_packet(data, length, &tracker_id, &channel, &epoch))
		return false;
	if (tracker_id >= stored_trackers) {
		LOG_WRN("AFH sync for unknown tracker id %u", tracker_id);
		return true;
	}

	atomic_set(&pending_epoch, epoch);
	atomic_set(&pending_channel, channel);
	LOG_INF("AFH sync queued: tracker %u channel %u epoch %u", tracker_id, channel, epoch);
	return true;
}

bool afh_wrapper_take_pending_channel(uint8_t *channel)
{
	atomic_val_t pending;

	afh_wrapper_init();
	if (channel == NULL)
		return false;

	pending = atomic_set(&pending_channel, -1);
	if (pending < AFH_MIN_CHANNEL || pending > AFH_MAX_CHANNEL)
		return false;

	*channel = (uint8_t)pending;
	afh_set_epoch((uint8_t)atomic_get(&pending_epoch));
	return true;
}
