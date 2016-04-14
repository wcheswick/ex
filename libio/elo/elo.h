/*
 * From the elo spec:
 */

#define ELO_PACKET_SIZE		10

extern	int setup_elo_tty(char *dev);
extern	int send_elo_command(u_char c[ELO_PACKET_SIZE]);
extern	int read_elo_response(u_char r[ELO_PACKET_SIZE]);
extern	int init_elo(void);
extern	int get_elo_touch(int *x, int *y, int *z, int *stat);
extern	int  is_elo_data(void);
extern	void elo_calibrate_and_scale(int x0, int x1, int y0, int y1,
		int c0, int c1, int r0, int r1);
