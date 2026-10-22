#include "NuiScreenSSD1325.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <gpiod.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace NuiLite {

static constexpr const char *SPIDEV_PATH = "/dev/spidev0.0";
static constexpr const char *GPIO_CHIP = "gpiochip0";
static constexpr uint32_t PIXEL_WIDTH = 128;
static constexpr uint32_t PIXEL_HEIGHT = 64;
static constexpr uint32_t SPIDEV_BUFFER_LEN = (PIXEL_WIDTH * PIXEL_HEIGHT / 2); // 4096 bytes

NuiScreenSSD1325::NuiScreenSSD1325() = default;

NuiScreenSSD1325::~NuiScreenSSD1325() {
    deinit();
}

int NuiScreenSSD1325::openSpi(const char *path) {
    uint8_t mode = SPI_MODE_0;
    uint8_t bits_per_word = 8;
    uint8_t little_endian = 0;
    uint32_t speed_hz = 1200000000 / 64; // ~18.75 MHz

    int fd = open(path, O_RDWR | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "NuiScreenSSD1325: cannot open %s\n", path);
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits_per_word) < 0 ||
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz) < 0 ||
        ioctl(fd, SPI_IOC_WR_LSB_FIRST, &little_endian) < 0) {
        fprintf(stderr, "NuiScreenSSD1325: could not set SPI WR settings via ioctl\n");
        close(fd);
        return -1;
    }
    return fd;
}

int NuiScreenSSD1325::writeCommand(uint8_t command, uint8_t data_len, ...) {
    if (spidevFd_ <= 0) return -1;
    va_list args;
    uint8_t tx_buf[256];
    struct spi_ioc_transfer transfer = {0};

    if (gpioDc_) {
        gpiod_line_set_value(gpioDc_, 0);
    }

    tx_buf[0] = command;
    if (data_len > 0) {
        va_start(args, data_len);
        for (uint8_t i = 0; i < data_len; i++) {
            tx_buf[1 + i] = static_cast<uint8_t>(va_arg(args, int));
        }
        va_end(args);
    }

    transfer.tx_buf = reinterpret_cast<unsigned long>(tx_buf);
    transfer.len = static_cast<uint32_t>(1 + data_len);

    if (ioctl(spidevFd_, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        fprintf(stderr, "NuiScreenSSD1325: could not send command 0x%02x\n", command);
        return -1;
    }
    return 0;
}

bool NuiScreenSSD1325::init(int dcPin, int resetPin) {
    if (dcPin < 0) dcPin = 17;     // Default for Fates
    if (resetPin < 0) resetPin = 4; // Default for Fates

    fprintf(stderr, "NuiScreenSSD1325: initializing (DC GPIO %d, RST GPIO %d)\n", dcPin, resetPin);

    spidevBuffer_ = static_cast<uint8_t *>(calloc(SPIDEV_BUFFER_LEN, 1));
    if (!spidevBuffer_) {
        fprintf(stderr, "NuiScreenSSD1325: failed to allocate SPI buffer\n");
        return false;
    }

    spidevFd_ = openSpi(SPIDEV_PATH);
    if (spidevFd_ < 0) {
        fprintf(stderr, "NuiScreenSSD1325: failed to open SPI device %s\n", SPIDEV_PATH);
        free(spidevBuffer_);
        spidevBuffer_ = nullptr;
        return false;
    }

    gpioChip_ = gpiod_chip_open_by_name(GPIO_CHIP);
    if (!gpioChip_) {
        fprintf(stderr, "NuiScreenSSD1325: failed to open %s\n", GPIO_CHIP);
        close(spidevFd_);
        spidevFd_ = -1;
        free(spidevBuffer_);
        spidevBuffer_ = nullptr;
        return false;
    }

    gpioDc_ = gpiod_chip_get_line(gpioChip_, dcPin);
    gpioReset_ = gpiod_chip_get_line(gpioChip_, resetPin);

    if (gpioDc_) {
        if (gpiod_line_request_output(gpioDc_, "SSD1325_DC", 0) < 0) {
            fprintf(stderr, "WARN (screen): failed to request DC line (errno=%d: %s)\n", errno, strerror(errno));
        }
    }
    if (gpioReset_) {
        if (gpiod_line_request_output(gpioReset_, "SSD1325_RST", 1) < 0) {
            fprintf(stderr, "WARN (screen): failed to request RST line (errno=%d: %s)\n", errno, strerror(errno));
        }
    }

    // Hardware reset pulse: RST low for 10us, then high
    if (gpioReset_) {
        gpiod_line_set_value(gpioReset_, 0);
        usleep(10);
        gpiod_line_set_value(gpioReset_, 1);
        usleep(10);
    }

    // Init sequence from Fates SSD1325 driver (fbtft-coherent)
    writeCommand(0xAE, 0);       // Display OFF
    writeCommand(0xB3, 1, 0xF1); // Oscillator frequency
    writeCommand(0xA8, 1, 0x3F); // Multiplex ratio 64
    writeCommand(0xA2, 1, 0x4C); // Display offset
    writeCommand(0xA1, 1, 0x00); // Display start line
    writeCommand(0xAD, 1, 0x02); // Master config
    writeCommand(0xA0, 1, 0x56); // Remap 0x56: vertical address increment
    writeCommand(0x86, 0);       // Full current range

    // Grayscale table entries
    writeCommand(0x01, 0);
    writeCommand(0x11, 0);
    writeCommand(0x22, 0);
    writeCommand(0x32, 0);
    writeCommand(0x43, 0);
    writeCommand(0x54, 0);
    writeCommand(0x65, 0);
    writeCommand(0x76, 0);

    writeCommand(0x81, 1, 0x7F); // Contrast current
    writeCommand(0xB2, 1, 0x51); // Row period
    writeCommand(0xB1, 1, 0x55); // Phase length
    writeCommand(0xB4, 1, 0x02); // Precharge compensation level
    writeCommand(0xB0, 1, 0x28); // Enable precharge compensation
    writeCommand(0xBE, 1, 0x55); // VCOMH voltage
    writeCommand(0xBF, 1, 0x02); // Segment low voltage
    writeCommand(0xA4, 0);       // Normal display mode

    shouldTurnOn_ = true;
    return true;
}

void NuiScreenSSD1325::deinit() {
    if (spidevFd_ > 0) {
        close(spidevFd_);
        spidevFd_ = -1;
    }
    if (gpioReset_) {
        gpiod_line_set_value(gpioReset_, 1); // Keep reset HIGH (inactive) for other processes
        gpiod_line_release(gpioReset_);
        gpioReset_ = nullptr;
    }
    if (gpioDc_) {
        gpiod_line_set_value(gpioDc_, 0);
        gpiod_line_release(gpioDc_);
        gpioDc_ = nullptr;
    }
    if (gpioChip_) {
        gpiod_chip_close(gpioChip_);
        gpioChip_ = nullptr;
    }
    if (spidevBuffer_) {
        free(spidevBuffer_);
        spidevBuffer_ = nullptr;
    }
    shouldTurnOn_ = true;
}

void NuiScreenSSD1325::update(cairo_surface_t *surface) {
    if (spidevFd_ <= 0 || !spidevBuffer_ || !surface) return;

    const uint32_t w = cairo_image_surface_get_width(surface);
    const uint32_t h = cairo_image_surface_get_height(surface);
    if (w != PIXEL_WIDTH || h != PIXEL_HEIGHT) return;

    writeCommand(0x15, 2, 0x00, 0x7F); // Column address 0..127
    writeCommand(0x75, 2, 0x00, 0x3F); // Row address 0..63
    writeCommand(0x5C, 0);             // Write RAM

    if (shouldTurnOn_) {
        writeCommand(0xAF, 0); // Display ON
        shouldTurnOn_ = false;
    }

    const uint32_t *src = reinterpret_cast<const uint32_t *>(cairo_image_surface_get_data(surface));
    uint8_t *dst = spidevBuffer_;

    // SSD1325 remap 0x56 sets vertical address increment:
    // Outer loop traverses column pairs (x += 2), inner loop traverses all rows (y).
    // Each byte holds two 4-bit grayscale pixels (even column in high nibble, odd in low nibble).
    for (uint32_t x = 0; x < PIXEL_WIDTH; x += 2) {
        for (uint32_t y = 0; y < PIXEL_HEIGHT; y++) {
            const uint32_t idx = (y * PIXEL_WIDTH + x);
            const uint8_t *pixel1 = reinterpret_cast<const uint8_t *>(src + idx);
            const uint8_t *pixel2 = reinterpret_cast<const uint8_t *>(src + idx + 1);

            uint8_t g1 = pixel1[1]; // Green channel for grayscale
            uint8_t g2 = pixel2[1];

            uint8_t g4_1 = (g1 >> 4) & 0x0F;
            uint8_t g4_2 = (g2 >> 4) & 0x0F;

            *dst++ = (g4_1 << 4) | g4_2;
        }
    }

    if (gpioDc_) {
        gpiod_line_set_value(gpioDc_, 1);
    }

    struct spi_ioc_transfer transfer = {0};
    transfer.tx_buf = reinterpret_cast<unsigned long>(spidevBuffer_);
    transfer.len = SPIDEV_BUFFER_LEN;

    if (ioctl(spidevFd_, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        fprintf(stderr, "NuiScreenSSD1325: SPI data transfer failed\n");
    }
}

} // namespace NuiLite
