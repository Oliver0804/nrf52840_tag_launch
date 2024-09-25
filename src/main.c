/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/pm/pm.h>			// 用於功耗管理
#include <zephyr/drivers/counter.h> // 用於定時器

#include <nfc_t2t_lib.h>
#include <nfc/ndef/launchapp_msg.h>

#include <dk_buttons_and_leds.h>

#define NDEF_MSG_BUF_SIZE 256
#define NFC_FIELD_LED DK_LED1
#define WAKEUP_INTERVAL_MS 5000 // 休眠定時器，5秒喚醒一次

/* 更新為 Instagram 的包名和 URL */
static const uint8_t android_pkg_name[] = {
	'c', 'o', 'm', '.', 'i', 'n', 's', 't', 'a', 'g', 'r', 'a', 'm', '.', 'a', 'n', 'd', 'r',
	'o', 'i', 'd'}; // Instagram Android 包名

/* URI https://www.instagram.com/code.homemade/ */
static const uint8_t universal_link[] = {
	'h', 't', 't', 'p', 's', ':', '/', '/', 'w', 'w', 'w', '.', 'i', 'n', 's', 't', 'a', 'g',
	'r', 'a', 'm', '.', 'c', 'o', 'm', '/', 'c', 'o', 'd', 'e', '.', 'h', 'o', 'm', 'e', 'm',
	'a', 'd', 'e', '/'};

/* Buffer used to hold an NFC NDEF message. */
static uint8_t ndef_msg_buf[NDEF_MSG_BUF_SIZE];

static void nfc_callback(void *context,
						 nfc_t2t_event_t event,
						 const uint8_t *data,
						 size_t data_length)
{
	ARG_UNUSED(context);
	ARG_UNUSED(data);
	ARG_UNUSED(data_length);

	switch (event)
	{
	case NFC_T2T_EVENT_FIELD_ON:
		dk_set_led_on(NFC_FIELD_LED);
		break;
	case NFC_T2T_EVENT_FIELD_OFF:
		dk_set_led_off(NFC_FIELD_LED);
		break;
	default:
		break;
	}
}

static void timer_callback(struct k_timer *dummy)
{
	printk("定時喚醒!\n");
}

K_TIMER_DEFINE(my_timer, timer_callback, NULL);

int main(void)
{
	int err;
	size_t len = sizeof(ndef_msg_buf);

	printk("Starting NFC Launch app example with sleep\n");

	/* Configure LED-pins as outputs */
	err = dk_leds_init();
	if (err)
	{
		printk("Cannot init LEDs!\n");
		goto fail;
	}

	/* Set up NFC */
	err = nfc_t2t_setup(nfc_callback, NULL);
	if (err)
	{
		printk("Cannot setup NFC T2T library!\n");
		goto fail;
	}

	/* Encode launch app data */
	err = nfc_launchapp_msg_encode(android_pkg_name,
								   sizeof(android_pkg_name),
								   universal_link,
								   sizeof(universal_link),
								   ndef_msg_buf,
								   &len);
	if (err)
	{
		printk("Cannot encode message!\n");
		goto fail;
	}

	/* Set created message as the NFC payload */
	err = nfc_t2t_payload_set(ndef_msg_buf, len);
	if (err)
	{
		printk("Cannot set payload!\n");
		goto fail;
	}

	/* Start sensing NFC field */
	err = nfc_t2t_emulation_start();
	if (err)
	{
		printk("Cannot start emulation!\n");
		goto fail;
	}

	printk("NFC configuration done\n");

	/* 開始定時器，每5秒喚醒一次 */
	k_timer_start(&my_timer, K_MSEC(WAKEUP_INTERVAL_MS), K_MSEC(WAKEUP_INTERVAL_MS));

	while (1)
	{
		/* 讓系統進入低功耗模式 */
		printk("系統進入休眠模式...\n");
		k_sleep(K_SECONDS(5)); // 休眠5秒
		printk("系統喚醒!\n");
	}

	return 0;

fail:
#if CONFIG_REBOOT
	sys_reboot(SYS_REBOOT_COLD);
#endif /* CONFIG_REBOOT */

	return err;
}
