#ifndef _wlan_bmxwlanstatus_h
#define _wlan_bmxwlanstatus_h

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bmx_wlan_flow_status
{
	unsigned tx_sequence;
	unsigned tx_window;
	unsigned flow_control_mask;
	unsigned tx_queue_frames;
	unsigned long long tx_frames;
	unsigned long long rx_data_frames;
	unsigned long long tx_window_updates;
	unsigned long long tx_flow_updates;
	unsigned long long tx_window_stalls;
	unsigned long long tx_window_stall_ms;
	unsigned long long tx_window_stall_max_ms;
	unsigned long long tx_window_stall_current_ms;
	unsigned long long tx_flow_stalls;
	unsigned long long tx_flow_stall_ms;
	unsigned long long tx_flow_stall_max_ms;
	unsigned long long tx_flow_stall_current_ms;
	unsigned long long tx_timing_samples;
	unsigned long long tx_queue_us;
	unsigned long long tx_queue_max_us;
	unsigned long long tx_pktlock_wait_us;
	unsigned long long tx_pktlock_wait_max_us;
	unsigned long long tx_sdio_us;
	unsigned long long tx_sdio_max_us;
	unsigned long long tx_pktlock_yield_calls;
	unsigned long long tx_pktlock_yield_us;
	unsigned long long tx_pktlock_yield_max_us;
	unsigned long long rx_timing_samples;
	unsigned long long rx_pktlock_wait_us;
	unsigned long long rx_pktlock_wait_max_us;
	unsigned long long rx_sdio_us;
	unsigned long long rx_sdio_max_us;
	unsigned long long rx_pktlock_yield_calls;
	unsigned long long rx_pktlock_yield_us;
	unsigned long long rx_pktlock_yield_max_us;
	unsigned long long rx_to_netdev_samples;
	unsigned long long rx_to_netdev_us;
	unsigned long long rx_to_netdev_max_us;
	unsigned long long emmc_dataready_precheck_hits;
	unsigned long long emmc_dataready_poll_hits;
	unsigned long long emmc_dataready_sleep_calls;
	unsigned long long emmc_dataready_poll_us;
	unsigned long long emmc_dataready_poll_max_us;
	unsigned long long emmc_datadone_precheck_hits;
	unsigned long long emmc_datadone_poll_hits;
	unsigned long long emmc_datadone_sleep_calls;
	unsigned long long emmc_datadone_poll_us;
	unsigned long long emmc_datadone_poll_max_us;
} bmx_wlan_flow_status_t;

void bmx_emmc_wait_status(bmx_wlan_flow_status_t *status);

#ifdef __cplusplus
}
#endif

#endif
