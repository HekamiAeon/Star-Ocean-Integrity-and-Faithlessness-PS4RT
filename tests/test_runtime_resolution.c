/* Internal resolution game patches on a synthetic image: only the reference bytes are accepted,
 * a mismatch changes nothing, the sizes follow the request, and the wide-screen group is written
 * only for a size wider than 16:9. No game data. */
#include "../src/runtime_resolution.c" /* the patch tables are private to the module */
#include <assert.h>
#include <math.h>

#define IMAGE_SIZE 0xb86000u
#define SIZE_COUNT COUNT(size_patches)
#define WIDE_COUNT COUNT(wide_patches)
#define PATCH_COUNT (SIZE_COUNT + WIDE_COUNT)

static const Patch *patch(size_t i) {
    return i < SIZE_COUNT ? &size_patches[i] : &wide_patches[i - SIZE_COUNT];
}
static void imm32(unsigned char *at, unsigned char opcode, uint32_t value) {
    at[0] = opcode;
    memcpy(at + 1, &value, 4);
}
static uint32_t read32(const unsigned char *at) {
    uint32_t value;
    memcpy(&value, at, 4);
    return value;
}
/* The per-size fields of the reference build. */
static const uint32_t fields[] = {0x3de8a9, 0x3de8ae, 0x851098, 0x916e8, 0x91712, 0x91730, 0x91770};
static void reference_fields(unsigned char *image) {
    const float aspect = 16.0f / 9.0f;
    imm32(image + 0x3de8a9, 0xba, 1920);
    imm32(image + 0x3de8ae, 0xb9, 1080);
    memcpy(image + 0x851098, &aspect, 4);
    imm32(image + 0x916e8, 0xba, 0x78000000);
    imm32(image + 0x91712, 0xbe, 0x78000000);
    imm32(image + 0x91730, 0xbf, 0x77fffc00);
    imm32(image + 0x91770, 0xba, 0x77fffc00);
}

int main(void) {
    uint32_t w = 0, h = 0;
    assert(runtime_resolution_parse(NULL, &w, &h) == 0 && runtime_resolution_parse("", &w, &h) == 0);
    assert(runtime_resolution_parse("1920x1080", &w, &h) == 0);
    assert(runtime_resolution_parse("5120x1440", &w, &h) == 1 && w == 5120 && h == 1440);
    assert(runtime_resolution_parse("1024x576", &w, &h) == 1 && w == 1024 && h == 576);
    assert(runtime_resolution_parse("3840x2160", &w, &h) == 1 && w == 3840 && h == 2160);
    /* Narrower than 16:9, too few lines, malformed, or too large for the heap's 32-bit size. */
    assert(runtime_resolution_parse("1440x1080", &w, &h) == -1 && runtime_resolution_parse("1920x1200", &w, &h) == -1);
    assert(runtime_resolution_parse("640x350", &w, &h) == -1 && runtime_resolution_parse("0x0", &w, &h) == -1);
    assert(runtime_resolution_parse("5120", &w, &h) == -1 && runtime_resolution_parse("5120x1440x", &w, &h) == -1);
    assert(runtime_resolution_parse("wide", &w, &h) == -1 && runtime_resolution_parse("6880x2880", &w, &h) == -1);
    assert(w == 3840 && h == 2160); /* a rejected request leaves the outputs alone */

    /* 136 bytes per pixel over 1920x1080, rounded up to 64 MiB, on the 1920 MiB heap; a smaller
     * screen keeps the game's own heap. */
    assert(runtime_resolution_heap_bytes(1920, 1080) == UINT64_C(1920) << 20);
    assert(runtime_resolution_heap_bytes(1024, 576) == UINT64_C(1920) << 20);
    assert(runtime_resolution_heap_bytes(2560, 1080) == UINT64_C(2048) << 20);
    assert(runtime_resolution_heap_bytes(5120, 1440) == UINT64_C(2624) << 20);

    unsigned char *image = calloc(1, IMAGE_SIZE), *before = malloc(IMAGE_SIZE), *reference = malloc(IMAGE_SIZE);
    assert(image && before && reference);

    /* No request, or the game's own size: nothing is read or written. */
    unsetenv("PS4RT_SO_RESOLUTION");
    assert(!runtime_resolution_apply(image, IMAGE_SIZE));
    setenv("PS4RT_SO_RESOLUTION", "1920x1080", 1);
    assert(!runtime_resolution_apply(image, IMAGE_SIZE));
    for (uint32_t i = 0; i < IMAGE_SIZE; ++i) assert(!image[i]);

    /* An image that is not the reference build (all zero here) is left as it is. */
    unsetenv("PS4RT_DMEM_MB");
    for (const char *const *size = (const char *const[]){"5120x1440", "1280x720", NULL}; *size; ++size) {
        setenv("PS4RT_SO_RESOLUTION", *size, 1);
        assert(!runtime_resolution_apply(image, IMAGE_SIZE));
        for (uint32_t i = 0; i < IMAGE_SIZE; ++i) assert(!image[i]);
        assert(!getenv("PS4RT_DMEM_MB"));
        assert(!runtime_resolution_apply(image, 0x1000)); /* too small to hold the sites */
    }

    /* The reference build: the per-size fields plus the bytes each table site expects. */
    reference_fields(image);
    for (size_t i = 0; i < PATCH_COUNT; ++i)
        if (patch(i)->original) memcpy(image + patch(i)->at, patch(i)->original, patch(i)->size);
    memcpy(reference, image, IMAGE_SIZE);

    /* One changed byte at any site rejects the whole set and writes nothing, for a 16:9 size as
     * for a wide one: a build that differs anywhere is not the reference build. That holds for
     * the first and last byte of every entry of both tables (used space where new code would go
     * included), for each per-size field and for both ends of the widget list. */
    for (int wide = 0; wide < 2; ++wide) {
        setenv("PS4RT_SO_RESOLUTION", wide ? "5120x1440" : "2560x1440", 1);
        for (size_t i = 0; i < PATCH_COUNT + sizeof(fields) / sizeof(*fields) + 2; ++i) {
            for (int last = 0; last < 2; ++last) {
                uint32_t at;
                if (i < PATCH_COUNT) at = patch(i)->at + (last ? patch(i)->size - 1 : 0);
                else if (i < PATCH_COUNT + sizeof(fields) / sizeof(*fields)) at = fields[i - PATCH_COUNT] + (last ? 3 : 0);
                else at = i == PATCH_COUNT + sizeof(fields) / sizeof(*fields) ? 0xb85e40 : 0xb85fff;
                image[at] ^= 0x01;
                memcpy(before, image, IMAGE_SIZE);
                assert(!runtime_resolution_apply(image, IMAGE_SIZE));
                assert(!memcmp(before, image, IMAGE_SIZE) && !getenv("PS4RT_DMEM_MB"));
                image[at] ^= 0x01;
            }
        }
    }
    assert(!memcmp(reference, image, IMAGE_SIZE));

    /* The reference build takes each menu size; the per-size fields follow the request. The pool
     * is only set when the heap grows (pool NULL: the default is kept). */
    static const struct { const char *text; uint32_t w, h, heap_mb; const char *pool; } sizes[] = {
        {"1024x576", 1024, 576, 1920, NULL},     {"1280x720", 1280, 720, 1920, NULL},
        {"2560x1440", 2560, 1440, 2176, "5312"}, {"3840x2160", 3840, 2160, 2752, "5888"},
        {"2560x1080", 2560, 1080, 2048, "5184"}, {"3440x1440", 3440, 1440, 2304, "5440"},
        {"3840x1440", 3840, 1440, 2432, "5568"}, {"3840x1080", 3840, 1080, 2240, "5376"},
        {"5120x1440", 5120, 1440, 2624, "5760"}};
    for (size_t s = 0; s < sizeof(sizes) / sizeof(*sizes); ++s) {
        const int wide = sizes[s].w * 9 > sizes[s].h * 16;
        memcpy(image, reference, IMAGE_SIZE);
        setenv("PS4RT_SO_RESOLUTION", sizes[s].text, 1);
        unsetenv("PS4RT_DMEM_MB");
        assert(runtime_resolution_apply(image, IMAGE_SIZE) == 1);
        assert(image[0x3de8a9] == 0xba && read32(image + 0x3de8aa) == sizes[s].w);
        assert(image[0x3de8ae] == 0xb9 && read32(image + 0x3de8af) == sizes[s].h);
        float aspect;
        memcpy(&aspect, image + 0x851098, 4);
        assert(fabsf(aspect - (float)sizes[s].w / (float)sizes[s].h) < 1e-6f);
        const uint32_t heap = sizes[s].heap_mb << 20;
        assert(read32(image + 0x916e9) == heap && read32(image + 0x91713) == heap);
        assert(read32(image + 0x91731) == heap - 0x400 && read32(image + 0x91771) == heap - 0x400);
        const char *pool = getenv("PS4RT_DMEM_MB"), *own_pool = getenv("PS4RT_SO_DMEM_MB");
        if (sizes[s].pool) assert(pool && own_pool && !strcmp(pool, sizes[s].pool) && !strcmp(own_pool, sizes[s].pool));
        else assert(!pool && !own_pool);
        /* The screen-size group is written for every size and the wide-screen group only for a
         * size wider than 16:9: a 16:9 size keeps the game's camera, 2D layer and shadow code
         * and leaves the space for new code empty. Nothing else changed, and the widget list is
         * still empty for the game to fill. */
        memcpy(before, reference, IMAGE_SIZE);
        for (size_t i = 0; i < PATCH_COUNT; ++i) {
            const int written = i < SIZE_COUNT || wide;
            if (written) memcpy(before + patch(i)->at, patch(i)->patched, patch(i)->size);
            assert(!memcmp(image + patch(i)->at, before + patch(i)->at, patch(i)->size));
            assert((memcmp(image + patch(i)->at, patch(i)->patched, patch(i)->size) == 0) == written);
        }
        for (size_t i = 0; i < sizeof(fields) / sizeof(*fields); ++i) memcpy(before + fields[i], image + fields[i], 5);
        assert(!memcmp(before, image, IMAGE_SIZE));
        if (!wide) assert(!memcmp(image + 0x851098, reference + 0x851098, 4));
        for (uint32_t i = 0xb85e40; i < 0xb86000; ++i) assert(!image[i]);
        /* Applying twice is refused: the image no longer matches the reference build. */
        memcpy(before, image, IMAGE_SIZE);
        assert(!runtime_resolution_apply(image, IMAGE_SIZE) && !memcmp(before, image, IMAGE_SIZE));
        assert(!getenv("PS4RT_DMEM_MB") && !getenv("PS4RT_SO_DMEM_MB"));
    }
    /* A pool size this module set is its own: an in-game restart hands it to the next launch,
     * which replaces it for another size and drops it at a size that keeps the game's heap. */
    setenv("PS4RT_DMEM_MB", "5184", 1);
    setenv("PS4RT_SO_DMEM_MB", "5184", 1);
    memcpy(image, reference, IMAGE_SIZE);
    setenv("PS4RT_SO_RESOLUTION", "5120x1440", 1);
    assert(runtime_resolution_apply(image, IMAGE_SIZE) == 1 && !strcmp(getenv("PS4RT_DMEM_MB"), "5760"));
    memcpy(image, reference, IMAGE_SIZE);
    setenv("PS4RT_SO_RESOLUTION", "1280x720", 1);
    assert(runtime_resolution_apply(image, IMAGE_SIZE) == 1 && !getenv("PS4RT_DMEM_MB") && !getenv("PS4RT_SO_DMEM_MB"));
    setenv("PS4RT_DMEM_MB", "5760", 1);
    setenv("PS4RT_SO_DMEM_MB", "5760", 1);
    memcpy(image, reference, IMAGE_SIZE);
    unsetenv("PS4RT_SO_RESOLUTION");
    assert(!runtime_resolution_apply(image, IMAGE_SIZE) && !getenv("PS4RT_DMEM_MB") && !getenv("PS4RT_SO_DMEM_MB"));
    assert(!memcmp(reference, image, IMAGE_SIZE));
    /* An explicit pool size from the caller is kept, at any size and at the game's own. */
    setenv("PS4RT_DMEM_MB", "8192", 1);
    assert(!runtime_resolution_apply(image, IMAGE_SIZE) && !strcmp(getenv("PS4RT_DMEM_MB"), "8192"));
    setenv("PS4RT_SO_RESOLUTION", "3440x1440", 1);
    assert(runtime_resolution_apply(image, IMAGE_SIZE) == 1 && !strcmp(getenv("PS4RT_DMEM_MB"), "8192"));
    assert(!getenv("PS4RT_SO_DMEM_MB"));

    free(image); free(before); free(reference);
    puts("PASS: internal resolution game patches");
    return 0;
}
