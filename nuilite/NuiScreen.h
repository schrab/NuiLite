#pragma once

#include <cairo.h>
#include <string>
#include <memory>

namespace NuiLite {

enum class ScreenDriverType {
    SSD1325,
    SSD1322,
    NOP
};

class NuiScreen {
public:
    virtual ~NuiScreen() = default;
    virtual bool init(int dcPin, int resetPin) = 0;
    virtual void deinit() = 0;
    virtual void update(cairo_surface_t *surface) = 0;
    virtual const char *name() const = 0;
};

std::unique_ptr<NuiScreen> createScreenDriver(ScreenDriverType type);
ScreenDriverType screenDriverTypeFromString(const std::string &name);

} // namespace NuiLite
