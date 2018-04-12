/*
 * Copyright (c) 2000
 *	Traakan, Inc., Los Altos, CA
 *	All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Project:  NDMJOB
 * Ident:    $Id: $
 *
 * Description:
 *	NDMPv0, represented here, is a ficticious version
 *	used to negotiate NDMP protocol version for the
 *	remainder of the session. Early, as a connection is
 *	being set up, the version of the protocol is unknown.
 *	The first messages exchanged negotiate the protocol
 *	version, and such messages are in the NDMP format.
 *	This is different than other protocols, such as ONC RPC
 *	which negotiate version by lower layers before the
 *	objective protocol becomes involved. During the
 *	negotiation, we deem the connection to be in "v0" mode.
 *	This NDMPv0 protocol specification is the subset of
 *	the NDMP protocol(s) required for the negotiation,
 *	and necessarily must remain immutable for all time.
 */


/*
 * Copyright (c) 1997 Network Appliance. All Rights Reserved.
 *
 * Network Appliance makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 *
 */
%#if __clang__
%#pragma clang diagnostic ignored "-Wunused-const-variable"
%#elif __GNUC__
%#if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 402
%#pragma GCC diagnostic ignored "-Wunused-variable"
%#endif
%#endif

const NDMPPORT = 10000;

enum ndmp0_error
{
	NDMP0_NO_ERR,			/* No error */
	NDMP0_NOT_SUPPORTED_ERR,	/* Call is not supported */
	NDMP0_DEVICE_BUSY_ERR,		/* The device is in use */
	NDMP0_DEVICE_OPENED_ERR,	/* Another tape or scsi device
					 * is already open */
	NDMP0_NOT_AUTHORIZED_ERR,	/* connection has not been authorized*/
	NDMP0_PERMISSION_ERR,		/* some sort of permission problem */
	NDMP0_DEV_NOT_OPEN_ERR,		/* SCSI device is not open */
	NDMP0_IO_ERR,			/* I/O error */
	NDMP0_TIMEOUT_ERR,		/* command timed out */
	NDMP0_ILLEGAL_ARGS_ERR,		/* illegal arguments in request */
	NDMP0_NO_TAPE_LOADED_ERR,	/* Cannot open because there is
					   no tape loaded */
	NDMP0_WRITE_PROTECT_ERR,	/* tape cannot be open for write */
	NDMP0_EOF_ERR,			/* Command encountered EOF */
	NDMP0_EOM_ERR,			/* Command encountered EOM */
	NDMP0_FILE_NOT_FOUND_ERR,	/* File not found during restore */
	NDMP0_BAD_FILE_ERR,		/* The file descriptor is invalid */
	NDMP0_NO_DEVICE_ERR,		/* The device is not at that target */
	NDMP0_NO_BUS_ERR,		/* Invalid controller */
	NDMP0_XDR_DECODE_ERR,		/* Can't decode the request argument */
	NDMP0_ILLEGAL_STATE_ERR,	/* Call can't be done at this state */
	NDMP0_UNDEFINED_ERR,		/* Undefined Error */
	NDMP0_XDR_ENCODE_ERR,		/* Can't encode the reply argument */
	NDMP0_NO_MEM_ERR		/* no memory */
};

enum ndmp0_header_message_type
{
	NDMP0_MESSAGE_REQUEST,
	NDMP0_MESSAGE_REPLY
};

enum ndmp0_message
{
	NDMP0_CONNECT_OPEN = 0x900,	/* CONNECT INTERFACE */
	NDMP0_CONNECT_CLOSE = 0x902,

	NDMP0_NOTIFY_CONNECTED = 0x502
};

struct ndmp0_header
{
	uint32_t		sequence;	/* monotonically increasing */
	uint32_t		time_stamp;	/* time stamp of message */
	ndmp0_header_message_type message_type;	/* what type of message */
	ndmp0_message		message;	/* message number */
	uint32_t		reply_sequence;	/* reply is in response to */
	ndmp0_error		error;		/* communications errors */
};

/**********************/
/*  CONNECT INTERFACE */
/**********************/
/* NDMP0_CONNECT_OPEN */
struct ndmp0_connect_open_request
{
	uint16_t	protocol_version;	/* the version of protocol supported */
};

struct ndmp0_connect_open_reply
{
	ndmp0_error	error;
};

/* NDMP0_CONNECT_CLOSE */
/* no request arguments */
/* no reply arguments */

/****************************/
/* NOTIFY INTERFACE	    */
/****************************/

/* NDMP0_NOTIFY_CONNECTED */
enum ndmp0_connect_reason
{
	NDMP0_CONNECTED,	/* Connect sucessfully */
	NDMP0_SHUTDOWN,		/* Connection shutdown */
	NDMP0_REFUSED		/* reach the maximum number of connections */
};

struct ndmp0_notify_connected_request
{
	ndmp0_connect_reason	reason;
	uint16_t		protocol_version;
	string			text_reason<>;
};
