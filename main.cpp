#include <ws2811.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <thread>
#include <signal.h>
#include <cstring>

#include "sync.h"
#include "stb_image.h"

/* --- Colors --- */
uint32_t goldColor = 0x00D8AF1F;
uint32_t aheadColor = 0x0000CC36;
uint32_t behindColor = 0x00CC1200;
uint32_t PBColor = 0x0016A6FF;
uint32_t notRunningColor = 0x00ACACAC;
uint32_t textColor = 0x00FFFFFF;
uint32_t whiteColor = 0x00FFFFFF;

#define BLACK_COLOR 0x00000000


/* --- Bitmaps --- */
std::map<char, std::string> bitmapFontFiles = {
    {'!', "!.png"},
    {'\"', "doublequote.png"},
    {'#', "#.png"},
    {'$', "$.png"},
    {'%', "%.png"},
    {'&', "&.png"},
    {'\'', "'.png"},
    {'(', "(.png"},
    {')', ").png"},
    {'*', "asterisk.png"},
    {'+', "+.png"},
    {',', ",.png"},
    {'-', "-.png"},
    {'.', "dot.png"},
    {'/', "slash.png"},
    {'0', "0.png"},
    {'1', "1.png"},
    {'2', "2.png"},
    {'3', "3.png"},
    {'4', "4.png"},
    {'5', "5.png"},
    {'6', "6.png"},
    {'7', "7.png"},
    {'8', "8.png"},
    {'9', "9.png"},
    {':', "colon.png"},
    {';', ";.png"},
    {'<', "lessthan.png"},
    {'=', "=.png"},
    {'>', "greaterthan.png"},
    {'?', "questionmark.png"},
    {'@', "@.png"},
    {'A', "A.png"},
    {'B', "B.png"},
    {'C', "C.png"},
    {'D', "D.png"},
    {'E', "E.png"},
    {'F', "F.png"},
    {'G', "G.png"},
    {'H', "H.png"},
    {'I', "I.png"},
    {'J', "J.png"},
    {'K', "K.png"},
    {'L', "L.png"},
    {'M', "M.png"},
    {'N', "N.png"},
    {'O', "O.png"},
    {'P', "P.png"},
    {'Q', "Q.png"},
    {'R', "R.png"},
    {'S', "S.png"},
    {'T', "T.png"},
    {'U', "U.png"},
    {'V', "V.png"},
    {'W', "W.png"},
    {'X', "X.png"},
    {'Y', "Y.png"},
    {'Z', "Z.png"},
    {'[', "[.png"},
    {'\\', "backslash.png"},
    {']', "].png"},
    {'*', "^.png"},
    {'_', "_.png"},
    {'`', "`.png"},
    {'a', "alower.png"},
    {'b', "blower.png"},
    {'c', "clower.png"},
    {'d', "dlower.png"},
    {'e', "elower.png"},
    {'f', "flower.png"},
    {'g', "glower.png"},
    {'h', "hlower.png"},
    {'i', "ilower.png"},
    {'j', "jlower.png"},
    {'k', "klower.png"},
    {'l', "llower.png"},
    {'m', "mlower.png"},
    {'n', "nlower.png"},
    {'o', "olower.png"},
    {'p', "plower.png"},
    {'q', "qlower.png"},
    {'r', "rlower.png"},
    {'s', "slower.png"},
    {'t', "tlower.png"},
    {'u', "ulower.png"},
    {'v', "vlower.png"},
    {'w', "wlower.png"},
    {'x', "xlower.png"},
    {'y', "ylower.png"},
    {'z', "zlower.png"},
    {'{', "{.png"},
    {'|', "pipe.png"},
    {'}', "}.png"},
    {'~', "~.png"}
};

struct Bitmap {
    int width;
    int height;
    int channels;
    unsigned char *data;

    unsigned char get(int x, int y) const {
        return data[(y * width) + x];
    }
};

std::map<char, Bitmap> bitmapFont;
std::vector<Bitmap> fireworkFrames;
const int fireworkLoopLength = 300;
const int fireworkLoopPoint = 150;




std::string formatTime(int64_t ms, bool displayTenths) {
    int sign = 1;
    if (ms < 0)
        sign = -1;
    int64_t unsignedMs = sign * ms;
    bool hasHrs = (unsignedMs >= 1000 * 60 * 60);
    bool hasM = (unsignedMs >= 1000 * 60);
    int tenths = (unsignedMs / 100) % 10;
    int s = (unsignedMs / 1000) % 60;
    int m = (unsignedMs / (1000 * 60)) % 60;
    int hrs = (unsignedMs / (1000 * 60 * 60));
    char str[64];
    if (!hasHrs && displayTenths) {
        if (!hasM)
            sprintf(str, "%d.%01d", s, tenths);
        else
            sprintf(str, "%d:%02d.%01d", m, s, tenths);
    } else {
        if (!hasHrs && !hasM)
            sprintf(str, "%d", s);
        else if (!hasHrs)
            sprintf(str, "%d:%02d", m, s);
        else
            sprintf(str, "%d:%02d:%02d", hrs, m, s);
    }
    std::string result = std::string(str);
    if (sign == 1)
        return result;
    else
        return "-" + result;
}

std::string formatDelta(int64_t ms, bool displayTenths) {
    std::string time = formatTime(ms, displayTenths);
    if (ms > 0)
        return "+" + time;
    else
        return time;
}




/* --- LEDs --- */
ws2811_t leds = {
    .render_wait_time = 0,
    .freq = 800000,
    .dmanum = 10,
    .channel = {
        [0] = {
            .gpionum = 18,
            .invert = 0,
            .count = 512,
            .strip_type = WS2812_STRIP,
            .brightness = 15
        },
        [1] = {
            .gpionum = 0,
            .invert = 0,
            .count = 0,
            .brightness = 0
        }
    }
};

void handleSigterm(int sig) {
    std::lock_guard<std::mutex> guard(endMutex);
    end = true;
}

void clear() {
    for (int i = 0; i < 512; ++i) {
        leds.channel[0].leds[i] = 0;
    }
}

int map(int x, int y) {
    if (x < 16) {
        if (y % 2 == 0)
            return (15 - x) + (16 * y);
        else
            return x + (16 * y);
    } else {
        if (y % 2 == 0)
            return 256 + (31 - x) + (16 * (15 - y));
        else
            return 256 + (x - 16) + (16 * (15 - y));
    }
}

void set(int x, int y, uint32_t color) {
    if (0 <= x && x < 32 && 0 <= y && y < 16)
        leds.channel[0].leds[map(x, y)] = color;
}

int writeChar(char c, int x, int y, uint32_t color) {
    const Bitmap &bitmap = bitmapFont[c];
    if (x >= 32 || x + bitmap.width <= 0)
        return bitmap.width;
    for (int i = 0; i < bitmap.width; ++i) {
        for (int j = 0; j < bitmap.height; ++j) {
            if (bitmap.get(i, j) != 0)
                set(x + i, y + j, color);
        }
    }
    return bitmap.width;
}

int writeChar(char c, int x, int y, uint32_t (* color)(int, int)) {
    const Bitmap &bitmap = bitmapFont[c];
    if (x >= 32 || x + bitmap.width <= 0)
        return bitmap.width;
    for (int i = 0; i < bitmap.width; ++i) {
        for (int j = 0; j < bitmap.height; ++j) {
            if (bitmap.get(i, j) != 0)
                set(x + i, y + j, color(x + i, y + j));
        }
    }
    return bitmap.width;
}

void drawFireworkFrame(int frame) {
    const Bitmap &bitmap = fireworkFrames[frame];
    for (int i = 0; i < 32; ++i) {
        for (int j = 0; j < 16; ++j) {
            unsigned char r = bitmap.data[((j * 32) + i) * 3];
            unsigned char g = bitmap.data[(((j * 32) + i) * 3) + 1];
            unsigned char b = bitmap.data[(((j * 32) + i) * 3) + 2];
            set(i, j, (r << 16) + (g << 8) + b);   
        }
    }
}

void writeLine(std::string text, int x, int y, uint32_t color) {
    int offset = x;
    for (const char &c : text)
        offset += writeChar(c, offset, y, color) + 1; 
}

void writeLine(std::string text, int x, int y, uint32_t (* color)(int, int)) {
    int offset = x;
    for (const char &c : text)
        offset += writeChar(c, offset, y, color) + 1; 
}

int lineWidth(std::string text) {
    int lineWidth = -1;
    for (const char &c : text) {
        int width = bitmapFont[c].width;
        lineWidth += width + 1;
    }
    return lineWidth;
}

void writeLineAlignRight(std::string text, int x, int y, uint32_t color) {
    writeLine(text, x - lineWidth(text), y, color);
}

void writeLineAlignRight(std::string text, int x, int y, uint32_t (* color)(int, int)) {
    writeLine(text, x - lineWidth(text), y, color);
}

void border(uint32_t color) {
    for (int i = 0; i < 32; ++i) {
        set(i, 0, color);
        set(i, 15, color);
        if (i > 0 && i < 31) {
            set(i, 1, 0);
            set(i, 14, 0);
        }
    }
    for (int i = 1; i < 15; ++i) {
        set(0, i, color);
        set(31, i, color);
        if (i > 1 && i < 14) {
            set(1, i, 0);
            set(30, i, 0);
        }
    }
}

void border(uint32_t (* color)(int, int)) {
    for (int i = 0; i < 32; ++i) {
        set(i, 0, color(i, 0));
        set(i, 15, color(i, 15));
        if (i > 0 && i < 31) {
            set(i, 1, 0);
            set(i, 14, 0);
        }
    }
    for (int i = 1; i < 15; ++i) {
        set(0, i, color(0, i));
        set(31, i, color(31, i));
        if (i > 1 && i < 14) {
            set(1, i, 0);
            set(30, i, 0);
        }
    }
}



int goldAnimFrame = 0;
uint32_t colorGold(int x, int y) {
    if ((x + y + goldAnimFrame + 3) % 150 < 3)
        return whiteColor;
    else
        return goldColor;
}

uint32_t colorAhead(int x, int y) {
    return aheadColor;
}

uint32_t colorBehind(int x, int y) {
    return behindColor;
}

uint32_t colorPB(int x, int y) {
    return PBColor;
}

uint32_t colorNotRunning(int x, int y) {
    return notRunningColor;
}

uint32_t colorBlack(int x, int y) {
    return BLACK_COLOR;
}

uint32_t colorBlend(uint32_t c1, uint32_t c2, float t) {
    if (t < 0)
        return c1;
    if (t > 1)
        return c2;
    float r1 = (float)((c1 >> 16) & 0xFF);
    float r2 = (float)((c2 >> 16) & 0xFF);
    float g1 = (float)((c1 >> 8) & 0xFF);
    float g2 = (float)((c2 >> 8) & 0xFF);
    float b1 = (float)(c1 & 0xFF);
    float b2 = (float)(c2 & 0xFF);
    float r = ((1 - t) * r1) + (t * r2);
    float g = ((1 - t) * g1) + (t * g2);
    float b = ((1 - t) * b1) + (t * b2);
    return ((uint32_t)r << 16) + ((uint32_t)g << 8) + (uint32_t)b; 
}

uint32_t (* lastBaseColor)(int, int) = colorNotRunning;
uint32_t (* lastBorderColor)(int, int) = colorNotRunning;
uint32_t (* baseColor)(int, int) = colorNotRunning;
uint32_t (* borderColor)(int, int) = colorNotRunning;
float lastMsgBrightness = 1.0f;
float msgBrightness = 1.0f;
const int transitionLength = 12;
int transitionFrame = transitionLength;
void startTransition(uint32_t (* base)(int, int), uint32_t (* border)(int, int), float msg) {
    if (baseColor != base || borderColor != border || msgBrightness != msg) {
        lastBaseColor = baseColor;
        lastBorderColor = borderColor;
        baseColor = base;
        borderColor = border;
        lastMsgBrightness = msgBrightness;
        msgBrightness = msg;
        transitionFrame = 0;
    }
}

uint32_t activeBaseColor(int x, int y) {
    if (transitionFrame < transitionLength)
        return colorBlend(lastBaseColor(x, y), baseColor(x, y), (float)transitionFrame / (float)transitionLength);
    else
        return baseColor(x, y);
}

uint32_t activeBorderColor(int x, int y) {
    if (transitionFrame < transitionLength)
        return colorBlend(lastBorderColor(x, y), borderColor(x, y), (float)transitionFrame / (float)transitionLength);
    else
        return borderColor(x, y);
}

float activeMsgBrightness() {
    float t = (float)transitionFrame / (float)transitionLength;
    return ((1.0f - t) * lastMsgBrightness) + (t * msgBrightness);
}

std::string scrollingMessage = "";
int scrollSpeed = 5;
int scrollOffset = 0;

int msgIndex = 0;
int msgFrame = 0;
std::string msg1;
std::string msg2;
uint32_t msg2Color;
void updateScrollingMessage() {
    msgIndex++;
    if (msgIndex == 4)
        msgIndex = 0;
    if (msgIndex == 0) {
        // SOB
        int64_t sobMs;
        {
            std::lock_guard<std::mutex> guard(sobMutex);
            sobMs = sob;
        }
        scrollingMessage = "SOB..." + formatTime(sobMs, true);
        msg1 = "SOB...";
        msg2 = formatTime(sobMs, true);
        msg2Color = textColor;
    } else if (msgIndex == 1) {
        // BPT
        int64_t bptMs;
        {
            std::lock_guard<std::mutex> guard(bptMutex);
            bptMs = bpt;
        }
        scrollingMessage = "BPT..." + formatTime(bptMs, true);
        msg1 = "BPT...";
        msg2 = formatTime(bptMs, true);
        msg2Color = textColor;
    } else if (msgIndex == 2) {
        // PB Delta
        bool _hasPBDelta;
        int64_t _PBDelta;
        {
            std::lock_guard<std::mutex> guard(deltaMutex);
            _hasPBDelta = hasDelta;
            _PBDelta = delta;
        }
        std::string deltaStr;
        if (!_hasPBDelta)
            deltaStr = "-";
        else
            deltaStr = formatDelta(_PBDelta, true);
        scrollingMessage = "VS PB..." + deltaStr;
        msg1 = "VS PB...";
        msg2 = deltaStr;
        if (!_hasPBDelta) {
            msg2Color = textColor;
        } else {
            if (_PBDelta > 0)
                msg2Color = behindColor;
            else
                msg2Color = aheadColor;
        }
    } else if (msgIndex == 3) {
        // BPE Delta
        bool _hasBPEDelta;
        int64_t _BPEDelta;
        {
            std::lock_guard<std::mutex> guard(bpeDeltaMutex);
            _hasBPEDelta = hasBpeDelta;
            _BPEDelta = bpeDelta;
        }
        std::string deltaStr;
        if (!_hasBPEDelta)
            deltaStr = "-";
        else
            deltaStr = formatDelta(_BPEDelta, true);
        scrollingMessage = "VS BPE..." + deltaStr;
        msg1 = "VS BPE...";
        msg2 = deltaStr;
        if (!_hasBPEDelta) {
            msg2Color = textColor;
        } else {
            if (_BPEDelta > 0)
                msg2Color = behindColor;
            else
                msg2Color = aheadColor;
        }
    }
}

void drawScrollingMessage(float brightness) {
    writeLine(msg1, 32 - scrollOffset, 9, colorBlend(BLACK_COLOR, textColor, brightness));
    writeLine(msg2, 32 - scrollOffset + lineWidth(msg1) + 1, 9, colorBlend(BLACK_COLOR, msg2Color, brightness));

    /* Update scrolling text */
    ++msgFrame;
    if (msgFrame == scrollSpeed) {
        msgFrame = 0;
        scrollOffset += 1;
        if (scrollOffset > lineWidth(scrollingMessage) + 40) {
            scrollOffset = 0;
            updateScrollingMessage();
        }
    }
}

bool isPb = false;

int main(int argc, char **argv) {
    if (argc < 3)
        return 1;

    /* TODO: Handle result. */
    setHostAddr(argv[1], argv[2]);


    /* Load the bitmap font */
    for (auto const &[c, filename] : bitmapFontFiles) {
        Bitmap bitmap;
        bitmap.data = stbi_load(("/home/pi/timer-display/font/" + filename).c_str(), &bitmap.width, &bitmap.height, &bitmap.channels, 0);
        bitmapFont[c] = bitmap;
    }

    /* Load the firework video */
    for (int i = 0; i < fireworkLoopLength; ++i) {
        Bitmap bitmap;
        bitmap.data = stbi_load(("/home/pi/timer-display/fireworks/" + std::to_string(i) + ".png").c_str(), &bitmap.width, &bitmap.height, &bitmap.channels, 0);
        fireworkFrames.push_back(bitmap);
    }

    /* Setup SIGTERM and SIGINT handler */
    struct sigaction termAction;
    memset(&termAction, 0, sizeof(termAction));
    termAction.sa_handler = handleSigterm;
    sigaction(SIGTERM, &termAction, NULL);
    sigaction(SIGINT, &termAction, NULL);

    /* Syncing threads */
    std::thread timerValueThread(timerValueSyncTask);
    std::thread timerPhaseThread(timerPhaseSyncTask);
    std::thread deltaThread(deltaSyncTask);
    std::thread pbSplitTimeThread(pbSplitTimeSyncTask);
    std::thread sobThread(sobSyncTask);
    std::thread bptThread(bptSyncTask);
    std::thread bestDeltaThread(bestDeltaSyncTask);
    std::thread bpeDeltaThread(bpeDeltaSyncTask);

    updateScrollingMessage();

    ws2811_return_t result = ws2811_init(&leds);
    if (result != WS2811_SUCCESS) {
        printf("Error: %d\n", result);
        return 1;
    }

    int frame = 0;
    while (true) {
        {
            std::lock_guard<std::mutex> guard(endMutex);
            if (end)
                break;
        }

        int64_t ms = 0;
        std::chrono::time_point<std::chrono::system_clock> updated;
        {
            std::lock_guard<std::mutex> guard(timerValueMutex);
            ms = timerValue;
            updated = timerValueLastUpdate;
        }

        TimerPhase phase;
        {
            std::lock_guard<std::mutex> guard(timerPhaseMutex);
            phase = timerPhase;
        }
        auto t = std::chrono::system_clock::now();
        if (phase == TimerPhase::Running) {
            int diff = std::chrono::duration_cast<std::chrono::milliseconds>(t - updated).count();
            ms += diff;
        }

        bool _hasDelta;
        bool _hasPbSplitTime;
        int64_t _delta;
        int64_t _pbSplitTime;

        {
            std::lock_guard<std::mutex> guard(deltaMutex);
            _hasDelta = hasDelta;
            _delta = delta;
        }
        {
            std::lock_guard<std::mutex> guard(pbSplitTimeMutex);
            _hasPbSplitTime = hasPbSplitTime;
            _pbSplitTime = pbSplitTime;
        }
        bool gold;
        {
            std::lock_guard<std::mutex> guard(bestDeltaMutex);
            gold = isGold;
        }
        
        clear();
        uint32_t (* base)(int, int) = colorNotRunning;
        if (gold) {
            base = colorGold;
        } else if ((_hasDelta && _delta > 0) || (_hasPbSplitTime && ms > _pbSplitTime)) {
            base = colorBehind;
            isPb = false;
        } else if (_hasDelta && _delta <= 0) {
            if (phase == TimerPhase::Ended) {
                base = colorPB;
            } else {
                base = colorAhead;
                isPb = false;
            }
        }
        if (phase == TimerPhase::Ended) {
            startTransition(base, colorBlack, 0.0f);
            if (!isPb && transitionFrame >= transitionLength) {
                // First PB Frame! Set up fireworks.
                frame = 0;
                isPb = true;
            }
        } else {
            startTransition(base, base, 1.0f);
            isPb = false;
        }

        if (isPb)
            drawFireworkFrame(frame);
        writeLineAlignRight(formatTime(ms, true), 30, 2, activeBaseColor);
        if (!isPb) {
            drawScrollingMessage(activeMsgBrightness());
            border(activeBorderColor);
        }
        result = ws2811_render(&leds);
        if (result != WS2811_SUCCESS)
            printf("Error: %d\n", result);

        if (gold) {
            goldAnimFrame += 1;
            if (goldAnimFrame == 150)
                goldAnimFrame = 0;
        } else {
            goldAnimFrame = 0;
        }

        frame += 1;
        if (frame == fireworkLoopLength)
            frame = fireworkLoopPoint;

        transitionFrame += 1;
        if (transitionFrame > transitionLength)
            transitionFrame = transitionLength;

        auto tim = std::chrono::system_clock::now();
        tim += std::chrono::milliseconds(33);
        std::this_thread::sleep_until(tim);
    }
    timerValueThread.join();
    usleep(200000);
    timerPhaseThread.join();
    usleep(200000);
    deltaThread.join();
    usleep(200000);
    pbSplitTimeThread.join();
    usleep(200000);
    sobThread.join();
    usleep(200000);
    bptThread.join();
    usleep(200000);
    bestDeltaThread.join();
    usleep(200000);
    bpeDeltaThread.join();
    clear();
    result = ws2811_render(&leds);
    if (result != WS2811_SUCCESS)
        printf("Error: %d\n", result);
    ws2811_fini(&leds);    
    return 0;
}

