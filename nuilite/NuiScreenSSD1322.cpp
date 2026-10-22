#include "NuiScreenSSD1322.h"

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
static constexpr uint32_t SPIDEV_BUFFER_LEN = (PIXEL_WIDTH * PIXEL_HEIGHT); // 8192 bytes

NuiScreenSSD1322::NuiScreenSSD1322() = default;

NuiScreenSSD1322::~NuiScreenSSD1322() {
    deinit();
}

int NuiScreenSSD1322::openSpi(const char *path) {
    uint8_t mode = SPI_MODE_0;
    uint8_t bits_per_word = 8;
    uint8_t little_endian = 0;
    uint32_t speed_hz = 1200000000 / 64; // ~18.75 MHz

    int fd = open(path, O_RDWR | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "NuiScreenSSD1322: cannot open %s\n", path);
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits_per_word) < 0 ||
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz) < 0 ||
        ioctl(fd, SPI_IOC_WR_LSB_FIRST, &little_endian) < 0) {
        fprintf(stderr, "NuiScreenSSD1322: could not set SPI WR settings via ioctl\n");
        close(fd);
        return -1;
    }
    return fd;
}

int NuiScreenSSD1322::writeCommand(uint8_t command, uint8_t data_len, ...) {
    if (spidevFd_ <= 0) return -1;
    va_list args;
    uint8_t cmd_buf[1] = {command};
    uint8_t data_buf[256];
    struct spi_ioc_transfer cmd_transfer = {0};
    struct spi_ioc_transfer data_transfer = {0};

    // Command byte sent with D/C = 0
    if (gpioDc_) {
        gpiod_line_set_value(gpioDc_, 0);
    }

    cmd_transfer.tx_buf = reinterpret_cast<unsigned long>(cmd_buf);
    cmd_transfer.len = 1;

    if (ioctl(spidevFd_, SPI_IOC_MESSAGE(1), &cmd_transfer) < 0) {
        fprintf(stderr, "NuiScreenSSD1322: could not send command 0x%02x\n", command);
        return -1;
    }

    // Parameter bytes sent with D/C = 1
    if (data_len > 0) {
        if (gpioDc_) {
            gpiod_line_set_value(gpioDc_, 1);
        }

        va_start(args, data_len);
        for (uint8_t i = 0; i < data_len; i++) {
            data_buf[i] = static_cast<uint8_t>(va_arg(args, int));
        }
        va_end(args);

        data_transfer.tx_buf = reinterpret_cast<unsigned long>(data_buf);
        data_transfer.len = data_len;

        if (ioctl(spidevFd_, SPI_IOC_MESSAGE(1), &data_transfer) < 0) {
            fprintf(stderr, "NuiScreenSSD1322: could not send parameter bytes for 0x%02x\n", command);
            return -1;
        }
    }
    return 0;
}

bool NuiScreenSSD1322::init(int dcPin, int resetPin) {
    if (dcPin < 0) dcPin = 5;      // Default for stock Norns shield
    if (resetPin < 0) resetPin = 6; // Default for stock Norns shield

    fprintf(stderr, "NuiScreenSSD1322: initializing (DC GPIO %d, RST GPIO %d)\n", dcPin, resetPin);

    spidevBuffer_ = static_cast<uint8_t *>(calloc(SPIDEV_BUFFER_LEN, 1));
    if (!spidevBuffer_) {
        fprintf(stderr, "NuiScreenSSD1322: failed to allocate SPI buffer\n");
        return false;
    }

    spidevFd_ = openSpi(SPIDEV_PATH);
    if (spidevFd_ < 0) {
        fprintf(stderr, "NuiScreenSSD1322: failed to open SPI device %s\n", SPIDEV_PATH);
        free(spidevBuffer_);
        spidevBuffer_ = nullptr;
        return false;
    }

    gpioChip_ = gpiod_chip_open_by_name(GPIO_CHIP);
    if (!gpioChip_) {
        fprintf(stderr, "NuiScreenSSD1322: failed to open %s\n", GPIO_CHIP);
        close(spidevFd_);
        spidevFd_ = -1;
        free(spidevBuffer_);
        spidevBuffer_ = nullptr;
        return false;
    }

    gpioDc_ = gpiod_chip_get_line(gpioChip_, dcPin);
    gpioReset_ = gpiod_chip_get_line(gpioChip_, resetPin);

    if (gpioDc_) {
        if (gpiod_line_request_output(gpioDc_, "SSD1322_DC", 0) < 0) {
            fprintf(stderr, "WARN (screen): failed to request DC line (errno=%d: %s)\n", errno, strerror(errno));
        }
    }
    if (gpioReset_) {
        if (gpiod_line_request_output(gpioReset_, "SSD1322_RST", 1) < 0) {
            fprintf(stderr, "WARN (screen): failed to request RST line (errno=%d: %s)\n", errno, strerror(errno));
        }
    }

    // Pull reset HIGH for normal operation
    if (gpioReset_) {
        gpiod_line_set_value(gpioReset_, 1);
        usleep(10);
    }

    // Init sequence from standard Norns SSD1322 driver
    writeCommand(0xAE, 0);       // Display OFF
    writeCommand(0xB9, 0);       // Linear grayscale
    writeCommand(0xB3, 1, 0x91); // Oscillator frequency
    writeCommand(0xCA, 1, 0x3F); // Mux ratio 64
    writeCommand(0xA2, 1, 0x00); // Display offset 0
    writeCommand(0xA1, 1, 0x00); // Display start line 0
    writeCommand(0xAB, 1, 0x01); // Enable internal VDD regulator
    writeCommand(0xB4, 2, 0xA0, 0xFD); // Display enhancement A
    writeCommand(0xC1, 1, 0x7F); // Contrast current
    writeCommand(0xC7, 1, 0x0F); // Master current control
    writeCommand(0xB1, 1, 0xF2); // Phase length
    writeCommand(0xBB, 1, 0x1F); // Precharge voltage
    writeCommand(0xBE, 1, 0x04); // VCOMH voltage
    writeCommand(0xA6, 0);       // Normal display mode
    writeCommand(0xA0, 2, 0x16, 0x11); // Dual COM line mode / remap

    shouldTurnOn_ = true;
    return true;
}

void NuiScreenSSD1322::deinit() {
    if (spidevFd_ > 0) {
        writeCommand(0xAE, 0); // Display OFF
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

void NuiScreenSSD1322::update(cairo_surface_t *surface) {
    if (spidevFd_ <= 0 || !spidevBuffer_ || !surface) return;

    const uint32_t w = cairo_image_surface_get_width(surface);
    const uint32_t h = cairo_image_surface_get_height(surface);
    if (w != PIXEL_WIDTH || h != PIXEL_HEIGHT) return;

    writeCommand(0x15, 2, 28, 91); // Column address 28..91
    writeCommand(0x75, 2, 0, 63);  // Row address 0..63
    writeCommand(0x5C, 0);         // Write RAM

    if (shouldTurnOn_) {
        writeCommand(0xAF, 0); // Display ON
        shouldTurnOn_ = false;
    }

    const uint32_t *src = reinterpret_cast<const uint32_t *>(cairo_image_surface_get_data(surface));

    // SSD1322: row-by-row mapping, 128 pixels across.
    // Each 4-bit pixel is placed in high nibble with duplicate in low nibble: (g4 << 4) | g4
    for (uint32_t i = 0; i < SPIDEV_BUFFER_LEN; i++) {
        const uint8_t *pixel = reinterpret_cast<const uint8_t *>(src + i);
        uint8_t g = pixel[1]; // Green channel
        uint8_t g4 = (g >> 4) & 0x0F;
        spidevBuffer_[i] = (g4 << 4) | g4;
    }

    if (gpioDc_) {
        gpiod_line_set_value(gpioDc_, 1);
    }

    struct spi_ioc_transfer transfer = {0};
    transfer.tx_buf = reinterpret_cast<unsigned long>(spidevBuffer_);
    transfer.len = SPIDEV_BUFFER_LEN;

    if (ioctl(spidevFd_, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        fprintf(stderr, "NuiScreenSSD1322: SPI data transfer failed\n");
    }
}

} // namespace NuiLite
