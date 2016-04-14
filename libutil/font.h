// #include <rune.h>

#define FONTDIR	"lib/fonts"
#define MAXENCODING 0xFFFF

#define FONTHEIGHT(fp)	((fp)->ascent + (fp)->descent + 1)

typedef u_char	byte;

struct rune_info {
	Point dwidth;		// move distance for next glyph
	Point bbx_size;		// BBOX size of pixelated area
	Point bbx_offset;	// BBOX lower left corner
	int bytes_per_row;
	byte *bitmap;
};

typedef u_int rune_t;		// hack

#define CHAR_ROWS	256
#define CHAR_COLS	256

typedef struct font font;
struct font {
	float	size;		// unused
	int	dpi_x, dpi_y;
	int	point_size;
	int	weight;
	int	resx;
	int	resy;
	int	xheight;
	int	quad_width;
	rune_t	default_char;
	int	descent;
	int	ascent;
	struct rune_info chars[CHAR_ROWS][CHAR_COLS];
};

extern	font *load_font(char *fn);
extern	void set_font(font *fp);
extern	void init_font_locale(char *loc);

extern	void write_string(Rectangle r, Pixel color, utf8 *str);
extern	void write_centered_string(Rectangle r, Pixel color, utf8 *str);
extern	void render_string(Rectangle r, utf8 *str,
	    void report_render_size(Rectangle r),
	    void bitmap_row(Point p, int len, byte *bitmap), int centerx, int centery);
extern	PixMap *string_to_pixmap(Rectangle r, Pixel color, utf8 *str,
		int cx, int cy);
extern	void string_on_pixmap(PixMap *pm, utf8 *str, Pixel color,
		int cx, int cy);

extern	font *current_font;
