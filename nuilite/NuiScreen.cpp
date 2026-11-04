#include "NuiScreen.h"
#include "NuiScreenSSD1325.h"
#include "NuiScreenSSD1322.h"
#include <algorithm>

namespace NuiLite {

ScreenDriverType screenDriverTypeFromString(const std::string &name) {
    std::string s = name;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "ssd1322") {
        return ScreenDriverType::SSD1322;
    }
    if (s == "ssd1325") {
        return ScreenDriverType::SSD1325;
    }
    return ScreenDriverType::SSD1325;
}

std::unique_ptr<NuiScreen> createScreenDriver(ScreenDriverType type) {
    switch (type) {
        case ScreenDriverType::SSD1322:
            return std::unique_ptr<NuiScreen>(new NuiScreenSSD1322());
        case ScreenDriverType::SSD1325:
        default:
            return std::unique_ptr<NuiScreen>(new NuiScreenSSD1325());
    }
}

} // namespace NuiLite
