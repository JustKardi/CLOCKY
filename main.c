// Ultra-low-power e-paper clock
// MSP430FR2111 + PCF2131 RTC (SPI) + UC8151 e-paper 152x296 (SPI)
// Build with TI Code Composer Studio (default 1 MHz clock assumed).

#include <msp430.h>
#include <stdint.h>

// ---------------- settings ----------------
#define SET_TIME_ON_BOOT   1  // 1 = load compile time into RTC. Flash once, then set 0 and reflash.
#define UPDATE_EVERY_MIN   5  // redraw when minute % N == 0 (full refresh costs a lot of energy)
#define BUSY_ACTIVE_LEVEL  1  // level of display BUSY pin while busy (verify in display datasheet)

// ---------------- pins ----------------
// Port 1
#define RTC_INTA   BIT0   // P1.0 in  (falling edge = minute tick)
#define VBAT_OK    BIT1   // P1.1 in  (from BQ25504)
#define RTC_CE     BIT2   // P1.2 out (active low)
#define EPD_BUSY   BIT3   // P1.3 in
#define EPD_CS     BIT4   // P1.4 out (active low)
// P1.5 CLK, P1.6 MISO, P1.7 MOSI = eUSCI_A0 SPI
// Port 2
#define EPD_DC     BIT0   // P2.0 out
#define EPD_RES    BIT1   // P2.1 out

#define RTC_LOW()  (P1OUT &= ~RTC_CE)
#define RTC_HIGH() (P1OUT |=  RTC_CE)

static void delay_ms(uint16_t ms) { while (ms--) __delay_cycles(1000); }

// ---------------- SPI ----------------
static uint8_t spi_xfer(uint8_t b) {
    UCA0TXBUF = b;
    while (!(UCA0IFG & UCRXIFG));
    return UCA0RXBUF;
}

static void spi_init(void) {
    UCA0CTLW0 = UCSWRST;
    UCA0CTLW0 |= UCSSEL__SMCLK | UCSYNC | UCMST | UCMSB | UCCKPH;  // SPI mode 0
    UCA0BRW = 2;                                                  // ~500 kHz
    UCA0CTLW0 &= ~UCSWRST;
}

// ---------------- RTC (PCF2131) ----------------
// SPI command byte: 0x20|reg to write, 0xA0|reg to read (same as the Linux driver).
static void rtc_write(uint8_t reg, const uint8_t *d, uint8_t n) {
    RTC_LOW();
    spi_xfer(0x20 | reg);
    while (n--) spi_xfer(*d++);
    RTC_HIGH();
}

static void rtc_read(uint8_t reg, uint8_t *d, uint8_t n) {
    RTC_LOW();
    spi_xfer(0xA0 | reg);
    while (n--) *d++ = spi_xfer(0);
    RTC_HIGH();
}

static void rtc_write1(uint8_t reg, uint8_t v) { rtc_write(reg, &v, 1); }

#define TD(i) (__TIME__[i] - '0')

static void rtc_set_time(void) {
    uint8_t t[7];
    t[0] = (TD(6) << 4) | TD(7);   // seconds (BCD)
    t[1] = (TD(3) << 4) | TD(4);   // minutes
    t[2] = (TD(0) << 4) | TD(1);   // hours (24 h)
    t[3] = 0x01;                   // day of month (dummy)
    t[4] = 0x00;                   // weekday (dummy)
    t[5] = 0x01;                   // month (dummy)
    t[6] = 0x26;                   // year 2026 (dummy)
    rtc_write1(0x00, 0x20);        // Control_1: STOP
    rtc_write1(0x05, 0xA4);        // SR_Reset: clear prescaler
    rtc_write(0x07, t, 7);         // time registers start at 0x07
}

static void rtc_init(void) {
    rtc_write1(0x31, 0xDF);        // INT_A_MASK1: only minute interrupt (bit 5) enabled (0 = enabled)
    rtc_write1(0x32, 0xFF);        // INT_A_MASK2: everything else off
    rtc_write1(0x00, 0x02);        // Control_1: minute interrupt on, STOP cleared, 24 h
}

static void rtc_clear_flag(void) {
    uint8_t c;
    rtc_read(0x01, &c, 1);         // Control_2
    rtc_write1(0x01, c & ~0x80);   // clear MSF so INTA# releases
}

static void rtc_get(uint8_t *hh, uint8_t *mm) {
    uint8_t b[3];
    rtc_read(0x07, b, 3);          // seconds, minutes, hours
    uint8_t m = b[1] & 0x7F, h = b[2] & 0x3F;
    *mm = (m >> 4) * 10 + (m & 0x0F);
    *hh = (h >> 4) * 10 + (h & 0x0F);
}

// ---------------- e-paper (UC8151) ----------------
static void epd_cmd(uint8_t c) {
    P2OUT &= ~EPD_DC; P1OUT &= ~EPD_CS;
    spi_xfer(c);
    P1OUT |= EPD_CS;
}

static void epd_data(uint8_t d) {
    P2OUT |= EPD_DC; P1OUT &= ~EPD_CS;
    spi_xfer(d);
    P1OUT |= EPD_CS;
}

static void epd_begin_data(uint8_t cmd) { epd_cmd(cmd); P2OUT |= EPD_DC; P1OUT &= ~EPD_CS; }
static void epd_end_data(void)          { P1OUT |= EPD_CS; }

static void epd_wait_busy(void) {
    uint16_t t = 10000;  // 10 s timeout
    while (t--) {
        uint8_t busy = (P1IN & EPD_BUSY) ? 1 : 0;
        if (busy != BUSY_ACTIVE_LEVEL) return;
        delay_ms(1);
    }
}

static void epd_init(void) {
    P2OUT &= ~EPD_RES; delay_ms(10);
    P2OUT |=  EPD_RES; delay_ms(10);
    epd_cmd(0x06); epd_data(0x17); epd_data(0x17); epd_data(0x17);  // booster soft start
    epd_cmd(0x04); epd_wait_busy();                                // power on
    epd_cmd(0x00); epd_data(0x1F);                                 // panel setting (OTP LUT, B/W)
    epd_cmd(0x61); epd_data(0x98); epd_data(0x01); epd_data(0x28); // 152 x 296
    epd_cmd(0x50); epd_data(0x97);                                 // VCOM / data interval
}

// 5x7 digit font, one byte per column, bit0 = top row
static const uint8_t font[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31}, {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03}, {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E}
};

static uint8_t dig[4];  // H H M M

// Builds one 152-pixel row (19 bytes). Black = 0, white = 1.
// Layout: hours at y=40..123, minutes at y=160..243, each digit 60x84 px (scale 12).
static void build_row(uint16_t y, uint8_t *row) {
    uint8_t i, d, c, k, line, r;
    for (i = 0; i < 19; i++) row[i] = 0xFF;
    if (y >= 40 && y < 124)       { line = 0; r = (y - 40) / 12; }
    else if (y >= 160 && y < 244) { line = 1; r = (y - 160) / 12; }
    else return;
    for (d = 0; d < 2; d++) {
        const uint8_t *g = font[dig[line * 2 + d]];
        uint8_t x0 = d ? 80 : 12;
        for (c = 0; c < 5; c++) {
            if ((g[c] >> r) & 1) {
                for (k = 0; k < 12; k++) {
                    uint8_t x = x0 + c * 12 + k;
                    row[x >> 3] &= ~(0x80 >> (x & 7));
                }
            }
        }
    }
}

static void epd_show(uint8_t hh, uint8_t mm) {
    uint8_t row[19];
    uint16_t y, n;
    dig[0] = hh / 10; dig[1] = hh % 10; dig[2] = mm / 10; dig[3] = mm % 10;

    epd_init();
    epd_begin_data(0x10);                                   // old data = all white
    for (n = 0; n < 5624; n++) spi_xfer(0xFF);
    epd_end_data();
    epd_begin_data(0x13);                                   // new data = the time
    for (y = 0; y < 296; y++) {
        uint8_t i;
        build_row(y, row);
        for (i = 0; i < 19; i++) spi_xfer(row[i]);
    }
    epd_end_data();
    epd_cmd(0x12); delay_ms(10); epd_wait_busy();           // refresh (the energy-hungry part)
    epd_cmd(0x02); epd_wait_busy();                         // power off boost
    epd_cmd(0x07); epd_data(0xA5);                          // deep sleep
}

static void show_time(void) {
    uint8_t hh, mm;
    if (!(P1IN & VBAT_OK)) return;   // not enough stored energy: skip this redraw
    rtc_get(&hh, &mm);
    epd_show(hh, mm);
}

// ---------------- main ----------------
int main(void) {
    uint8_t hh, mm;

    WDTCTL = WDTPW | WDTHOLD;
    P1OUT = RTC_CE | EPD_CS;                         // both chip selects idle high
    P1DIR = RTC_CE | EPD_CS;
    P2OUT = EPD_RES;
    P2DIR = EPD_DC | EPD_RES | BIT6 | BIT7;          // BIT6/7 = unused pins, driven low
    P1SEL0 |= BIT5 | BIT6 | BIT7;                    // eUSCI_A0 SPI pins
    PM5CTL0 &= ~LOCKLPM5;                            // release GPIO

    spi_init();
    delay_ms(50);
    if (SET_TIME_ON_BOOT) rtc_set_time();
    rtc_init();

    P1IES |= RTC_INTA;                               // falling edge
    P1IFG &= ~RTC_INTA;
    P1IE  |= RTC_INTA;
    __enable_interrupt();

    show_time();

    for (;;) {
        rtc_clear_flag();                            // release INTA# before sleeping
        __bis_SR_register(LPM4_bits | GIE);          // sleep until the next minute tick
        rtc_get(&hh, &mm);
        if (mm % UPDATE_EVERY_MIN == 0) show_time();
    }
}

#pragma vector = PORT1_VECTOR
__interrupt void port1_isr(void) {
    P1IFG &= ~RTC_INTA;
    __bic_SR_register_on_exit(LPM4_bits);
}
