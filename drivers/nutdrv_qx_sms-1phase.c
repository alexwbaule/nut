/* nutdrv_qx_sms-1phase.c - Subdriver for SMS Brazil UPSes with MONO protocol
 *
 * Copyright (C)
 *   2023 Alex W Baule <alexwbaule@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "main.h"
#include "nutdrv_qx.h"
#include <stdbool.h> /* bool type */
#include "nut_stdint.h"
#include "nutdrv_qx_sms-1phase.h"


#define sms_1phase_VERSION "sms-1phase 0.01"
#define ENDCHAR '\r'



uint8_t sms_prepare_cancel_shutdown(uint8_t *buffer) {
    buffer[0] = 'C';
    buffer[1] = -1;
    buffer[2] = -1;
    buffer[3] = -1;
    buffer[4] = -1;
    buffer[5] = (buffer[0] + buffer[1] + buffer[2] + buffer[3] + buffer[4]) * -1;
    buffer[6] = ENDCHAR;

    return 7;
}

/* == qx2nut lookup table == */
static item_t	sms_1phase_qx2nut[] = {

	/* Query UPS for protocol
	 * > [M\r]
	 * < [P\r]
	 *    01
	 *    0
	 */

	{ "ups.firmware.aux",		0,	NULL,	"M\r",	"",	2,	0,	"",	0,	0,	"PM-%s",	QX_FLAG_STATIC,	NULL,	NULL,	voltronic_qs_hex_protocol },

	/* Query UPS for status
	 * > [QS\r]
	 * < [#6C01 35 6C01 35 03 519A 1312D0 E6 1E 00001001\r]			('P' protocol, after being preprocessed)
	 * < [#6901 6C 6802 6C 00 5FD7 12C000 E4 1E 00001001 00000010\r]	('T' protocol, after being preprocessed)
	 *    01234567890123456789012345678901234567890123456789012345
	 *    0         1         2         3         4         5
	 */

	{ "input.voltage",		0,	NULL,	"QS\r",	"",	47,	'#',	"",	1,	7,	"%.1f",	0,	NULL,	sms_1phase_preprocess_qs_answer,	voltronic_qs_hex_input_output_voltage },
	{ "output.voltage",		0,	NULL,	"QS\r",	"",	47,	'#',	"",	9,	15,	"%.1f",	0,	NULL,	sms_1phase_preprocess_qs_answer,	voltronic_qs_hex_input_output_voltage },
	{ "ups.load",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	17,	18,	"%d",	0,	NULL,	sms_1phase_preprocess_qs_answer,	voltronic_qs_hex_load },
	{ "output.frequency",		0,	NULL,	"QS\r",	"",	47,	'#',	"",	20,	30,	"%.1f",	0,	NULL,	sms_1phase_preprocess_qs_answer,	voltronic_qs_hex_frequency },
	{ "battery.voltage",		0,	NULL,	"QS\r",	"",	47,	'#',	"",	32,	36,	"%.2f",	0,	NULL,	sms_1phase_preprocess_qs_answer,	voltronic_qs_hex_battery_voltage },
	/* Status bits */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	38,	38,	NULL,	QX_FLAG_STATIC,	NULL,	sms_1phase_preprocess_qs_answer,	blazer_process_status_bits },	/* Utility Fail (Immediate) */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	39,	39,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	sms_1phase_preprocess_qs_answer,	blazer_process_status_bits },	/* Battery Low */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	40,	40,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	sms_1phase_preprocess_qs_answer,	blazer_process_status_bits },	/* Bypass/Boost or Buck Active */
	{ "ups.alarm",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	41,	41,	NULL,	0,			NULL,	sms_1phase_preprocess_qs_answer,	blazer_process_status_bits },	/* UPS Failed */
	{ "ups.type",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	42,	42,	"%s",	QX_FLAG_STATIC,		NULL,	sms_1phase_preprocess_qs_answer,	blazer_process_status_bits },	/* UPS Type */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	43,	43,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	sms_1phase_preprocess_qs_answer,	blazer_process_status_bits },	/* Test in Progress */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	44,	44,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	sms_1phase_preprocess_qs_answer,	blazer_process_status_bits },	/* Shutdown Active */
	{ "ups.beeper.status",		0,	NULL,	"QS\r",	"",	47,	'#',	"",	45,	45,	"%s",	0,			NULL,	sms_1phase_preprocess_qs_answer,	blazer_process_status_bits },	/* Beeper status */
	/* Ratings bits */
	{ "output.frequency.nominal",	0,	NULL,	"QS\r",	"",	56,	'#',	"",	47,	47,	"%.1f",	QX_FLAG_SKIP,		NULL,	sms_1phase_preprocess_qs_answer,	voltronic_qs_hex_process_ratings_bits },
	{ "battery.voltage.nominal",	0,	NULL,	"QS\r",	"",	56,	'#',	"",	48,	49,	"%.1f",	QX_FLAG_SKIP,		NULL,	sms_1phase_preprocess_qs_answer,	voltronic_qs_hex_process_ratings_bits },
/*	{ "reserved.1",			0,	NULL,	"QS\r",	"",	56,	'#',	"",	50,	50,	"%s",	QX_FLAG_SKIP,		NULL,	sms_1phase_preprocess_qs_answer,	voltronic_qs_hex_process_ratings_bits },	*//* Reserved */
/*	{ "reserved.2",			0,	NULL,	"QS\r",	"",	56,	'#',	"",	51,	51,	"%s",	QX_FLAG_SKIP,		NULL,	sms_1phase_preprocess_qs_answer,	voltronic_qs_hex_process_ratings_bits },	*//* Reserved */
	{ "output.voltage.nominal",	0,	NULL,	"QS\r",	"",	56,	'#',	"",	52,	54,	"%.1f",	QX_FLAG_SKIP,		NULL,	sms_1phase_preprocess_qs_answer,	voltronic_qs_hex_process_ratings_bits },

	/* Instant commands */
	{ "beeper.toggle",		0,	NULL,	"Q\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.off",			0,	NULL,	"S00R0000\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.on",			0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "shutdown.return",		0,	NULL,	"S%s\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stayoff",		0,	NULL,	"S%sR0000\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stop",		0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.start.quick",	0,	NULL,	"T\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD | QX_FLAG_SKIP,	NULL,	NULL,	NULL },

	/* Server-side settable vars */
	{ "ups.delay.start",		ST_FLAG_RW,	voltronic_qs_hex_r_ondelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_ONDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },
	{ "ups.delay.shutdown",		ST_FLAG_RW,	voltronic_qs_hex_r_offdelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_OFFDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },

	/* End of structure. */
	{ NULL,				0,	NULL,	NULL,		"",	0,	0,	"",	0,	0,	NULL,	0,	NULL,	NULL,	NULL }
};

/* This function allows the subdriver to "claim" a device: return 1 if the device is supported by this subdriver, else 0. */
static int	sms_1phase_claim(void)
{
	/* We need at least M and QS to run this subdriver */

	/* UPS Protocol */
	item_t	*item = find_nut_info("ups.firmware.aux", 0, 0);

	/* Don't know what happened */
	if (!item)
		return 0;

	/* No reply/Unable to get value */
	if (qx_process(item, NULL))
		return 0;

	/* Unable to process value/Protocol not supported */
	if (ups_infoval_set(item) != 1)
		return 0;

	item = find_nut_info("input.voltage", 0, 0);

	/* Don't know what happened */
	if (!item) {
		dstate_delinfo("ups.firmware.aux");
		return 0;
	}

	/* No reply/Unable to get value */
	if (qx_process(item, NULL)) {
		dstate_delinfo("ups.firmware.aux");
		return 0;
	}

	/* Unable to process value */
	if (ups_infoval_set(item) != 1) {
		dstate_delinfo("ups.firmware.aux");
		return 0;
	}

	return 1;
}

/* Subdriver-specific initups */
static void	sms_1phase_initups(void)
{
	blazer_initups_light(sms_1phase_qx2nut);
}

/* Preprocess the answer we got back from the UPS when queried with 'QS\r' */
static int	sms_1phase_preprocess_qs_answer(item_t *item, const int len)
{

}


/* == Subdriver interface == */
subdriver_t	sms_1phase_subdriver = {
	sms_1phase_VERSION,
	sms_1phase_claim,
	sms_1phase_qx2nut,
	sms_1phase_initups,
	NULL,
	blazer_makevartable_light,
	NULL,
	"N\r",
#ifdef TESTING
	sms_1phase_testing,
#endif	/* TESTING */
};
