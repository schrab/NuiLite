#pragma once

#include "NuiScreen.h"
#include <cstdint>

struct gpiod_chip;
struct gpiod_line;

namespace NuiLite {

class NuiScreenSSD1325 : public NuiScreen {
public:
    NuiScreenSSD1325();
    ~NuiScreenSSD1325() override;

    bool init(int dcPin, int resetPin) override;
    void deinit() override;
    void update(cairo_surface_t *surface) override;
    const char *name() const override { return "ssd1325"; }

private:
    int openSpi(const char *path);
    int writeCommand(uint8_t command, uint8_t data_len, ...);

    int spidevFd_ = -1;
    struct gpiod_chip *gpioChip_ = nullptr;
    struct gpiod_line *gpioDc_ = nullptr;
    struct gpiod_line *gpioReset_ = nullptr;
    uint8_t *spidevBuffer_ = nullptr;
    bool shouldTurnOn_ = true;
};

} // namespace NuiLite
