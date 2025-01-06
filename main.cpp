#include <ws2811.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <map>
#include <chrono>
#include <thread>
#include <signal.h>
#include <cstring>

#include "sync.h"
#include "stb_image.h"

/* --- Colors --- */
uint32_t goldColor = 0x00D8AF1F;
uint32_t aheadGainingColor = 0x0000CC36;
uint32_t aheadLosingColor = 0x0052CC73;
uint32_t behindGainingColor = 0x00CC5C52;
uint32_t behindLosingColor = 0x00CC1200;
uint32_t notRunningColor = 0x00ACACAC;
uint32_t PBColor = 0x0016A6FF;
uint32_t pausedColor = 0x007A7A7A;



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

void writeLine(std::string text, int x, int y, uint32_t color) {
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

std::string scrollingMessage = "";
int scrollSpeed = 5;
int scrollOffset = 0;

int msgIndex = 0;
void updateScrollingMessage() {
    msgIndex++;
    if (msgIndex == 2)
        msgIndex = 0;
    if (msgIndex == 0) {
        // SOB
        int64_t sobMs;
        {
            std::lock_guard<std::mutex> guard(sobMutex);
            sobMs = sob;
        }
        scrollingMessage = "SOB..." + formatTime(sobMs, true);
    } else if (msgIndex == 1) {
        // BPT
        int64_t bptMs;
        {
            std::lock_guard<std::mutex> guard(bptMutex);
            bptMs = bpt;
        }
        scrollingMessage = "BPT..." + formatTime(bptMs, true);
    }
}

int main() {
    /* Load the bitmap font */
    for (auto const &[c, filename] : bitmapFontFiles) {
        Bitmap bitmap;
        bitmap.data = stbi_load(("font/" + filename).c_str(), &bitmap.width, &bitmap.height, &bitmap.channels, 0);
        bitmapFont[c] = bitmap;
    }

    /* Setup SIGTERM and SIGINT handler */
    struct sigaction termAction;
    memset(&termAction, 0, sizeof(termAction));
    termAction.sa_handler = handleSigterm;
    sigaction(SIGTERM, &termAction, NULL);
    sigaction(SIGINT, &termAction, NULL);

    /* Start the sync thread */
    std::thread timerValueThread(timerValueSyncTask);
    std::thread timerPhaseThread(timerPhaseSyncTask);
    std::thread deltaThread(deltaSyncTask);
    std::thread pbSplitTimeThread(pbSplitTimeSyncTask);
    std::thread sobThread(sobSyncTask);
    std::thread bptThread(bptSyncTask);

    ws2811_return_t result = ws2811_init(&leds);
    if (result != WS2811_SUCCESS) {
        printf("Error: %d\n", result);
        return 1;
    }

    updateScrollingMessage();

    int i = 0;
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
        
        clear();
        uint32_t base = notRunningColor;
        if ((_hasDelta && _delta > 0) || (_hasPbSplitTime && ms > _pbSplitTime))
            base = behindLosingColor;
        else if (_hasDelta && _delta <= 0) {
            if (phase == TimerPhase::Ended)
                base = PBColor;
            else
                base = aheadGainingColor;
        }

        writeLineAlignRight(formatTime(ms, true), 30, 2, base);
        writeLine(scrollingMessage, 32 - scrollOffset, 9, 0x00FFFFFF);
        border(base);
        result = ws2811_render(&leds);
        if (result != WS2811_SUCCESS)
            printf("Error: %d\n", result);

        /* Update scrolling text */
        ++i;
        if (i == scrollSpeed) {
            i = 0;
            scrollOffset += 1;
            if (scrollOffset > lineWidth(scrollingMessage) + 40) {
                scrollOffset = 0;
                updateScrollingMessage();
            }
        }

        auto tim = std::chrono::system_clock::now();
        tim += std::chrono::milliseconds(33);
        std::this_thread::sleep_until(tim);
    }
    timerValueThread.join();
    timerPhaseThread.join();
    deltaThread.join();
    pbSplitTimeThread.join();
    sobThread.join();
    bptThread.join();
    clear();
    result = ws2811_render(&leds);
    if (result != WS2811_SUCCESS)
        printf("Error: %d\n", result);
    ws2811_fini(&leds);    
    return 0;
}

