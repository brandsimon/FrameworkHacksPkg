#pragma once

int ECSendCommandLPCv3(int command,
                       int version,
                       const void* outdata,
                       int outsize,
                       void* indata,
                       int insize);

#define EECRESULT 1000

/////////////// ec_commands.h from CrosEC

/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Host communication command constants for Chrome EC */

#if !defined(__ACPI__) && !defined(__KERNEL__) && !defined(EFI_SUCCESS)
#include <stdint.h>
#elif defined(EFI_SUCCESS)
typedef UINT8  uint8_t;
typedef UINT16 uint16_t;
typedef UINT32 uint32_t;
typedef UINT64 uint64_t;
typedef INT8   int8_t;
typedef INT16  int16_t;
typedef INT32  int32_t;
typedef INT64  int64_t;
#define UINT16_MAX MAX_UINT16
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CHROMIUM_EC
/*
 * CHROMIUM_EC is defined by the Makefile system of Chromium EC repository.
 * It is used to not include macros that may cause conflicts in foreign
 * projects (refer to crbug.com/984623).
 */

/*
 * Include common.h for CONFIG_HOSTCMD_ALIGNED, if it's defined. This
 * generates more efficient code for accessing request/response structures on
 * ARM Cortex-M if the structures are guaranteed 32-bit aligned.
 */
#include "common.h"
#include "compile_time_macros.h"

#else
#define BUILD_ASSERT(_cond)
#endif  /* CHROMIUM_EC */

#ifdef __KERNEL__
#include <linux/limits.h>
#else
/*
 * Defines macros that may be needed but are for sure defined by the linux
 * kernel. This section is removed when cros_ec_commands.h is generated (by
 * util/make_linux_ec_commands_h.sh).
 * cros_ec_commands.h looks more integrated to the kernel.
 */

#ifndef BIT
#define BIT(nr)         (1UL << (nr))
#endif

#ifndef BIT_ULL
#define BIT_ULL(nr)     (1ULL << (nr))
#endif

#endif  /* __KERNEL__ */

/*
 * Current version of this protocol
 *
 * TODO(crosbug.com/p/11223): This is effectively useless; protocol is
 * determined in other ways.  Remove this once the kernel code no longer
 * depends on it.
 */
#define EC_PROTO_VERSION          0x00000002

/* I/O addresses for host command */
#define EC_LPC_ADDR_HOST_DATA  0x200
#define EC_LPC_ADDR_HOST_CMD   0x204

/* Protocol version 3 */
#define EC_LPC_ADDR_HOST_PACKET  0x800  /* Offset of version 3 packet */
#define EC_LPC_HOST_PACKET_SIZE  0x100  /* Max size of version 3 packet */

/*
 * This header file is used in coreboot both in C and ACPI code.  The ACPI code
 * is pre-processed to handle constants but the ASL compiler is unable to
 * handle actual C code so keep it separate.
 */
#ifndef __ACPI__

#ifndef __KERNEL__
/*
 * Define __packed if someone hasn't beat us to it.  Linux kernel style
 * checking prefers __packed over __attribute__((packed)).
 */
#ifndef __packed
#define __packed __attribute__((packed))
#endif

#ifndef __aligned
#define __aligned(x) __attribute__((aligned(x)))
#endif
#endif  /* __KERNEL__ */

/*
 * Attributes for EC request and response packets.  Just defining __packed
 * results in inefficient assembly code on ARM, if the structure is actually
 * 32-bit aligned, as it should be for all buffers.
 *
 * Be very careful when adding these to existing structures.  They will round
 * up the structure size to the specified boundary.
 *
 * Also be very careful to make that if a structure is included in some other
 * parent structure that the alignment will still be true given the packing of
 * the parent structure.  This is particularly important if the sub-structure
 * will be passed as a pointer to another function, since that function will
 * not know about the misalignment caused by the parent structure's packing.
 *
 * Also be very careful using __packed - particularly when nesting non-packed
 * structures inside packed ones.  In fact, DO NOT use __packed directly;
 * always use one of these attributes.
 *
 * Once everything is annotated properly, the following search strings should
 * not return ANY matches in this file other than right here:
 *
 * "__packed" - generates inefficient code; all sub-structs must also be packed
 *
 * "struct [^_]" - all structs should be annotated, except for structs that are
 * members of other structs/unions (and their original declarations should be
 * annotated).
 */
#ifdef CONFIG_HOSTCMD_ALIGNED

/*
 * Packed structures where offset and size are always aligned to 1, 2, or 4
 * byte boundary.
 */
#define __ec_align1 __packed
#define __ec_align2 __packed __aligned(2)
#define __ec_align4 __packed __aligned(4)

/*
 * Packed structure which must be under-aligned, because its size is not a
 * 4-byte multiple.  This is sub-optimal because it forces byte-wise access
 * of all multi-byte fields in it, even though they are themselves aligned.
 *
 * In theory, we could duplicate the structure with __aligned(4) for accessing
 * its members, but use the __packed version for sizeof().
 */
#define __ec_align_size1 __packed

/*
 * Packed structure which must be under-aligned, because its offset inside a
 * parent structure is not a 4-byte multiple.
 */
#define __ec_align_offset1 __packed
#define __ec_align_offset2 __packed __aligned(2)

/*
 * Structures which are complicated enough that I'm skipping them on the first
 * pass.  They are effectively unchanged from their previous definitions.
 *
 * TODO(rspangler): Figure out what to do with these.  It's likely necessary
 * to work out the size and offset of each member and add explicit padding to
 * maintain those.
 */
#define __ec_todo_packed __packed
#define __ec_todo_unpacked

#else  /* !CONFIG_HOSTCMD_ALIGNED */

/*
 * Packed structures make no assumption about alignment, so they do inefficient
 * byte-wise reads.
 */
#define __ec_align1 __packed
#define __ec_align2 __packed
#define __ec_align4 __packed
#define __ec_align_size1 __packed
#define __ec_align_offset1 __packed
#define __ec_align_offset2 __packed
#define __ec_todo_packed __packed
#define __ec_todo_unpacked

#endif  /* !CONFIG_HOSTCMD_ALIGNED */

/* LPC command status byte masks */
/* EC has written a byte in the data register and host hasn't read it yet */
#define EC_LPC_STATUS_TO_HOST     0x01
/* Host has written a command/data byte and the EC hasn't read it yet */
#define EC_LPC_STATUS_FROM_HOST   0x02
/* EC is processing a command */
#define EC_LPC_STATUS_PROCESSING  0x04
/* Last write to EC was a command, not data */
#define EC_LPC_STATUS_LAST_CMD    0x08
/* EC is in burst mode */
#define EC_LPC_STATUS_BURST_MODE  0x10
/* SCI event is pending (requesting SCI query) */
#define EC_LPC_STATUS_SCI_PENDING 0x20
/* SMI event is pending (requesting SMI query) */
#define EC_LPC_STATUS_SMI_PENDING 0x40
/* (reserved) */
#define EC_LPC_STATUS_RESERVED    0x80

/*
 * EC is busy.  This covers both the EC processing a command, and the host has
 * written a new command but the EC hasn't picked it up yet.
 */
#define EC_LPC_STATUS_BUSY_MASK \
	(EC_LPC_STATUS_FROM_HOST | EC_LPC_STATUS_PROCESSING)

/*
 * Host command response codes (16-bit).
 */
enum ec_status {
	EC_RES_SUCCESS = 0,
	EC_RES_INVALID_COMMAND = 1,
	EC_RES_ERROR = 2,
	EC_RES_INVALID_PARAM = 3,
	EC_RES_ACCESS_DENIED = 4,
	EC_RES_INVALID_RESPONSE = 5,
	EC_RES_INVALID_VERSION = 6,
	EC_RES_INVALID_CHECKSUM = 7,
	EC_RES_IN_PROGRESS = 8,		/* Accepted, command in progress */
	EC_RES_UNAVAILABLE = 9,		/* No response available */
	EC_RES_TIMEOUT = 10,		/* We got a timeout */
	EC_RES_OVERFLOW = 11,		/* Table / data overflow */
	EC_RES_INVALID_HEADER = 12,     /* Header contains invalid data */
	EC_RES_REQUEST_TRUNCATED = 13,  /* Didn't get the entire request */
	EC_RES_RESPONSE_TOO_BIG = 14,   /* Response was too big to handle */
	EC_RES_BUS_ERROR = 15,		/* Communications bus error */
	EC_RES_BUSY = 16,		/* Up but too busy.  Should retry */
	EC_RES_INVALID_HEADER_VERSION = 17,  /* Header version invalid */
	EC_RES_INVALID_HEADER_CRC = 18,      /* Header CRC invalid */
	EC_RES_INVALID_DATA_CRC = 19,        /* Data CRC invalid */
	EC_RES_DUP_UNAVAILABLE = 20,         /* Can't resend response */

	EC_RES_MAX = UINT16_MAX		/**< Force enum to be 16 bits */
} __packed;
BUILD_ASSERT(sizeof(enum ec_status) == sizeof(uint16_t));

/* Flags for ec_lpc_host_args.flags */
/*
 * Args are from host.  Data area at EC_LPC_ADDR_HOST_PARAM contains command
 * params.
 *
 * If EC gets a command and this flag is not set, this is an old-style command.
 * Command version is 0 and params from host are at EC_LPC_ADDR_OLD_PARAM with
 * unknown length.  EC must respond with an old-style response (that is,
 * without setting EC_HOST_ARGS_FLAG_TO_HOST).
 */
#define EC_HOST_ARGS_FLAG_FROM_HOST 0x01
/*
 * Args are from EC.  Data area at EC_LPC_ADDR_HOST_PARAM contains response.
 *
 * If EC responds to a command and this flag is not set, this is an old-style
 * response.  Command version is 0 and response data from EC is at
 * EC_LPC_ADDR_OLD_PARAM with unknown length.
 */
#define EC_HOST_ARGS_FLAG_TO_HOST   0x02

/*****************************************************************************/

/*
 * Value written to legacy command port / prefix byte to indicate protocol
 * 3+ structs are being used.  Usage is bus-dependent.
 */
#define EC_COMMAND_PROTOCOL_3 0xda

#define EC_HOST_REQUEST_VERSION 3

/**
 * struct ec_host_request - Version 3 request from host.
 * @struct_version: Should be 3. The EC will return EC_RES_INVALID_HEADER if it
 *                  receives a header with a version it doesn't know how to
 *                  parse.
 * @checksum: Checksum of request and data; sum of all bytes including checksum
 *            should total to 0.
 * @command: Command to send (EC_CMD_...)
 * @command_version: Command version.
 * @reserved: Unused byte in current protocol version; set to 0.
 * @data_len: Length of data which follows this header.
 */
struct ec_host_request {
	uint8_t struct_version;
	uint8_t checksum;
	uint16_t command;
	uint8_t command_version;
	uint8_t reserved;
	uint16_t data_len;
} __ec_align4;

#define EC_HOST_RESPONSE_VERSION 3

/**
 * struct ec_host_response - Version 3 response from EC.
 * @struct_version: Struct version (=3).
 * @checksum: Checksum of response and data; sum of all bytes including
 *            checksum should total to 0.
 * @result: EC's response to the command (separate from communication failure)
 * @data_len: Length of data which follows this header.
 * @reserved: Unused bytes in current protocol version; set to 0.
 */
struct ec_host_response {
	uint8_t struct_version;
	uint8_t checksum;
	uint16_t result;
	uint16_t data_len;
	uint16_t reserved;
} __ec_align4;

#endif  /* !__ACPI__ */

#ifdef __cplusplus
}
#endif
