#include "NuiDevice.h"
#include "NuiScreen.h"
#include <cJSON.h>

#include <memory>
#include <vector>
#include <thread>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <cairo.h>
#include <cairo-ft.h>

#include <math.h>
// gpio
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <linux/input.h>
#include <time.h>
#include <sys/poll.h>
#include <sys/stat.h>

#include <readerwriterqueue.h>


#ifndef SCREEN_FB_FMT
#define SCREEN_FB_FMT CAIRO_FORMAT_RGB16_565
#endif

#ifndef SCREEN_FMT
#define SCREEN_FMT CAIRO_FORMAT_ARGB32
#endif

#ifndef SCREEN_X
#define SCREEN_X 128
#endif

#ifndef SCREEN_Y
#define SCREEN_Y 64
#endif

#ifndef SCREEN_SCALE
#define SCREEN_SCALE 1.0
#endif


static constexpr unsigned SCREEN_LINE_SIZE = 10;
static constexpr unsigned SCREEN_CHAR_DROP = 1;


static constexpr unsigned NUM_FONTS = 67;

static cairo_font_face_t *ct[NUM_FONTS];

static float colours[16] =
    {0.0f, 0.066666666666667f, 0.13333333333333f, 0.2f, 0.26666666666667f,
     0.33333333333333f,
     0.4f, 0.46666666666667f, 0.53333333333333f, 0.6f, 0.66666666666667f,
     0.73333333333333f, 0.8f, 0.86666666666667f, 0.93333333333333f, 1.0f};

namespace NuiLite {

struct NuiEventMsg {
    enum {
        N_BUTTON,
        N_ENCODER
    } type_;
    unsigned id_;
    int value_;
};

// implementation class
class NuiDeviceImpl_ {
public:
    NuiDeviceImpl_(const char *resPath);
    NuiDeviceImpl_(const char *resPath, const char *driver, int dcPin = -1, int resetPin = -1);
    ~NuiDeviceImpl_();

    void start();
    void stop();
    unsigned process(bool);
    void addCallback(std::shared_ptr<NuiCallback>);

    bool buttonState(unsigned but);
    unsigned numEncoders();


    void displayClear();
    void displayPaint();


    void gClear(unsigned clr);
    void gSetPixel(unsigned clr, unsigned x, unsigned y);
    void gFillArea(unsigned clr, unsigned x, unsigned y, unsigned w, unsigned h);
    void gCircle(unsigned clr, unsigned x, unsigned y, unsigned r);
    void gFilledCircle(unsigned clr, unsigned x, unsigned y, unsigned r);
    void gLine(unsigned clr, unsigned x1, unsigned y1, unsigned x2, unsigned y2);
    void gRectangle(unsigned clr, unsigned x, unsigned y, unsigned w, unsigned h);
    void gInvert();
    void gText(unsigned clr, unsigned x, unsigned y, const std::string &str);
    void gWaveform(unsigned clr, const std::vector<unsigned> &wave);
    void gInvertArea(unsigned x, unsigned y, unsigned w, unsigned h);
    void gPng(unsigned x, unsigned y, const char *filename);

    void textLine(unsigned clr, unsigned line, unsigned col, const std::string &str);
    void invertLine(unsigned line);
    void clearLine(unsigned clr, unsigned line);

    // text functions 
    void clearText(unsigned clr, unsigned line);

    // public but not part of interface!
    void processGPIO();

private:
    void initGPIO();
    void deinitGPIO();
    void initDisplay();
    void deinitDisplay();
    void loadPrefsFromSidekickJson();

    std::string resPath_;
    std::thread gpioThread_;
    bool keepRunning_;

    static constexpr unsigned MAX_EVENTS = 64;
    static constexpr unsigned MAX_ENCODERS = 4;
    int keyFd_;
    int encFd_[MAX_ENCODERS];
    unsigned numEncoders_ = 4;

    std::unique_ptr<NuiScreen> screenDriver_;
    std::string driverName_;
    int dcPin_ = -1;
    int resetPin_ = -1;

    cairo_surface_t *surface_;
    cairo_t *cr_;
    std::vector<std::shared_ptr<NuiCallback>> callbacks_;
    moodycamel::ReaderWriterQueue<NuiEventMsg> eventQueue_;
    bool dirty_ = true;
};

//NuiDevice proxy
NuiDevice::NuiDevice(const char *resPath) {
    impl_ = new NuiDeviceImpl_(resPath);
}

NuiDevice::NuiDevice(const char *resPath, const char *driver, int dcPin, int resetPin) {
    impl_ = new NuiDeviceImpl_(resPath, driver, dcPin, resetPin);
}

NuiDevice::~NuiDevice() {
    if (impl_) {
        delete impl_;
        impl_ = nullptr;
    }
}

void NuiDevice::start() {
    impl_->start();
}

void NuiDevice::stop() {
    impl_->stop();
}

unsigned NuiDevice::process(bool paint) {
    return impl_->process(paint);
}

void NuiDevice::addCallback(std::shared_ptr<NuiCallback> cb) {
    impl_->addCallback(cb);

}

bool NuiDevice::buttonState(unsigned but) {
    return impl_->buttonState(but);
}

unsigned NuiDevice::numEncoders() {
    return impl_->numEncoders();
}


void NuiDevice::displayClear() {
    impl_->displayClear();
}

void NuiDevice::displayPaint() {
    impl_->displayPaint();
}


void NuiDevice::gClear(unsigned clr) {
    impl_->gClear(clr);
}


void NuiDevice::gSetPixel(unsigned clr, unsigned x, unsigned y) {
    impl_->gSetPixel(clr, x, y);
}


void NuiDevice::gFillArea(unsigned clr, unsigned x, unsigned y, unsigned w, unsigned h) {
    impl_->gFillArea(clr, x, y, w, h);
}


void NuiDevice::gCircle(unsigned clr, unsigned x, unsigned y, unsigned r) {
    impl_->gCircle(clr, x, y, r);
}


void NuiDevice::gFilledCircle(unsigned clr, unsigned x, unsigned y, unsigned r) {
    impl_->gFilledCircle(clr, x, y, r);
}


void NuiDevice::gLine(unsigned clr, unsigned x1, unsigned y1, unsigned x2, unsigned y2) {
    impl_->gLine(clr, x1, y1, x2, y2);
}


void NuiDevice::gRectangle(unsigned clr, unsigned x, unsigned y, unsigned w, unsigned h) {
    impl_->gRectangle(clr, x, y, w, h);
}


void NuiDevice::gInvert() {
    impl_->gInvert();
}

void NuiDevice::gText(unsigned clr, unsigned x, unsigned y, const std::string &str) {
    impl_->gText(clr, x, y, str);
}

void NuiDevice::gWaveform(unsigned clr, const std::vector<unsigned> &wave) {
    impl_->gWaveform(clr, wave);
}

void NuiDevice::gInvertArea(unsigned x, unsigned y, unsigned w, unsigned h) {
    impl_->gInvertArea(x, y, w, h);
}

void NuiDevice::gPng(unsigned x, unsigned y, const char *filename) {
    impl_->gPng(x, y, filename);
}

void NuiDevice::textLine(unsigned clr, unsigned line, unsigned col, const std::string &str) {
    impl_->textLine(clr, line, col, str);
}

void NuiDevice::clearLine(unsigned clr, unsigned line) {
    impl_->clearLine(clr, line);
}

void NuiDevice::invertLine(unsigned line) {
    impl_->invertLine(line);
}


// old api, for backwards compat only - depreciated! 
void NuiDevice::drawPNG(unsigned x, unsigned y, const char *filename) {
    gPng(x, y, filename);
}

void NuiDevice::clearRect(unsigned clr, unsigned x, unsigned y, unsigned w, unsigned h) {
    gFillArea(clr, x, y, w, h);
}

void NuiDevice::drawText(unsigned clr, unsigned x, unsigned y, const std::string &str) {
    gText(clr, x, y, str);
}

void NuiDevice::displayText(unsigned clr, unsigned line, unsigned col, const std::string &str) {
    textLine(clr, line, col, str);
}

void NuiDevice::invertText(unsigned line) {
    invertLine(line);
}

void NuiDevice::clearText(unsigned clr, unsigned line) {
    clearLine(clr, line);
}


// fwd decl for helper functions
extern int opengpio(const char *pathname, int flags);
void setup_local_fonts(const char *);

void NuiDeviceImpl_::loadPrefsFromSidekickJson() {
    const std::vector<std::string> searchPaths = {
        "./sidekick.json",
        std::string(getenv("HOME") ? getenv("HOME") : "") + "/sidekick/sidekick.json",
        "/home/we/sidekick/sidekick.json",
        "/usr/local/sidekick/sidekick.json"
    };

    for (const auto &path : searchPaths) {
        if (path.empty()) continue;
        FILE *f = fopen(path.c_str(), "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (len <= 0) {
            fclose(f);
            continue;
        }
        char *data = static_cast<char *>(malloc(len + 1));
        if (!data) {
            fclose(f);
            continue;
        }
        size_t readLen = fread(data, 1, len, f);
        data[readLen] = '\0';
        fclose(f);

        cJSON *json = cJSON_Parse(data);
        free(data);
        if (!json) continue;

        cJSON *driverItem = cJSON_GetObjectItem(json, "screenDriver");
        if (driverItem && driverItem->type == cJSON_String && driverItem->valuestring) {
            driverName_ = driverItem->valuestring;
        }
        cJSON *dcItem = cJSON_GetObjectItem(json, "screenDcPin");
        if (dcItem && dcItem->type == cJSON_Number && dcPin_ < 0) {
            dcPin_ = dcItem->valueint;
        }
        cJSON *rstItem = cJSON_GetObjectItem(json, "screenResetPin");
        if (rstItem && rstItem->type == cJSON_Number && resetPin_ < 0) {
            resetPin_ = rstItem->valueint;
        }

        cJSON_Delete(json);
        if (!driverName_.empty()) {
            fprintf(stderr, "NuiDevice: loaded display config from %s (driver=%s, dc=%d, rst=%d)\n",
                    path.c_str(), driverName_.c_str(), dcPin_, resetPin_);
            break;
        }
    }
}

//// start of IMPLEMENTATION

NuiDeviceImpl_::NuiDeviceImpl_(const char *resPath)
    : NuiDeviceImpl_(resPath, nullptr, -1, -1) {
}

NuiDeviceImpl_::NuiDeviceImpl_(const char *resPath, const char *driver, int dcPin, int resetPin)
    : resPath_(resPath == nullptr ? "" : resPath),
      dcPin_(dcPin),
      resetPin_(resetPin),
      eventQueue_(MAX_EVENTS) {
    if (driver && strlen(driver) > 0) {
        driverName_ = driver;
    }

    if (driverName_.empty()) {
        loadPrefsFromSidekickJson();
    }

    if (driverName_.empty()) {
#ifdef DEFAULT_SCREEN_DRIVER
        driverName_ = DEFAULT_SCREEN_DRIVER;
#else
        driverName_ = "ssd1325";
#endif
    }

    if (resPath_.empty()) {
        const char *home = getenv("HOME");
        if (home) {
            resPath_ = std::string(home) + "/norns/resources";
        } else {
            resPath_ = "/home/we/norns/resources";
        }
    }
}

NuiDeviceImpl_::~NuiDeviceImpl_() {
    ;
}

void NuiDeviceImpl_::addCallback(std::shared_ptr<NuiCallback> cb) {
    callbacks_.push_back(cb);
}

void NuiDeviceImpl_::start() {
    keepRunning_ = true;
    initGPIO();
    initDisplay();
}

void NuiDeviceImpl_::stop() {
    if(!keepRunning_) return;
    keepRunning_ = false;
    usleep(100 * 1000);

    deinitDisplay();
    deinitGPIO();
}

unsigned NuiDeviceImpl_::process(bool paint) {
    NuiEventMsg msg;
    while (eventQueue_.try_dequeue(msg)) {
        for (auto cb: callbacks_) {
            switch (msg.type_) {
                case NuiEventMsg::N_BUTTON : {
                    cb->onButton(msg.id_, msg.value_);
                    break;
                }
                case NuiEventMsg::N_ENCODER : {
                    cb->onEncoder(msg.id_, msg.value_);
                    break;
                }
                default:; // ignore
            }
        }
    }
    if (paint && dirty_) displayPaint();
    return 0;
}


bool NuiDeviceImpl_::buttonState(unsigned but) {
    char key = 0;
    ioctl(keyFd_, EVIOCGKEY(1), &key);
    return ((unsigned) key) & (1 << but + 1);
}


unsigned NuiDeviceImpl_::numEncoders() {
    return numEncoders_;
}


//// display functions

void NuiDeviceImpl_::displayPaint() {
    if (screenDriver_ && surface_) {
        cairo_surface_flush(surface_);
        screenDriver_->update(surface_);
    }
    dirty_ = false;
}

void NuiDeviceImpl_::displayClear() {
    cairo_set_operator(cr_, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_OVER);
    dirty_ = true;
}

void NuiDeviceImpl_::gClear(unsigned clr) {
    gFillArea(clr, 0, 0, SCREEN_X, SCREEN_Y);
    dirty_ = true;
}

void NuiDeviceImpl_::gSetPixel(unsigned clr, unsigned x, unsigned y) {
    cairo_set_source_rgb(cr_, colours[clr], colours[clr], colours[clr]);
    cairo_rectangle(cr_, x, y, 1, 1);
    cairo_fill(cr_);
    dirty_ = true;
}


void NuiDeviceImpl_::gFillArea(unsigned clr, unsigned x, unsigned y, unsigned w, unsigned h) {
    cairo_set_source_rgb(cr_, colours[clr], colours[clr], colours[clr]);
    cairo_rectangle(cr_, x, y, w, h);
    cairo_fill(cr_);
    dirty_ = true;
}


void NuiDeviceImpl_::gCircle(unsigned clr, unsigned x, unsigned y, unsigned r) {
    cairo_set_source_rgb(cr_, colours[clr], colours[clr], colours[clr]);
    cairo_arc(cr_, x, y, r, 0, 2 * M_PI);
    cairo_stroke(cr_);
    dirty_ = true;
}


void NuiDeviceImpl_::gFilledCircle(unsigned clr, unsigned x, unsigned y, unsigned r) {
    cairo_set_source_rgb(cr_, colours[clr], colours[clr], colours[clr]);
    cairo_arc(cr_, x, y, r, 0, 2 * M_PI);
    cairo_fill(cr_);
    dirty_ = true;
}


void NuiDeviceImpl_::gLine(unsigned clr, unsigned x1, unsigned y1, unsigned x2, unsigned y2) {
    cairo_set_source_rgb(cr_, colours[clr], colours[clr], colours[clr]);
    cairo_move_to(cr_, x1, y1);
    cairo_line_to(cr_, x2, y2);
    cairo_stroke(cr_);
    dirty_ = true;
}


void NuiDeviceImpl_::gRectangle(unsigned clr, unsigned x, unsigned y, unsigned w, unsigned h) {
    cairo_rectangle(cr_, x, y, w, h);
    cairo_stroke(cr_);
    dirty_ = true;
}


void NuiDeviceImpl_::gInvert() {
    gInvertArea(0, 0, SCREEN_X, SCREEN_Y);
}


void NuiDeviceImpl_::gWaveform(unsigned clr, const std::vector<unsigned> &wave) {
    cairo_set_source_rgb(cr_, colours[clr], colours[clr], colours[clr]);
    int x = 0;
    for (auto y : wave) {
        if (x == 0) {
            cairo_move_to(cr_, x, y);
        } else {
            cairo_line_to(cr_, x, y);
        }
        x++;
        if (x == 128) break;
    }
    cairo_stroke(cr_);
    dirty_ = true;
}

void NuiDeviceImpl_::gText(unsigned clr, unsigned x, unsigned y, const std::string &str) {
    cairo_set_source_rgb(cr_, colours[clr], colours[clr], colours[clr]);
    cairo_move_to(cr_, x, y);
    cairo_show_text(cr_, str.c_str());
    cairo_fill(cr_);
    dirty_ = true;
}


void NuiDeviceImpl_::gInvertArea(unsigned x, unsigned y, unsigned w, unsigned h) {
    cairo_set_source_rgb(cr_, 1., 1., 1.);
    cairo_set_operator(cr_, CAIRO_OPERATOR_DIFFERENCE);
    cairo_rectangle(cr_, x, y, w, h);
    cairo_fill(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_OVER);
    dirty_ = true;
}

void NuiDeviceImpl_::gPng(unsigned x, unsigned y, const char *filename) {
    int img_w, img_h;

    auto image = cairo_image_surface_create_from_png(filename);
    if (image == NULL) {
        fprintf(stderr, "failed to load: %s\n", filename);
        return;
    }
    img_w = cairo_image_surface_get_width(image);
    img_h = cairo_image_surface_get_height(image);

    cairo_set_source_surface(cr_, image, x, y);
    //cairo_paint (cr);
    cairo_rectangle(cr_, x, y, img_w, img_h);
    cairo_fill(cr_);
    cairo_set_source_surface(cr_, surface_, SCREEN_X, SCREEN_Y);
    cairo_surface_destroy(image);
    dirty_ = true;
}

void NuiDeviceImpl_::textLine(unsigned clr, unsigned line, unsigned col, const std::string &str) {
    unsigned x = col * 4;
    unsigned y = line * SCREEN_LINE_SIZE + SCREEN_LINE_SIZE;
    gText(clr, x, y, str);
}

void NuiDeviceImpl_::invertLine(unsigned line) {
    unsigned x = 0;
    unsigned y = (line * SCREEN_LINE_SIZE + SCREEN_LINE_SIZE) + SCREEN_CHAR_DROP; // letters with drop
    gInvertArea(x, y - SCREEN_LINE_SIZE, SCREEN_X, SCREEN_LINE_SIZE);
    dirty_ = true;
}

void NuiDeviceImpl_::clearLine(unsigned clr, unsigned line) {
    unsigned x = 0;
    unsigned y = (line * SCREEN_LINE_SIZE + SCREEN_LINE_SIZE) + SCREEN_CHAR_DROP; // letters with drop
    cairo_set_source_rgb(cr_, colours[clr], colours[clr], colours[clr]);
    gFillArea(clr, x, y - SCREEN_LINE_SIZE, SCREEN_X, SCREEN_LINE_SIZE);
}


//////  ENCODER AND KEY HANDLING //////////////////////////////////////

void *process_gpio_func(void *x) {
    auto pThis = static_cast<NuiDeviceImpl_ *>(x);
    pThis->processGPIO();
    return nullptr;
}


void NuiDeviceImpl_::processGPIO() {

    int encdir[NuiDeviceImpl_::MAX_ENCODERS] = {1, 1, 1, 1};
//    clock_t encnow[MAX_ENCODERS];
    clock_t encprev[MAX_ENCODERS];
    encprev[0] = encprev[1] = encprev[2] = encprev[3] = clock();
    struct input_event event;

    while (keepRunning_) {

        struct pollfd fds[1 + MAX_ENCODERS];

        //FIXME only works if order of fd doesnt change
        // i.e key/enc 1 to 3 must open
        unsigned fdcount = 0;
        if (keyFd_ > 0) {
            fds[fdcount].fd = keyFd_;
            fds[fdcount].events = POLLIN;
            fdcount++;
        }
        for (auto i = 0; i < numEncoders_; i++) {
            if (encFd_[i] > 0) {
                fds[fdcount].fd = encFd_[i];
                fds[fdcount].events = POLLIN;
                fdcount++;
            } else {
                fprintf(stderr, "enc %d not opened", i);
            }
        }

        auto result = poll(fds, fdcount, 5000);

        if (result > 0) {
            if (fds[0].revents & POLLIN) {
                auto rd = read(keyFd_, &event, sizeof(struct input_event));
                if (rd < (int) sizeof(struct input_event)) {
                    fprintf(stderr, "ERROR (key) read error\n");
                } else {
                    if (event.type) { // make sure it's not EV_SYN == 0
//                        fprintf(stderr, "button %d = %d\n", event.code, event.value);
                        NuiEventMsg msg;
                        msg.type_ = NuiEventMsg::N_BUTTON;
                        msg.id_ = event.code - 1; // make zerp based
                        msg.value_ = event.value;
                        eventQueue_.enqueue(msg);
                    }
                }
            }

            for (auto n = 0; n < numEncoders_; n++) {
                if (fds[n + 1].revents & POLLIN) {
                    auto rd = read(encFd_[n], &event, sizeof(struct input_event));
                    if (rd < (int) sizeof(struct input_event)) {
                        fprintf(stderr, "ERROR (enc) read error\n");
                    }

                    auto now = clock();
                    if (event.type) { // make sure it's not EV_SYN == 0
                        auto diff = now - encprev[n];
//                        fprintf(stderr, "encoder %d\t%d\t%lu\n", n, event.value, diff);
                        encprev[n] = now;
                        if (diff > 100) { // filter out glitches
                            if (encdir[n] != event.value && diff > 500) { // only reverse direction if there is reasonable settling time
                                encdir[n] = event.value;
                            }
                            NuiEventMsg msg;
                            msg.type_ = NuiEventMsg::N_ENCODER;
                            msg.id_ = n;
                            msg.value_ = event.value;
                            eventQueue_.enqueue(msg);
                        }
                    }
                }
            }
        }

    }
}


void NuiDeviceImpl_::initGPIO() {

    const char *enc_filenames[MAX_ENCODERS] = {"/dev/input/by-path/platform-soc:knob1-event",
                                               "/dev/input/by-path/platform-soc:knob2-event",
                                               "/dev/input/by-path/platform-soc:knob3-event",
                                               "/dev/input/by-path/platform-soc:knob4-event"};
    keyFd_ = opengpio("/dev/input/by-path/platform-keys-event", O_RDONLY);

    struct stat fs;
    numEncoders_ = 3;
    if ((stat("/dev/input/by-path/platform-soc:knob4-event", &fs) == 0)) {
        numEncoders_ = 4;
    }


    for (auto i = 0; i < numEncoders_; i++) {
        encFd_[i] = opengpio(enc_filenames[i], O_RDONLY);
    }

    gpioThread_ = std::thread(process_gpio_func, this);
}

void NuiDeviceImpl_::deinitGPIO() {
    keepRunning_ = false;
    if (gpioThread_.joinable()) gpioThread_.join();

}

////////////////////////    DISPLAY HANDLING ////////////////////////////////////////////



void NuiDeviceImpl_::initDisplay() {
    surface_ = cairo_image_surface_create(SCREEN_FMT, SCREEN_X, SCREEN_Y);
    cr_ = cairo_create(surface_);

    setup_local_fonts(resPath_.c_str());

    ScreenDriverType dtype = screenDriverTypeFromString(driverName_);
    screenDriver_ = createScreenDriver(dtype);
    if (screenDriver_) {
        if (!screenDriver_->init(dcPin_, resetPin_)) {
            fprintf(stderr, "WARN (screen): failed to initialize %s driver\n", screenDriver_->name());
        } else {
            fprintf(stderr, "NuiDevice: using screen driver %s\n", screenDriver_->name());
        }
    } else {
        fprintf(stderr, "WARN (screen): no screen driver created for %s\n", driverName_.c_str());
    }

    // clear display surface
    cairo_set_operator(cr_, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_OVER);

    cairo_font_options_t *font_options = cairo_font_options_create();
    cairo_font_options_set_antialias(font_options, CAIRO_ANTIALIAS_SUBPIXEL);
    cairo_set_font_options(cr_, font_options);
    cairo_font_options_destroy(font_options);

    if (ct[0]) {
        cairo_set_font_face(cr_, ct[0]);
    }
    cairo_set_font_size(cr_, 8.0);
    cairo_set_source_rgb(cr_, 1, 1, 1);

    dirty_ = true;
}

void NuiDeviceImpl_::deinitDisplay() {
    if (screenDriver_) {
        screenDriver_->deinit();
        screenDriver_.reset();
    }
    if (cr_) {
        cairo_destroy(cr_);
        cr_ = nullptr;
    }
    if (surface_) {
        cairo_surface_destroy(surface_);
        surface_ = nullptr;
    }
}


// Legacy framebuffer stubs for binary backward compatibility with precompiled MEC/externals
cairo_surface_t *cairo_linuxfb_surface_create(const char *fbdev) {
    (void)fbdev;
    return nullptr;
}

void cairo_linuxfb_surface_destroy(void *device) {
    (void)device;
}

///////// helper functions

int opengpio(const char *pathname, int flags) {
    int fd;
    int open_attempts = 0, ioctl_attempts = 0;
    while (open_attempts < 200) {
        fd = open(pathname, flags);
        if (fd > 0) {
            if (ioctl(fd, EVIOCGRAB, 1) == 0) {
                ioctl(fd, EVIOCGRAB, (void *) 0);
                goto done;
            }
            ioctl_attempts++;
            close(fd);
        }
        open_attempts++;
        usleep(50000); // 50ms sleep * 200 = 10s fail after 10s
    };
    done:
    if (open_attempts > 0) {
        fprintf(stderr, "WARN opengpio GPIO '%s' required %d open attempts & %d ioctl attempts\n", pathname, open_attempts, ioctl_attempts);
    }
    return fd;
}

static char font_path[NUM_FONTS][32];


void setup_local_fonts(const char *resPath) {
    static FT_Library value;
    static FT_Error status;
    static FT_Face face[NUM_FONTS];

    status = FT_Init_FreeType(&value);
    if (status != 0) {
        fprintf(stderr, "ERROR (screen) freetype init\n");
        return;
    }

    strcpy(font_path[0], "04B_03__.TTF");
    strcpy(font_path[1], "liquid.ttf");
    strcpy(font_path[2], "Roboto-Thin.ttf");
    strcpy(font_path[3], "Roboto-Light.ttf");
    strcpy(font_path[4], "Roboto-Regular.ttf");
    strcpy(font_path[5], "Roboto-Medium.ttf");
    strcpy(font_path[6], "Roboto-Bold.ttf");
    strcpy(font_path[7], "Roboto-Black.ttf");
    strcpy(font_path[8], "Roboto-ThinItalic.ttf");
    strcpy(font_path[9], "Roboto-LightItalic.ttf");
    strcpy(font_path[10], "Roboto-Italic.ttf");
    strcpy(font_path[11], "Roboto-MediumItalic.ttf");
    strcpy(font_path[12], "Roboto-BoldItalic.ttf");
    strcpy(font_path[13], "Roboto-BlackItalic.ttf");
    strcpy(font_path[14], "VeraBd.ttf");
    strcpy(font_path[15], "VeraBI.ttf");
    strcpy(font_path[16], "VeraIt.ttf");
    strcpy(font_path[17], "VeraMoBd.ttf");
    strcpy(font_path[18], "VeraMoBI.ttf");
    strcpy(font_path[19], "VeraMoIt.ttf");
    strcpy(font_path[20], "VeraMono.ttf");
    strcpy(font_path[21], "VeraSeBd.ttf");
    strcpy(font_path[22], "VeraSe.ttf");
    strcpy(font_path[23], "Vera.ttf");
    //------------------
    // bitmap fonts
    strcpy(font_path[24], "bmp/tom-thumb.bdf");
    // FIXME: this is totally silly...
    int i = 25;
    strcpy(font_path[i++], "bmp/creep.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-10b.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-10r.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-13b.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-13b-i.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-13r.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-13r-i.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-16b.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-16b-i.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-16r.bdf");
    strcpy(font_path[i++], "bmp/ctrld-fixed-16r-i.bdf");
    strcpy(font_path[i++], "bmp/scientifica-11.bdf");
    strcpy(font_path[i++], "bmp/scientificaBold-11.bdf");
    strcpy(font_path[i++], "bmp/scientificaItalic-11.bdf");
    strcpy(font_path[i++], "bmp/ter-u12b.bdf");
    strcpy(font_path[i++], "bmp/ter-u12n.bdf");
    strcpy(font_path[i++], "bmp/ter-u14b.bdf");
    strcpy(font_path[i++], "bmp/ter-u14n.bdf");
    strcpy(font_path[i++], "bmp/ter-u14v.bdf");
    strcpy(font_path[i++], "bmp/ter-u16b.bdf");
    strcpy(font_path[i++], "bmp/ter-u16n.bdf");
    strcpy(font_path[i++], "bmp/ter-u16v.bdf");
    strcpy(font_path[i++], "bmp/ter-u18b.bdf");
    strcpy(font_path[i++], "bmp/ter-u18n.bdf");
    strcpy(font_path[i++], "bmp/ter-u20b.bdf");
    strcpy(font_path[i++], "bmp/ter-u20n.bdf");
    strcpy(font_path[i++], "bmp/ter-u22b.bdf");
    strcpy(font_path[i++], "bmp/ter-u22n.bdf");
    strcpy(font_path[i++], "bmp/ter-u24b.bdf");
    strcpy(font_path[i++], "bmp/ter-u24n.bdf");
    strcpy(font_path[i++], "bmp/ter-u28b.bdf");
    strcpy(font_path[i++], "bmp/ter-u28n.bdf");
    strcpy(font_path[i++], "bmp/ter-u32b.bdf");
    strcpy(font_path[i++], "bmp/ter-u32n.bdf");
    strcpy(font_path[i++], "bmp/unscii-16-full.pcf");
    strcpy(font_path[i++], "bmp/unscii-16.pcf");
    strcpy(font_path[i++], "bmp/unscii-8-alt.pcf");
    strcpy(font_path[i++], "bmp/unscii-8-fantasy.pcf");
    strcpy(font_path[i++], "bmp/unscii-8-mcr.pcf");
    strcpy(font_path[i++], "bmp/unscii-8.pcf");
    strcpy(font_path[i++], "bmp/unscii-8-tall.pcf");
    strcpy(font_path[i++], "bmp/unscii-8-thin.pcf");

    assert(i == NUM_FONTS);

    //fprintf(stderr, "fonts: \n");
    //for (int i=0; i<NUM_FONTS; ++i) {
    // fprintf(stderr, "  %d: %s\n", i, font_path[i]);
    //}

    char filename[256];

    for (int i = 0; i < NUM_FONTS; i++) {
        // FIXME should be path relative to norns/
        snprintf(filename, 256, "%s/%s", resPath, font_path[i]);

        status = FT_New_Face(value, filename, 0, &face[i]);
        if (status != 0 && i == 0) {
            // Modern Norns renamed 04B_03__.TTF to norns.ttf
            snprintf(filename, 256, "%s/norns.ttf", resPath);
            status = FT_New_Face(value, filename, 0, &face[i]);
        }
        if (status != 0) {
            fprintf(stderr, "WARN (screen) font load failed: %s\n", filename);
            ct[i] = nullptr;
        } else {
            ct[i] = cairo_ft_font_face_create_for_ft_face(face[i], 0);
        }
    }

}


}// namespace
