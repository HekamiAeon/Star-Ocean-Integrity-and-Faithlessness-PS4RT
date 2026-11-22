/* Stick dead zone of the production pad code (src/runtime_pad.c), without opening a controller. */
#include "../src/runtime_pad.c"
#include <assert.h>

int ps4rtgpu_overlay_captures_input(void) { return 0; }
int ps4rtgpu_vibration_percent(void) { return 100; }
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    for (size_t i=0;i<count;++i) if (!strcmp(table[i].name,name)) return (uintptr_t)table[i].function;
    return 0;
}

/* SDL's value for a DualSense report byte (0..255, 128 at rest). */
static int16_t dualsense(int byte) { return (int16_t)(byte*257-32768); }

int main(void) {
    /* A stick resting or settling up to 10 steps off center reads as centered: the game's own
     * dead zone of 2 would count it as input. */
    for (int byte=118;byte<=138;++byte) assert(axis(dualsense(byte))==128);
    assert(axis(0)==128 && axis(2621)==128 && axis(-2621)==128);
    /* The whole travel remains, and the response rises one step at a time past the dead zone. */
    assert(axis(-32768)==0 && axis(32767)==255);
    assert(axis(dualsense(0))==0 && axis(dualsense(255))==255);
    for (int v=-32767;v<=32767;++v) {
        const int previous=axis((int16_t)(v-1)), current=axis((int16_t)v);
        assert(current>=previous && current-previous<=1);
    }
    /* Both directions are symmetric around 128 (within the one step of the 0..255 range). */
    for (int v=0;v<=32767;v+=97) {
        const int up=axis((int16_t)v)-128, down=128-axis((int16_t)-v);
        assert(up>=0 && down>=0 && (up-down<=1 && down-up<=1));
    }
    puts("PASS: stick dead zone");
    return 0;
}
