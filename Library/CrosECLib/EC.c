#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/TimerLib.h>
#include <Library/CrosECLib.h>

#define memcpy CopyMem

typedef UINT16 USHORT;
typedef UINT8  UCHAR;

static __inline void outb(unsigned char __val, unsigned short __port)
{
	__asm__ volatile ("outb %0,%1" : : "a" (__val), "dN" (__port));
}

static __inline void outw(unsigned short __val, unsigned short __port)
{
	__asm__ volatile ("outw %0,%1" : : "a" (__val), "dN" (__port));
}

static __inline unsigned char inb(unsigned short __port)
{
	unsigned char __val;
	__asm__ volatile ("inb %1,%0" : "=a" (__val) : "dN" (__port));
	return __val;
}

static __inline unsigned short inw(unsigned short __port)
{
	unsigned short __val;
	__asm__ volatile ("inw %1,%0" : "=a" (__val) : "dN" (__port));
	return __val;
}

typedef enum _EC_TRANSFER_DIRECTION
{
	EC_XFER_WRITE,
	EC_XFER_READ
} EC_TRANSFER_DIRECTION;

// As defined in MEC172x section 16.8.3
// https://ww1.microchip.com/downloads/en/DeviceDoc/MEC172x-Data-Sheet-DS00003583C.pdf
#define MEC_EC_BYTE_ACCESS               0x00
#define MEC_EC_LONG_ACCESS_AUTOINCREMENT 0x03

#define MEC_LPC_ADDRESS_REGISTER0 0x0802
#define MEC_LPC_ADDRESS_REGISTER1 0x0803
#define MEC_LPC_DATA_REGISTER0    0x0804
#define MEC_LPC_DATA_REGISTER1    0x0805
#define MEC_LPC_DATA_REGISTER2    0x0806
#define MEC_LPC_DATA_REGISTER3    0x0807

static int ECTransfer(EC_TRANSFER_DIRECTION direction,
                      USHORT address,
                      char* data,
                      USHORT size) {
	int pos = 0;
	USHORT temp[2];
	if(address % 4 > 0) {
		outw((address & 0xFFFC) | MEC_EC_BYTE_ACCESS, MEC_LPC_ADDRESS_REGISTER0);
		/* Unaligned start address */
		for(int i = address % 4; i < 4; ++i) {
			char* storage = &data[pos++];
			if(direction == EC_XFER_WRITE)
				outb(*storage, MEC_LPC_DATA_REGISTER0 + i);
			else if(direction == EC_XFER_READ)
				*storage = inb(MEC_LPC_DATA_REGISTER0 + i);
		}
		address = (address + 4) & 0xFFFC;  // Up to next multiple of 4
	}

	if(size - pos >= 4) {
		outw((address & 0xFFFC) | MEC_EC_LONG_ACCESS_AUTOINCREMENT, MEC_LPC_ADDRESS_REGISTER0);
		// Chunk writing for anything large, 4 bytes at a time
		// Writing to 804, 806 automatically increments dest address
		while(size - pos >= 4) {
			if(direction == EC_XFER_WRITE) {
				memcpy(temp, &data[pos], sizeof(temp));
				outw(temp[0], MEC_LPC_DATA_REGISTER0);
				outw(temp[1], MEC_LPC_DATA_REGISTER2);
			} else if(direction == EC_XFER_READ) {
				temp[0] = inw(MEC_LPC_DATA_REGISTER0);
				temp[1] = inw(MEC_LPC_DATA_REGISTER2);
				memcpy(&data[pos], temp, sizeof(temp));
			}

			pos += 4;
			address += 4;
		}
	}

	if(size - pos > 0) {
		// Unaligned remaining data - R/W it by byte
		outw((address & 0xFFFC) | MEC_EC_BYTE_ACCESS, MEC_LPC_ADDRESS_REGISTER0);
		for(int i = 0; i < (size - pos); ++i) {
			char* storage = &data[pos + i];
			if(direction == EC_XFER_WRITE)
				outb(*storage, MEC_LPC_DATA_REGISTER0 + i);
			else if(direction == EC_XFER_READ)
				*storage = inb(MEC_LPC_DATA_REGISTER0 + i);
		}
	}
	return 0;
}

/*
 * Wait for the EC to be unbusy.  Returns 0 if unbusy, non-zero if
 * timeout.
 */
static int ECWaitForReady(int statusAddr, int timeoutUsec) {
	int i;
	int delay = 5;

	for(i = 0; i < timeoutUsec; i += delay) {
		/*
		 * Delay first, in case we just sent out a command but the EC
		 * hasn't raised the busy flag.  However, I think this doesn't
		 * happen since the LPC commands are executed in order and the
		 * busy flag is set by hardware.  Minor issue in any case,
		 * since the initial delay is very short.
		 */
		MicroSecondDelay(MIN(delay, timeoutUsec - i));

		if(!(inb(statusAddr) & EC_LPC_STATUS_BUSY_MASK))
			return 0;

		/* Increase the delay interval after a few rapid checks */
		if(i > 20)
			delay = MIN(delay * 2, 10000);
	}
	return -1; /* Timeout */
}

static UCHAR ECChecksumBuffer(char* data, int size) {
	UCHAR sum = 0;
	for(int i = 0; i < size; ++i) {
		sum += data[i];
	}
	return sum;
};

int ECSendCommandLPCv3(int command,
                       int version,
                       const void* outdata,
                       int outsize,
                       void* indata,
                       int insize) {
	int res = EC_RES_SUCCESS;
	UCHAR csum = 0;
	int i;

	union {
		struct ec_host_request rq;
		char data[EC_LPC_HOST_PACKET_SIZE];
	} u;

	union {
		struct ec_host_response rs;
		char data[EC_LPC_HOST_PACKET_SIZE];
	} r;

	/* Fail if output size is too big */
	if(outsize + sizeof(u.rq) > EC_LPC_HOST_PACKET_SIZE) {
		res = -EC_RES_REQUEST_TRUNCATED;
		goto Out;
	}

	/* Fill in request packet */
	/* TODO(crosbug.com/p/23825): This should be common to all protocols */
	u.rq.struct_version = EC_HOST_REQUEST_VERSION;
	u.rq.checksum = 0;
	u.rq.command = (USHORT)command;
	u.rq.command_version = (UCHAR)version;
	u.rq.reserved = 0;
	u.rq.data_len = (USHORT)outsize;

	memcpy(&u.data[sizeof(u.rq)], outdata, outsize);
	csum = ECChecksumBuffer(u.data, outsize + sizeof(u.rq));
	u.rq.checksum = (UCHAR)(-csum);

	if(ECWaitForReady(EC_LPC_ADDR_HOST_CMD, 1000000)) {
		res = -EC_RES_TIMEOUT;
		goto Out;
	}

	ECTransfer(EC_XFER_WRITE, 0, u.data, (USHORT)(outsize + sizeof(u.rq)));

	/* Start the command */
	outb(EC_COMMAND_PROTOCOL_3, EC_LPC_ADDR_HOST_CMD);

	if(ECWaitForReady(EC_LPC_ADDR_HOST_CMD, 1000000)) {
		res = -EC_RES_TIMEOUT;
		goto Out;
	}

	/* Check result */
	i = inb(EC_LPC_ADDR_HOST_DATA);
	if(i) {
		res = -EECRESULT - i;
		goto Out;
	}

	csum = 0;
	ECTransfer(EC_XFER_READ, 0, r.data, sizeof(r.rs));

	if(r.rs.struct_version != EC_HOST_RESPONSE_VERSION) {
		res = -EC_RES_INVALID_HEADER_VERSION;
		goto Out;
	}

	if(r.rs.reserved) {
		res = -EC_RES_INVALID_HEADER;
		goto Out;
	}

	if(r.rs.data_len > insize) {
		res = -EC_RES_RESPONSE_TOO_BIG;
		goto Out;
	}

	if(r.rs.data_len > 0) {
		ECTransfer(EC_XFER_READ, 8, r.data + sizeof(r.rs), r.rs.data_len);
		if(ECChecksumBuffer(r.data, sizeof(r.rs) + r.rs.data_len)) {
			res = -EC_RES_INVALID_CHECKSUM;
			goto Out;
		}

		memcpy(indata, r.data + sizeof(r.rs), r.rs.data_len);
	}
	res = r.rs.data_len;

Out:
	return res;
}
