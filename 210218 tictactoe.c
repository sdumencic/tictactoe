#define F_CPU 7372800UL

#include <avr/cpufunc.h>
#include <avr/io.h>
#include <util/delay.h>

// Colors
#define LBLUE 0x963D
#define WHITE 0xFFFF
#define BLACK 0x0000
#define GREEN 0xc72b
#define RED   0xD369
#define CYAN  0x1AAE

// Screen dimensions
#define MAX_X 240
#define MAX_Y 320

// Drawing definitions
#define XBR 10                    // space between grid and edge of screen
#define YBR 90                    // space between grid and edge of screen
#define SKP 10                    // space between grid and characters 'X' or 'O'
#define DIM 60                    // width of field where 'X' or 'O' are drawn
#define RAD DIM / 2	              // radius
#define BBR 40                    // button border
#define BBSX 40                   // back button size x-axis
#define BBSY 70                   // back button size y-axis
#define BDX (MAX_X - 3 * BBR) / 2 // height of button x-axis
#define BDY (MAX_Y - 3 * BBR) / 2 // height of button y-axis

// Pinout
#define LCD_DATA_H PORTB // data pins DB8-DB15
#define LCD_DATA_L PORTA // data pins DB0-DB7

#define LCD_RS    PC0 // changing between commands and data
#define LCD_WR    PC1 // write data
#define LCD_RD    PC2 // read data
#define LCD_CS    PC6 // chip select
#define LCD_RESET PC7 // lcd reset

#define T_CLK PD0 // touch controller clock
#define T_CS  PD1 // chip select
#define T_DIN PD2 // sending commands or data to touch part of screen, x and y coordinates
#define T_DO  PD3 // receiving data from touch part of screen
#define T_IRQ PD4 // interrupt, 1 if the screen is being touched

// RS definitions
#define CMD 0  // command
#define DATA 1 // data

// Game definitions
#define EMPTY 0
#define CROSS 1
#define NOUGHT 2
#define DRAW 3

// These are used to set is_AI flag
#define HUMAN 0
#define AI 1

// These are used for minimax return values
#define SCORE_WIN   1
#define SCORE_DRAW  0
#define SCORE_LOSS -1

static const unsigned char font[29][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 41 A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // 42 B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 43 C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 44 D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 45 E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // 46 F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 47 G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 48 H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 49 I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // 4a J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // 4b K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 4c L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 4d M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 4e N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 4f O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 50 P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 51 Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 52 R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 53 S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 54 T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 55 U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 56 V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 57 W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 58 X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // 59 Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // 5a Z
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 20 space
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 31 1
    {0x42, 0x61, 0x51, 0x49, 0x46}  // 32 2
};

uint8_t get_bit(uint8_t reg, uint8_t offset) {
    return (reg >> offset) & 1;
}

// touch part starts working
void TFT_start() {
    PORTD |= _BV(T_CS) | _BV(T_CLK) | _BV(T_DIN);
}

// writes commands to touch
void TFT_touch_write(uint8_t num) {
    PORTD &= ~_BV(T_CLK);
    for (uint8_t i = 0; i < 8; i++) {
        if (get_bit(num, 7 - i)) {
            PORTD |= _BV(T_DIN);
            } else {
            PORTD &= ~_BV(T_DIN);
        }
        PORTD &= ~_BV(T_CLK);
        PORTD |= _BV(T_CLK);
    }
}

// reads data from ADC on touch part of the screen (coordiates)
uint16_t TFT_touch_read() {
    uint16_t value = 0;
    for (uint8_t i = 0; i < 12; i++) {
        value <<= 1;
        PORTD |= _BV(T_CLK);            // high signal on T_CLK
        PORTD &= ~_BV(T_CLK);           // low signal on T_CLK, initialization of data transfer
        value += get_bit(PIND, T_DO);   // touch has 12-bit ADC, counting 0-12, taking one by one bit from T_DO,
    }

    return value;
}

// sending commands to screen
void TFT_write(uint16_t val, uint8_t rs) {
    if (rs) {                   // rs == 1 - data
        PORTC |= _BV(LCD_RS);
    } else {                    // rs == 0 - command
        PORTC &= ~_BV(LCD_RS);
    }
    PORTC &= ~_BV(LCD_CS);
    LCD_DATA_H = val >> 8;
    LCD_DATA_L = val;
    PORTC |= _BV(LCD_WR);
    PORTC &= ~_BV(LCD_WR);
    PORTC |= _BV(LCD_CS);
}

// sending specified command and value to memory
void TFT_write_pair(uint16_t cmd, uint16_t data) {
    TFT_write(cmd, CMD);
    TFT_write(data, DATA);
}

// coordinates that define where elements will be drawn
void TFT_set_address(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    TFT_write_pair(0x0044, (x2 << 8) + x1);
    TFT_write_pair(0x0045, y1);
    TFT_write_pair(0x0046, y2);
    TFT_write_pair(0x004e, x1);
    TFT_write_pair(0x004f, y1);
    TFT_write(0x0022, CMD);
}

void TFT_init(void) {
    DDRA = 0xff ;
    DDRB = 0xff;
    DDRC = 0xff;
    DDRD = ~(_BV(T_DO) | _BV(T_IRQ)); // pins for receiving data

    // initializing lcd configuration
    PORTC |= _BV(LCD_RESET);
    _delay_ms(5);
    PORTC &= ~_BV(LCD_RESET);
    _delay_ms(10);
    PORTC |= _BV(LCD_RESET);
    PORTC |= _BV(LCD_CS);
    PORTC |= _BV(LCD_RD);
    PORTC &= ~_BV(LCD_WR);
    _delay_ms(20);

    TFT_write_pair(0x0000, 0x0001); _delay_ms(1);
    TFT_write_pair(0x0003, 0xA8A4); _delay_ms(1);
    TFT_write_pair(0x000C, 0x0000); _delay_ms(1);
    TFT_write_pair(0x000D, 0x080C); _delay_ms(1);
    TFT_write_pair(0x000E, 0x2B00); _delay_ms(1);
    TFT_write_pair(0x001E, 0x00B0); _delay_ms(1);
    TFT_write_pair(0x0001, 0x2B3F); _delay_ms(1);
    TFT_write_pair(0x0002, 0x0600); _delay_ms(1);
    TFT_write_pair(0x0010, 0x0000); _delay_ms(1);
    TFT_write_pair(0x0011, 0x6070); _delay_ms(1);
    TFT_write_pair(0x0005, 0x0000); _delay_ms(1);
    TFT_write_pair(0x0006, 0x0000); _delay_ms(1);
    TFT_write_pair(0x0016, 0xEF1C); _delay_ms(1);
    TFT_write_pair(0x0017, 0x0003); _delay_ms(1);
    TFT_write_pair(0x0007, 0x0233); _delay_ms(1);
    TFT_write_pair(0x000B, 0x0000); _delay_ms(1);
    TFT_write_pair(0x000F, 0x0000); _delay_ms(1);
    TFT_write_pair(0x0041, 0x0000); _delay_ms(1);
    TFT_write_pair(0x0042, 0x0000); _delay_ms(1);
    TFT_write_pair(0x0048, 0x0000); _delay_ms(1);
    TFT_write_pair(0x0049, 0x013F); _delay_ms(1);
    TFT_write_pair(0x004A, 0x0000); _delay_ms(1);
    TFT_write_pair(0x004B, 0x0000); _delay_ms(1);
    TFT_write_pair(0x0044, 0xEF00); _delay_ms(1);
    TFT_write_pair(0x0045, 0x0000); _delay_ms(1);
    TFT_write_pair(0x0046, 0x013F); _delay_ms(1);
    TFT_write_pair(0x0030, 0x0707); _delay_ms(1);
    TFT_write_pair(0x0031, 0x0204); _delay_ms(1);
    TFT_write_pair(0x0032, 0x0204); _delay_ms(1);
    TFT_write_pair(0x0033, 0x0502); _delay_ms(1);
    TFT_write_pair(0x0034, 0x0507); _delay_ms(1);
    TFT_write_pair(0x0035, 0x0204); _delay_ms(1);
    TFT_write_pair(0x0036, 0x0204); _delay_ms(1);
    TFT_write_pair(0x0037, 0x0502); _delay_ms(1);
    TFT_write_pair(0x003A, 0x0302); _delay_ms(1);
    TFT_write_pair(0x003B, 0x0302); _delay_ms(1);
    TFT_write_pair(0x0023, 0x0000); _delay_ms(1);
    TFT_write_pair(0x0024, 0x0000); _delay_ms(1);

    TFT_write_pair(0x004f, 0);
    TFT_write_pair(0x004e, 0);
    TFT_write(0x0022, CMD);
}

// setting cursor to a specific position
void TFT_set_cursor(uint16_t x, uint16_t y) {
    TFT_write_pair(0x004E, x);
    TFT_write_pair(0x004F, MAX_Y - y);
    TFT_write(0x0022, CMD);
}

// reading x and y coordinates from touch part of screen
void read_touch_coords(uint16_t *TP_X, uint16_t *TP_Y) {
    _delay_ms(1);

    PORTD &= ~_BV(T_CS);

    TFT_touch_write(0x90); // sending command to touch part of screen to write y coordinate

    _delay_ms(1);
    PORTD |= _BV(T_CLK);  _NOP(); _NOP(); _NOP(); _NOP();
    PORTD &= ~_BV(T_CLK); _NOP(); _NOP(); _NOP(); _NOP();
    *TP_Y = (TFT_touch_read() - 80) / 6;

    TFT_touch_write(0xD0); // sendng command to touch part of screen to write x coordinate
    PORTD |= _BV(T_CLK);  _NOP(); _NOP(); _NOP(); _NOP();
    PORTD &= ~_BV(T_CLK); _NOP(); _NOP(); _NOP(); _NOP();
    *TP_X = (TFT_touch_read() - 80) / 8;

    PORTD |= _BV(T_CS);
}

// fill the screen with the specied color
void set_background_color(uint16_t color) {
    TFT_set_address(0, 0, 239, 319);

    for (uint16_t i = 0; i < 320; i++) {
        for (uint8_t j = 0; j < 240; j++) {
            TFT_write(color, DATA);
        }
    }
}

// setting a color of a pixel at the specified position
void draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    PORTC &= ~_BV(LCD_CS);
    TFT_set_cursor(x, y);
    TFT_write(color, DATA);
    PORTC |= _BV(LCD_CS);
}

// setting a color of a pixel at the specified position for a letter
void draw_font_pixel(uint16_t x, uint16_t y, uint16_t color, uint8_t pixel_size) {
    for(uint8_t i = 0; i < pixel_size; i++) {
        for(uint8_t j = 0; j < pixel_size; j++) {
            draw_pixel(x + i, y + j, color);
        }
    }
}

// setting a color to the pixels needed to write the specified character
void print_char(uint16_t x, uint16_t y, uint8_t font_size, uint16_t color, uint16_t back_color, uint8_t val) {
    for (uint8_t i = 0x00; i < 0x05; i++) {
        uint8_t value = font[val][i];
        for (uint8_t j = 0x00; j < 0x08; j++) {
            if ((value >> j) & 0x01) {
                draw_font_pixel(x + j * font_size, y, color, font_size);
            } else {
                draw_font_pixel(x + j * font_size, y, back_color, font_size);
            }
        }
        y += font_size;
    }
}

// setting a color to the pixels needed to write the specified string
void print_string(uint16_t x, uint16_t y, uint8_t font_size, uint16_t color, uint16_t back_color, const char *ch) {
    uint8_t cnt = 0;

    do {
        if (ch[cnt] == ' ') {
            print_char(x + font_size, y, font_size, color, back_color, 26);
        } else if (ch[cnt] == '1') {
            print_char(x + font_size, y, font_size, color, back_color, 27);
        } else if (ch[cnt] == '2') {
            print_char(x + font_size, y, font_size, color, back_color, 28);
        } else {
            print_char(x + font_size, y, font_size, color, back_color, ch[cnt] - 'A');
        }
        cnt++;
        y += 0x05 * font_size + 0x01;
    } while(ch[cnt] != '\0');
}

// setting a color to the pixels in a horizontal line
void draw_h_line(uint16_t x1, uint16_t y1, uint16_t y2, uint16_t color) {
    for (; y1 < y2; y1++) {
        draw_pixel(x1, y1, color);
    }
}

// setting a color to the pixels in a vetical line
void draw_v_line(uint16_t y1, uint16_t x1, uint16_t x2, uint16_t color) {
    for (; x1 < x2; x1++) {
        draw_pixel(x1, y1, color);
    }
}

// setting a color to the pixels in two diagonal lines
void draw_cross(uint16_t x, uint16_t y, uint16_t d, uint16_t color) {
    for (uint8_t i = 0; i < d; i++) {
        draw_pixel(x + i, y + i, color);
        draw_pixel(x + i, d - i + y, color);
    }
}

// setting a color to the pixels in circle
void draw_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
    x0 += r;
    y0 += r;

    int16_t x = -r, y = 0, err = 2 - 2 * r, e2;
    do {
        draw_pixel(x0 - x, y0 + y, color);
        draw_pixel(x0 + x, y0 + y, color);
        draw_pixel(x0 + x, y0 - y, color);
        draw_pixel(x0 - x, y0 - y, color);

        e2 = err;
        if (e2 <= y) {
            err += ++y * 2 + 1;
            if (-x == y && e2 <= x)
            e2 = 0;
        }
        if (e2 > x) {
            err += ++x * 2 + 1;
        }
    } while (x <= 0);
}

// setting a color to the pixels of a rectangle
void draw_rectangle(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t color) {
    draw_h_line(x, y, y + dy, color);
    draw_h_line(x + dx, y, y + dy, color);
    draw_v_line(y, x, x + dx, color);
    draw_v_line(y + dy, x, x + dx, color);
}

void initialize_grid() {
    // Setting background color
    set_background_color(CYAN);

    // Drawing grid
    draw_h_line(XBR +     DIM +     SKP, YBR, 3 * DIM + 4 * SKP + YBR, WHITE);
    draw_h_line(XBR + 2 * DIM + 3 * SKP, YBR, 3 * DIM + 4 * SKP + YBR, WHITE);

    draw_v_line(YBR +     DIM +     SKP, XBR, 3 * DIM + 4 * SKP + XBR, WHITE);
    draw_v_line(YBR + 2 * DIM + 3 * SKP, XBR, 3 * DIM + 4 * SKP + XBR, WHITE);

    // Drawing back button
    draw_rectangle(SKP, SKP, BBSX, BBSY, WHITE);
    print_string(SKP + 8, SKP + 5, 3, WHITE, CYAN, "BACK\0"); // Text width = 60, Text height = 24
}

void initialize_menu() {
    set_background_color(CYAN);

    // 2 player button
    draw_rectangle(BBR, BBR, BDX, 2 * BDY + BBR, WHITE);
    print_string(BBR + 18, BBR + 37, 3, WHITE, CYAN, "TWO PLAYERS\0"); // Text width = 165, Text height = 24

    // AI 2nd buton, down right
    draw_rectangle(BDX + 2 * BBR, BDY + 2 * BBR, BDX, BDY, WHITE);
    print_string(BDX + 2 * BBR + 18, BDY + 2 * BBR + 5, 3, WHITE, CYAN, "AI 2ND\0"); // Text width = 90, Text height = 24
    // AI 1st button, down left
    draw_rectangle(BDX + 2 * BBR, BBR, BDX, BDY, WHITE);
    print_string(BDX + 2 * BBR + 18, BBR + 5, 3, WHITE, CYAN, "AI 1ST\0"); // Text width = 90, Text height = 24
}

// check if the screen is being touched
uint8_t check_touch(uint16_t TP_X, uint16_t TP_Y, uint16_t x, uint16_t y, uint16_t dx, uint16_t dy) {
    return TP_Y >= y && TP_Y <= y + dy && TP_X >= x && TP_X <= x + dx;
}

// chek if the e is over
uint8_t game_over(uint8_t board[3][3]) {
    for (uint8_t i = 0; i < 3; i++) {
        if (board[i][0] != EMPTY && board[i][0] == board[i][1] && board[i][1] == board[i][2]) {
            return board[i][0]; // 3 same in a row
        }
        if (board[0][i] != EMPTY && board[0][i] == board[1][i] && board[1][i] == board[2][i]) {
            return board[0][i]; // 3 same in a column
        }
    }

    if (board[1][1] != EMPTY && ((board[0][0] == board[1][1] && board[1][1] == board[2][2]) || (board[0][2] == board[1][1] && board[1][1] == board[2][0]))) {
        return board[1][1]; // 3 same in a diagonal
    }

    return 0;
}

int8_t minimax(uint8_t board[3][3], uint8_t depth, uint8_t is_AI, uint8_t ai_player) {
    int8_t score = 0, best_score = 0;

    // if it is the humans turn, check if game over before playing humans move
    // if game is over, return 1 -> ai won
    if (game_over(board)) {
        return is_AI ? SCORE_LOSS : SCORE_WIN;
    }

    // All 9 fields on the board have been filled and nobody has lost, wich means
    // that this is a draw -> return 0
    if (depth >= 9) {
        return SCORE_DRAW;
    }

    // worst score outcome is initial best_score
    best_score = is_AI ? SCORE_LOSS : SCORE_WIN;
    for (uint8_t i = 0; i < 3; i++) {
        for (uint8_t j = 0; j < 3; j++) {
            if (board[i][j] == EMPTY) {
                board[i][j] = is_AI ? ai_player : (ai_player == CROSS ? NOUGHT : CROSS);
                score = minimax(board, depth + 1, is_AI ? HUMAN : AI, ai_player);

                // AI looking for MAX score of moves which in turn means that
                // AI is picking from the MIN of the oponents moves
                if ((is_AI && (score > best_score)) || (!is_AI && (score < best_score))) {
                    best_score = score;
                }

                board[i][j] = EMPTY;
            }
        }
    }
    return best_score;
}

/**
 * Runs minimax for each step, roughly generates this:
 * ※ ※ 1
 * ※ ※ 0
 * -1 -1 -1
 * Best: 1 at [0,2]
 * Returns the best step to use :)
 */
uint8_t best_move(uint8_t board[3][3], uint8_t move_counter, uint8_t ai_player) {
    uint8_t x = 0, y = 0;
    int8_t score = SCORE_LOSS, best_score = SCORE_LOSS; // Set both to worst case which is loss

    // First iteration of minimax
    for (uint8_t i = 0; i < 3; i++) {
        for (uint8_t j = 0; j < 3; j++) {
            if (board[i][j] == EMPTY) {
                board[i][j] = ai_player;
                score = minimax(board, move_counter + 1, HUMAN, ai_player);
                board[i][j] = EMPTY;

                // Looking for MAX
                if (score > best_score) {
                    best_score = score;
                    x = i;
                    y = j;
                }
            }
        }
    }

    return x * 3 + y;
}

// draws characters 'X' or 'O' on touched field
uint8_t draw_on_grid(uint8_t board[3][3], uint8_t i, uint8_t j, uint8_t mark) {
        board[i][j] = mark;

        uint8_t x = XBR + 2 * i * SKP + i * DIM;
        uint16_t y = YBR + 2 * j * SKP + j * DIM;
        if (mark == NOUGHT) {
            draw_circle(x + SKP, y + SKP, RAD - SKP, GREEN);
            return CROSS;
        } else {
            draw_cross(x + SKP, y + SKP, DIM - 2 * SKP, RED);
            return NOUGHT;
        }
}

void main() {
    TFT_init();

    initialize_menu();

    TFT_start();

    uint8_t move_counter;           // number of moves
    uint8_t player;
    uint8_t board[3][3];            // grid
    uint16_t TP_X;                  // received coordiates rom tuch part of screen
    uint16_t TP_Y;                  // received coordiates rom tuch part of screen
    uint8_t flagGameInProgress = 0; // main menu or game
    uint8_t flagGameDone = 0;       // game is not finished
    uint8_t flagAIPlayer = 0;

    while (1) {
        if (flagGameInProgress && !flagGameDone) {
            flagGameDone = game_over(board);
            if (move_counter >= 9 || flagGameDone) {
                if (!flagGameDone) {
                    print_string(MAX_X - SKP - BBSY - 32, SKP + 5, 3, WHITE, CYAN, "DRAW\0"); // Text width = 60, Text height = 24
                } else {
                    print_string(MAX_X - SKP - BBSY - 32, SKP + 5, 3, WHITE, CYAN, "WINS\0"); // Text width = 60, Text height = 24
                }

                if (!flagGameDone || flagGameDone == NOUGHT) {
                    draw_circle(MAX_X - BBSY, 2 * SKP, BBSY / 2 - SKP, GREEN);
                }

                if (!flagGameDone || flagGameDone == CROSS) {
                    draw_cross(MAX_X - BBSY, 2 * SKP, BBSY - 2 * SKP, RED);
                }

                draw_rectangle(MAX_X - SKP - BBSY, SKP, BBSY, BBSY, WHITE);
                flagGameDone = 1;
            } else if (flagAIPlayer == player) {
                uint8_t n;
                if (!move_counter) {
                    n = 0;
                } else {
                    n = best_move(board, move_counter, flagAIPlayer);
                }
                player = draw_on_grid(board, n / 3, n % 3, player);
                move_counter++;
            }

            if (!flagGameDone) {
                if (flagAIPlayer == player) {
                    // Print AI
                    print_string(SKP + BBSX + 8, SKP + 5, 3, WHITE, CYAN, "AI\0"); // Text width = 30, Text height = 24
                } else if (player == CROSS) {
                    // Print P1
                    print_string(SKP + BBSX + 8, SKP + 5, 3, WHITE, CYAN, "P1\0"); // Text width = 30, Text height = 24
                } else if (player == NOUGHT) {
                    // Print P2
                    print_string(SKP + BBSX + 8, SKP + 5, 3, WHITE, CYAN, "P2\0"); // Text width = 30, Text height = 24
                }
            }

            if (flagGameDone && move_counter >= 9) {
                print_string(SKP + BBSX + 8, SKP + 5, 3, WHITE, CYAN, "  \0"); // Text width = 30, Text height = 24
            }
        }


        // if screen is touched
        if (get_bit(PIND, T_IRQ) == 0) {
            read_touch_coords(&TP_X, &TP_Y); //citaj koordinate x,y

            if (!flagGameInProgress) {
                // Menu check
                if (check_touch(TP_X, TP_Y, BDX + 2 * BBR, BBR, BDX, BDY)) {
                    // Button - down left, AI first
                    flagGameInProgress = 1;
                    flagAIPlayer = CROSS;
                } else if (check_touch(TP_X, TP_Y, BDX + 2 * BBR, BDY + 2 * BBR, BDX, BDY)) {
                    // Button - down right, AI second
                    flagGameInProgress = 1;
                    flagAIPlayer = NOUGHT;
                } else if (check_touch(TP_X, TP_Y, BBR, BBR, BDX, 2 * BDY + BBR)) {
                    // Upper button
                    flagGameInProgress = 1;
                    flagAIPlayer = 0;
                }

                if (flagGameInProgress) {
                    // Reseting game
                    initialize_grid();
                    move_counter = 0;
                    player = CROSS;
                    flagGameDone = 0;

                    for (uint8_t i = 0; i < 3; i++) {
                        for (uint8_t j = 0; j < 3; j++) {
                            board[i][j] = 0;
                        }
                    }
                }
            } else {
                // Detecting touch on back button
                if (check_touch(TP_X, TP_Y, SKP, SKP, BBSX, BBSY)) {
                    initialize_menu();
                    flagGameInProgress = 0;
                    continue;
                }

                // Victory check
                if (flagGameDone) {
                    if (check_touch(TP_X, TP_Y, MAX_X - SKP - BBSY, SKP, BBSY, BBSY)) {
                        // Reseting game
                        initialize_grid();
                        move_counter = 0;
                        player = CROSS;
                        flagGameDone = 0;

                        for (uint8_t i = 0; i < 3; i++) {
                            for (uint8_t j = 0; j < 3; j++) {
                                board[i][j] = 0;
                            }
                        }
                    }
                    continue;
                }

                // Detecting touch on grid
                for (uint8_t i = 0; i < 3; i++) {
                    uint8_t x = XBR + 2 * i * SKP + i * DIM;
                    for (uint8_t j = 0; j < 3; j++) {
                        uint16_t y = YBR + 2 * j * SKP + j * DIM;

                        if (check_touch(TP_X, TP_Y, x, y, DIM, DIM)) {
                            if (board[i][j] == EMPTY) {
                                player = draw_on_grid(board, i, j, player);
                                move_counter++;
                            }
                        }
                    }
                }
            }
        }
    }
}
