/*
	SlimeVR Code is placed under the MIT license
	Copyright (c) 2026 SlimeVR Contributors
*/
#include "afh_wrapper.h"

#include <errno.h>

#include <esb.h>
#include <zephyr/logging/log.h>

#include "afh.h"
#include "globals.h"

LOG_MODULE_REGISTER(afh_wrapper, LOG_LEVEL_INF);

static bool initialized;

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
	int err;

	afh_wrapper_init();
	if (!afh_parse_sync_packet(data, length, &tracker_id, &channel, &epoch))
		return false;
	if (tracker_id >= stored_trackers) {
		LOG_WRN("AFH sync for unknown tracker id %u", tracker_id);
		return true;
	}

	err = afh_wrapper_apply_channel(channel);
	if (err) {
		LOG_ERR("AFH sync channel %u from tracker %u failed: %d", channel, tracker_id, err);
		return true;
	}

	afh_set_epoch(epoch);
	LOG_INF("AFH sync applied: tracker %u channel %u epoch %u", tracker_id, channel, epoch);
	return true;
}
