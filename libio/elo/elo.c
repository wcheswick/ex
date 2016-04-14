/*
 * Interface routines for bare support of ELO touch screens.
 * Chunks of this stolen from xf86Elo.c, publicly-available xf86 driver
 * code available from the hardware manufacturer.
 *
 *	Bill Cheswick, Jan 2007.
 */

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#include <sys/select.h>

#include "elo.h"

int debug_elo = 0;

#define ELO_BAUD	B9600

/*
 ***************************************************************************
 *
 * Protocol constants.
 *
 ***************************************************************************
 */

#define ELO_SYNC_BYTE		'U'	/* Sync byte. First of a packet.	*/
#define ELO_TOUCH		'T'	/* Report of touchs and motions. Not	*
					 * used by 2310.			*/
#define ELO_OWNER		'O'	/* Report vendor name.			*/
#define ELO_ID			'I'	/* Report of type and features.		*/
#define ELO_MODE		'M'	/* Set current operating mode.		*/
#define ELO_PARAMETER		'P'	/* Set the serial parameters.		*/
#define ELO_REPORT		'B'	/* Set touch reports timings.		*/
#define ELO_ACK			'A'	/* Acknowledge packet			*/

#define ELO_CAL			'C'	/* Set calibration */
#define ELO_SCALE		'S'	/* Set scaling */

#define ELO_INIT_CHECKSUM	0xAA	/* Initial value of checksum.		*/

#define	ELO_PRESS		0x01	/* Flags in ELO_TOUCH status byte	*/
#define	ELO_STREAM		0x02
#define ELO_RELEASE		0x04

#define ELO_TOUCH_MODE		0x01	/* Flags in ELO_MODE command		*/
#define ELO_STREAM_MODE		0x02
#define ELO_UNTOUCH_MODE	0x04
#define ELO_RANGE_CHECK_MODE	0x40
#define ELO_TRIM_MODE		0x02
#define ELO_CALIB_MODE		0x04
#define ELO_SCALING_MODE	0x08
#define ELO_TRACKING_MODE	0x40

#define ELO_SERIAL_SPEED	0x06	/* Flags for high speed serial (19200)	*/
#define ELO_SERIAL_MASK		0xF8

#define ELO_SERIAL_IO		'0'	/* Indicator byte for PARAMETER command */

/*
 ***************************************************************************
 *
 * Default constants.
 *
 ***************************************************************************
 */
#define ELO_MAX_TRIALS	3		/* Number of timeouts waiting for a	*/
					/* pending reply.			*/
#define ELO_MAX_WAIT		100000	/* Max wait time for a reply (microsec)	*/
#define ELO_UNTOUCH_DELAY	5	/* 100 ms				*/
#define ELO_REPORT_DELAY	1	/* 40 ms or 25 motion reports/s		*/

#define DEFAULT_MAX_X		3000
#define DEFAULT_MIN_X		600
#define DEFAULT_MAX_Y		3000
#define DEFAULT_MIN_Y		600

int elo_fd = -1;

struct termios oldsb;
struct termios newsb;

#ifdef notdef
static void
set_blocking(int on) {
	if (on) {
		if (fcntl(elo_fd, F_SETFL, 0) < 0)
			perror("set_blocking: fcntl 1");
	} else {
		if (fcntl(elo_fd, F_SETFL, O_NONBLOCK) < 0)
			perror("set_blocking: fcntl 2");
	}

}
#endif

int
setup_elo_tty(char *dev) {
	elo_fd = open(dev, O_RDWR, 0666);
	if (elo_fd < 0) {
		perror(dev);
		return -1;
	}

	if (tcgetattr(elo_fd, &oldsb) < 0) {
		perror("setup_elo_tty tcgetattr");
		return -1;
	}

	newsb = oldsb;
	if (cfsetispeed(&newsb, ELO_BAUD)) {
		perror("setup_elo_tty cfsetispeed");
		return -1;
	}

	if (cfsetospeed(&newsb, ELO_BAUD)) {
		perror("setup_elo_tty cfsetospeed");
		return -1;
	}

	cfmakeraw(&newsb);

	newsb.c_cflag |= CLOCAL|CREAD;
	if (tcsetattr(elo_fd, TCSANOW, &newsb) < 0) {
		perror("setup_elo_tty");
		return -1;
	}

	if (debug_elo)
		fprintf(stderr, "setup_elo_tty: %s ready\n", dev);
	return 0;
}

char *
elo_command(char *cmd) {
	static char buf[100];
	char *cp = buf;
	int x, y, z;

	switch (tolower(*cmd)) {
	case 'a':	cp = "acknowledge";	break;
	case 'b':	cp = "report";		break;
	case 'c':	cp = "calibration";	break;
	case 'd':	cp = "diagnostics";	break;
	case 'e':	cp = "emulate";		break;
	case 'f':	cp = "filter";		break;
	case 'g':	cp = "configuration";	break;
	case 'h':	cp = "timer";		break;
	case 'i':	cp = "ID";		break;
	case 'j':	cp = "jumpers";		break;
	case 'k':	cp = "key";		break;
	case 'l':	cp = "low power";	break;
	case 'm':	cp = "mode";		break;
	case 'n':	cp = "Nonvolatile RAM";	break;
	case 'o':	cp = "owner";		break;
	case 'p':	cp = "parameter";	break;
	case 'q':	cp = "quiet";		break;
	case 'r':	cp = "reset";		break;
	case 's':	cp = "scaling";		break;
	case 't':
		x = (cmd[3] & 0xff)*256 + (0xff & cmd[2]);
		y = (cmd[5] & 0xff)*256 + (0xff & cmd[4]);
		z = (cmd[7] & 0xff)*256 + (0xff & cmd[6]);
		snprintf(buf, sizeof(buf), "touch  %s%s%s%s%s%s%s%s  %d,%d,%d",
			(cmd[1] & 0x80) ? "z" : ".",
			(cmd[1] & 0x40) ? "o" : ".",
			(cmd[1] & 0x20) ? "R" : ".",
			(cmd[1] & 0x10) ? "w" : ".",
			(cmd[1] & 0x08) ? "R" : ".",
			(cmd[1] & 0x04) ? "u" : ".",
			(cmd[1] & 0x02) ? "s" : ".",
			(cmd[1] & 0x01) ? "i" : ".",
			x, y, z);
		break;
	default:
		snprintf(buf, sizeof(buf), "unknown (%.02x", *cmd);
	}
	return cp;
}

static void
dump_pkt(u_char *p) {
	int i;

	printf("		%.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x\n",
		p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
	printf("		");
	for (i=0; i<ELO_PACKET_SIZE; i++)
		if (p[i] >= 0x20 && p[i] < 0x7F)
			printf(" %c ", p[i]);
		else
			printf("%.02x ", p[i]);

	printf("%s\n", elo_command(&p[1]));
}

int
elo_send_packet(u_char *pkt) {
	int sum = ELO_INIT_CHECKSUM;
	int i;

	pkt[0] = ELO_SYNC_BYTE;
	for (i=0; i< ELO_PACKET_SIZE-1; i++) {
		sum += pkt[i];
		sum &= 0xff;
	}
	pkt[ELO_PACKET_SIZE-1] = sum;

	if (debug_elo > 1) {
		fprintf(stderr, "elo_send_packet:\n");
		dump_pkt(pkt);
	}

	i = write(elo_fd, pkt, ELO_PACKET_SIZE);
	if (i != ELO_PACKET_SIZE) {
		perror("elo_send_packet");
		return 0;
	}
	return 1;
}

/*
 ***************************************************************************
 *
 * elo_get_packet_part --
 *	Read a packet from the port. Try to synchronize with start of
 *	packet and compute checksum.
 *      The packet structure read by this function is as follow:
 *		Byte 0 : ELO_SYNC_BYTE
 *		Byte 1
 *		...
 *		Byte 8 : packet data
 *		Byte 9 : checksum of bytes 0 to 8
 *
 *	This function returns if a valid packet has been assembled in
 *	buffer or if no more data is available.
 *
 *	Returns Success if a packet is successfully assembled including
 *	testing checksum. If a packet checksum is incorrect, it is discarded.
 *	Bytes preceding the ELO_SYNC_BYTE are also discarded.
 *	Returns 0 if out of data while reading. The start of the
 *	partially assembled packet is left in buffer, buffer_p and
 *	checksum reflect the current state of assembly.
 *
 ***************************************************************************
 */

static int
elo_get_packet_part(u_char *buff, int *bufp, int *chksum) {
	int ok;

	int n = read(elo_fd, (char *) (buff + *bufp), ELO_PACKET_SIZE - *bufp);
	if (n < 0) {
		perror("elo_get_packet_part read");
		return 0;
	}

	if (debug_elo > 2) {
		if (debug_elo > 3) {
			int i;
			fprintf(stderr, "elo_get_packet_part: read %d, bufp=%d, %d\n",
				n, *bufp, ELO_PACKET_SIZE);

			for (i=0; i<n; i++)
				fprintf(stderr, "%.02x ", buff[i]);
			fprintf(stderr, "\n");
		} else
			fprintf(stderr, "elo_get_packet_part:\n");
		dump_pkt(buff);
	}

	while (n > 0) {
		if ((*bufp == 0) && (buff[0] != ELO_SYNC_BYTE)) {
			perror("elo_get_packet_part: syncing");
			memcpy(&buff[0], &buff[1], n-1);
		} else {
			/* compute checksum in assembly buffer */

			if (*bufp < ELO_PACKET_SIZE-1) {
				*chksum += buff[*bufp];
				*chksum %= 256;
			}
			(*bufp)++;
		}
		n--;
	}

	if (*bufp == ELO_PACKET_SIZE) {
		ok = (*chksum == buff[ELO_PACKET_SIZE-1]);
		*chksum = ELO_INIT_CHECKSUM;
		*bufp = 0;

		if (!ok) {
			perror("elo_get_packet_part: checksum error");
			return 0;
		}
		if(debug_elo > 2)
			fprintf(stderr, "elo_get_packet_part succeeded\n");
		return 1;
	}
	return 0;
}

int
is_elo_data(void) {
	fd_set readfds;
	struct timeval to;
	int r;

	FD_ZERO(&readfds);
	FD_SET(elo_fd, &readfds);
	to.tv_sec = 0;
	to.tv_usec = ELO_MAX_WAIT;

	r = select(FD_SETSIZE, &readfds, NULL, NULL, &to);
	return r;
}

static void
elo_proto_err(u_char *reply) {
	int i;

	for (i=2; i < 2+4 && reply[i] != '0'; i++) {
		char *err, buf[100];

		switch (reply[i]) {
		case '1':	err = "Divide by zero";			break;
		case '2':	err = "Bad input packet";		break;
		case '3':	err = "Bad input checksum";		break;
		case '4':	err = "Input packet overrun";		break;
		case '5':	err = "Illegal command";		break;
		case '6':	err = "Calibration command cancelled";	break;
		case '8':	err = "Bad serial setup combination";	break;
		case '9':	err = "NVRAM not valid - initializing";	break;
		case 'A':	err = "No set available for this command"; break;
		case 'B':	err = "Unsupported firmware version";	break;
		case 'C':	err = "Illegal subcommand";		break;
		case 'D':	err = "Operand out of range";		break;
		case 'E':	err = "Invalid type";			break;
		case 'F':	err = "Fatal error condition exists";	break;
		case 'G':	err = "No query available for this command"; break;
		case 'H':	err = "Invalid interrupt number";	break;
		case 'I':	err = "NVRAM failure";			break;
		case 'J':	err = "Invalid address number";		break;
		case 'K':	err = "Power-on self-test failed";	break;
		case 'L':	err = "Operation failed";		break;
		case 'M':	err = "Measurement warning";		break;
		case 'N':	err = "Measurement error";		break;
		default:
			snprintf(buf, sizeof(buf), "error code %c", reply[i]);
			err = buf;
		}
		fprintf(stderr, "Touchscreen error: %s\n", err);
	}
}

/*
 * Returns:
 *	0: no reply
 *	1: reply received
 *	-1: error
 */
static int
elo_wait_reply(u_char type, u_char *reply) {
	int trials = ELO_MAX_TRIALS;
	int reply_p = 0;
	int sum = ELO_INIT_CHECKSUM;
	int ok = 0, r;

	if (debug_elo > 2) {
		fprintf(stderr, "elo_wait_reply: waiting for %c (0x%.02x, %s)\n",
			type, type, elo_command(&type));
	}

	do { 	r = is_elo_data();
		if (r > 0) {
			ok = elo_get_packet_part(reply, &reply_p, &sum);
			if (ok && reply[1] != type && type != ELO_PARAMETER) {
				elo_proto_err(reply);
				ok = 0;
			}
		} else
			return 0;
		if (r == 0)
			trials--;
	} while (!ok && trials > 0);

	if (debug_elo > 1) {
		fprintf(stderr, "elo_wait_reply: %s\n", ok ? "ok" : "failed");
		dump_pkt(reply);
	}
	return ok;
}

static int
elo_wait_ack(void) {
	u_char pkt[ELO_PACKET_SIZE];

	if (debug_elo > 1)
		fprintf(stderr, "elo_wait_ack:\n");

	if (elo_wait_reply(ELO_ACK, pkt) == 1) {
		int i, nerr;

		for (i=0, nerr=0; i<4; i++)
			if (pkt[2+i] != '0')
				nerr++;
		if (nerr)
			fprintf(stderr, "elo_wait_ack: reports %d errors\n", nerr);
		return 1;
	}
	return 0;
}

int
elo_cmd(u_char *cmd, u_char *reply) {
	if (!elo_send_packet(cmd))
		return 0;

	if (islower(cmd[1]) && !elo_wait_reply(toupper(cmd[1]), reply))
		return 0;

	if (!elo_wait_ack())
		return 0;
	return 1;
}

static void
print_elo_id(u_char *pkt) {

	if (debug_elo > 3) {
		fprintf(stderr, "print_elo_id:\n");
		dump_pkt(pkt);
	}
	switch(pkt[2]) {
	case '0':	fprintf(stderr, "AccuTouch");		break;
	case '1':	fprintf(stderr, "DuraTouch");		break;
	case '2':	fprintf(stderr, "Intellitouch");	break;
	default:	fprintf(stderr, "unknown (%d)", pkt[2]);
	}

	switch(pkt[3]) {
	case '0':	fprintf(stderr, " serial");		break;
	case '1':	fprintf(stderr, " PC bus");		break;
	case '2':	fprintf(stderr, " Microchanell");	break;
	default:	fprintf(stderr, " unknown (%d)", pkt[2]);
	}

	// skip controller check for now

	fprintf(stderr, " firmware rev %d.%d.  ", pkt[6], pkt[5]);
	if (pkt[4]) {
		fprintf(stderr, "    additional features: ");
		if (pkt[4] & 0x10)
			fprintf(stderr, "external A/D ");
		if (pkt[4] & 0x20)
			fprintf(stderr, "32K RAM ");
		if (pkt[4] & 0x40)
			fprintf(stderr, "RAM onboard ");
		if (pkt[4] & 0x80)
			fprintf(stderr, "z axis active ");
	}
	fprintf(stderr, "g pkts: %d  ctlr ", pkt[7]);
	switch (pkt[8]) {
	case 0x00:	fprintf(stderr, "E271-2200");		break;
	case 0x01:	fprintf(stderr, "E271-2210");		break;
	case 0x03:	fprintf(stderr, "E281-2300");		break;
	case 0x05:	fprintf(stderr, "E281-2310B");		break;
	case 0x06:	fprintf(stderr, "2500S");		break;
	case 0x07:	fprintf(stderr, "2500U");		break;
	case 0x08:	fprintf(stderr, "3000U");		break;
	case 0x09:	fprintf(stderr, "4000U");		break;
	case 0x0a:	fprintf(stderr, "4000S");		break;
	case 0x0e0:	fprintf(stderr, "COACh IIs");		break;
	default:	fprintf(stderr, "code 0x%.02x\n", pkt[8]);
	}
	fprintf(stderr, "\n");
}

int
init_elo(void) {
	u_char req[ELO_PACKET_SIZE];
	u_char reply[ELO_PACKET_SIZE];
	int rc;

	if (debug_elo)
		fprintf(stderr, "init_elo\n");

	memset(req, 0, sizeof(req));
	req[1]= tolower(ELO_ID);

	if ((rc=elo_cmd(req, reply))) {
		print_elo_id(reply);
	} else
		fprintf(stderr, "Unable to ask elo touchscreen id\n");

	memset(req, 0, ELO_PACKET_SIZE);
	req[1] = ELO_MODE;
	req[3] = ELO_TOUCH_MODE | ELO_STREAM_MODE | ELO_UNTOUCH_MODE;
	req[4] = ELO_TRACKING_MODE;
	if (!elo_cmd(req, reply)) {
		fprintf(stderr, "init_elo: failed setting modes\n");
		return -1;
	}

	// link speed delay adjustment?

	memset(req, 0, ELO_PACKET_SIZE);
	req[1] = tolower(ELO_REPORT);
	req[2] = ELO_UNTOUCH_DELAY;
	req[3] = ELO_REPORT_DELAY;
	if (!elo_cmd(req, reply))
		fprintf(stderr, "init_elo: failed setting delays\n");

	return 1;
}

/*
 * Return the touch information.  Zero is returned if no touch, else 1, and
 * x, y, and z are set, and stat has:
 *
 * 0000 0rsp
 * 	where	r if touch released
 *		s stream continues
 *		i initial touch
 */
int
get_elo_touch(int *x, int *y, int *z, int *stat) {
	u_char reply[ELO_PACKET_SIZE];
	int rc = elo_wait_reply(ELO_TOUCH, reply);

	if (rc == 0)
		return 0;

	*x = (reply[4] & 0xff)*256 + (0xff & reply[3]);

	*y = (reply[6] & 0xff)*256 + (0xff & reply[5]);
	*z = (reply[8] & 0xff)*256 + (0xff & reply[7]);
	*stat = reply[2] & (ELO_PRESS|ELO_STREAM|ELO_RELEASE);

	if (debug_elo > 1)
		fprintf(stderr, "get_touch: rc = %d\n", rc);
	if (debug_elo > 2)
		dump_pkt(reply);
	return rc;
}

void
elo_calibrate_and_scale(int x0, int x1, int y0, int y1,
    int c0, int c1, int r0, int r1) {
	u_char req[ELO_PACKET_SIZE];
	u_char reply[ELO_PACKET_SIZE];

	memset(req, 0, ELO_PACKET_SIZE);	// calibrate X
	req[1] = ELO_CAL;
	req[2] = 'X';
	req[3] = x0 % 256;
	req[4] = x0 / 256;
	req[5] = x1 % 256;
	req[6] = x1 / 256;
	if (!elo_cmd(req, reply))
		fprintf(stderr, "elo_calibrate_and_scale: failed X cal\n");

	memset(req, 0, ELO_PACKET_SIZE);	// calibrate Y
	req[1] = ELO_CAL;
	req[2] = 'Y';
	req[3] = y0 % 256;
	req[4] = y0 / 256;
	req[5] = y1 % 256;
	req[6] = y1 / 256;
	if (!elo_cmd(req, reply))
		fprintf(stderr, "elo_calibrate_and_scale: failed Y cal\n");

	memset(req, 0, ELO_PACKET_SIZE);	// scale X
	req[1] = ELO_SCALE;
	req[2] = 'X';
	req[3] = c0 % 256;
	req[4] = c0 / 256;
	req[5] = c1 % 256;
	req[6] = c1 / 256;
	if (!elo_cmd(req, reply))
		fprintf(stderr, "elo_calibrate_and_scale: failed X scale\n");

	memset(req, 0, ELO_PACKET_SIZE);	// scale Y
	req[1] = ELO_SCALE;
	req[2] = 'Y';
	req[3] = r0 % 256;
	req[4] = r0 / 256;
	req[5] = r1 % 256;
	req[6] = r1 / 256;
	if (!elo_cmd(req, reply))
		fprintf(stderr, "elo_calibrate_and_scale: failed Y scale\n");

	memset(req, 0, ELO_PACKET_SIZE);
	req[1] = ELO_MODE;
	req[3] = ELO_TOUCH_MODE | ELO_STREAM_MODE | ELO_UNTOUCH_MODE;
	req[4] = ELO_TRACKING_MODE | ELO_CALIB_MODE | ELO_SCALING_MODE;
	if (!elo_cmd(req, reply))
		fprintf(stderr, "elo_calibrate_and_scale: failed setting modes\n");
}

