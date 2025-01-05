#include <ws2811.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <map>
#include <chrono>
#include <thread>

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
    if (0 <= x < 32 && 0 <= y < 16)
        leds.channel[0].leds[map(x, y)] = color;
}

int writeChar(char c, int x, int y, uint32_t color) {
    const Bitmap &bitmap = bitmapFont[c];
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

void border(uint32_t color) {
    for (int i = 0; i < 32; ++i) {
        set(i, 0, color);
        set(i, 15, color);
    }
    for (int i = 1; i < 15; ++i) {
        set(0, i, color);
        set(31, i, color);
    }
}

int main() {
    /* Load the bitmap font */
    for (auto const &[c, filename] : bitmapFontFiles) {
        Bitmap bitmap;
        bitmap.data = stbi_load(("font/" + filename).c_str(), &bitmap.width, &bitmap.height, &bitmap.channels, 0);
        bitmapFont[c] = bitmap;
    }

    /* Start the sync thread */
    std::thread syncThread(syncTask);

    ws2811_return_t result = ws2811_init(&leds);
    if (result != WS2811_SUCCESS) {
        printf("Error: %d\n", result);
        return 1;
    }
    while (true) {
        const auto t = std::chrono::system_clock::now().time_since_epoch();
        int totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(t).count();
        int ms = totalMs % 1000;
        totalMs = totalMs - ms;
        int s = (totalMs / 1000) % 60;
        totalMs = totalMs - (s * 1000);
        int m = (totalMs / (60 * 1000)) % 60;
        totalMs = totalMs - (m * (60 * 1000));
        int h = (totalMs / (60 * 60 * 1000)) % 24;

        int tenths = ms / 100;
        
        int64_t x = 0;
        {
            std::lock_guard<std::mutex> guard(timerValueMutex);
            x = timerValue;
        }
        tenths = (x / 100) % 10;
        s = (x / 1000) % 60;
        m = (x / (1000 * 60)) % 60;        
        h = (x / (1000 * 60 * 60)) % 24;

        //printf("timer: %lld\n", x);


        char line1[16];
        char line2[16];
        sprintf(line1, "%02d:%02d:%02d", h, m, s);
        sprintf(line2, ".%01d", tenths);

        
        clear();
        uint32_t base = PBColor;
        border(base);
        writeLine(std::string(line1), 2, 2, base);
        writeLine(std::string(line2), 24, 9, base);
        result = ws2811_render(&leds);
        if (result != WS2811_SUCCESS)
            printf("Error: %d\n", result);

        auto tim = std::chrono::system_clock::now();
        tim += std::chrono::milliseconds(33);
        std::this_thread::sleep_until(tim);
    }
    ws2811_fini(&leds);    
    return 0;
}

