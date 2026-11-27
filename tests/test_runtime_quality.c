/* Graphics quality game patches on a synthetic image: only the reference bytes are accepted, a
 * mismatch changes nothing and is reported through the environment the renderer reads, Max writes
 * nothing, and High and Low write their shadow map limit at every site and nowhere else. No game
 * data. */
#include "../src/runtime_quality.c" /* the site table is private to the module */
#include <assert.h>

#define IMAGE_SIZE 0xd3000u

/* What the renderer and the menu read after the runtime: 'q' = the request stands, 'p' = it was
 * refused (request cleared, problem marked), '-' = neither. */
static char told(void) {
    const char *quality = getenv("PS4RT_SO_QUALITY"), *problem = getenv("PS4RT_SO_QUALITY_PROBLEM");
    if (quality && *quality) return problem ? '?' : 'q';
    return problem && !strcmp(problem, "1") ? 'p' : '-';
}

static uint16_t read16(const unsigned char *at) {
    uint16_t value;
    memcpy(&value, at, 2);
    return value;
}

int main(void) {
    assert(runtime_quality_parse(NULL) == 0 && runtime_quality_parse("") == 0 && runtime_quality_parse("max") == 0);
    assert(runtime_quality_parse("high") == 1 && runtime_quality_parse("low") == 2);
    assert(runtime_quality_parse("High") == -1 && runtime_quality_parse("medium") == -1 &&
           runtime_quality_parse("1") == -1);
    assert(runtime_quality_shadow_limit(0) == 4096 && runtime_quality_shadow_limit(1) == 1024 &&
           runtime_quality_shadow_limit(2) == 512 && runtime_quality_shadow_limit(7) == 4096);

    unsigned char *image = calloc(1, IMAGE_SIZE), *before = malloc(IMAGE_SIZE), *reference = malloc(IMAGE_SIZE);
    assert(image && before && reference);

    /* No request, Max, or text that is no quality: nothing is read or written. */
    static const char *const nothing[] = {NULL, "", "max", "ultra"};
    for (size_t t = 0; t < sizeof(nothing) / sizeof(*nothing); ++t) {
        if (nothing[t]) setenv("PS4RT_SO_QUALITY", nothing[t], 1);
        else unsetenv("PS4RT_SO_QUALITY");
        setenv("PS4RT_SO_QUALITY_PROBLEM", "1", 1); /* left by an earlier launch: not this one's */
        assert(!runtime_quality_apply(image, IMAGE_SIZE));
        for (uint32_t i = 0; i < IMAGE_SIZE; ++i) assert(!image[i]);
        /* Max stands as the request; text that is no quality is cleared; neither is a problem. */
        assert(told() == (nothing[t] && !strcmp(nothing[t], "max") ? 'q' : '-'));
    }

    /* An image that is not the reference build (all zero here) is left as it is, and the request
     * is taken back so the renderer and the menu run at Max and can say why. */
    for (const char *const *text = (const char *const[]){"high", "low", NULL}; *text; ++text) {
        setenv("PS4RT_SO_QUALITY", *text, 1);
        assert(!runtime_quality_apply(image, IMAGE_SIZE));
        for (uint32_t i = 0; i < IMAGE_SIZE; ++i) assert(!image[i]);
        assert(told() == 'p');
        setenv("PS4RT_SO_QUALITY", *text, 1);
        assert(!runtime_quality_apply(image, 0x1000)); /* too small to hold the sites */
        assert(told() == 'p');
    }

    /* The reference build: the bytes each site expects. */
    for (size_t i = 0; i < QUALITY_SITES; ++i)
        memcpy(image + quality_sites[i].at, quality_sites[i].original, quality_sites[i].size);
    memcpy(reference, image, IMAGE_SIZE);

    /* One changed byte at the first or last byte of any site rejects the whole set. */
    setenv("PS4RT_SO_QUALITY", "low", 1);
    for (size_t i = 0; i < QUALITY_SITES; ++i) {
        for (int last = 0; last < 2; ++last) {
            const uint32_t at = quality_sites[i].at + (last ? quality_sites[i].size - 1 : 0);
            image[at] ^= 0x01;
            memcpy(before, image, IMAGE_SIZE);
            setenv("PS4RT_SO_QUALITY", "low", 1);
            assert(!runtime_quality_apply(image, IMAGE_SIZE));
            assert(!memcmp(before, image, IMAGE_SIZE) && told() == 'p');
            image[at] ^= 0x01;
        }
    }
    assert(!memcmp(reference, image, IMAGE_SIZE));
    /* An image that ends inside a site is not the reference build either. */
    setenv("PS4RT_SO_QUALITY", "low", 1);
    assert(!runtime_quality_apply(image, 0xd2b9c + 5));
    assert(!memcmp(reference, image, IMAGE_SIZE) && told() == 'p');

    /* Max leaves the reference build alone. */
    setenv("PS4RT_SO_QUALITY", "max", 1);
    assert(!runtime_quality_apply(image, IMAGE_SIZE) && !memcmp(reference, image, IMAGE_SIZE));
    assert(told() == 'q');

    /* High and Low: every 0x1000 limit becomes theirs (eleven 16-bit fields in the seven sites)
     * and no other byte changes. */
    static const struct { const char *text; int quality; uint16_t limit; } levels[] = {{"high", 1, 1024}, {"low", 2, 512}};
    for (size_t l = 0; l < sizeof(levels) / sizeof(*levels); ++l) {
        memcpy(image, reference, IMAGE_SIZE);
        setenv("PS4RT_SO_QUALITY", levels[l].text, 1);
        setenv("PS4RT_SO_QUALITY_PROBLEM", "1", 1); /* left by an earlier launch */
        assert(runtime_quality_apply(image, IMAGE_SIZE) == levels[l].quality);
        assert(told() == 'q' && !strcmp(getenv("PS4RT_SO_QUALITY"), levels[l].text));
        unsigned changed = 0, fields = 0;
        for (uint32_t i = 0; i < IMAGE_SIZE; ++i) changed += image[i] != reference[i];
        for (size_t i = 0; i < QUALITY_SITES; ++i) {
            const QualitySite *site = &quality_sites[i];
            for (int k = 0; k < 2; ++k) {
                if (!site->limits[k]) continue;
                assert(read16(reference + site->at + site->limits[k]) == 0x1000);
                assert(read16(image + site->at + site->limits[k]) == levels[l].limit);
                ++fields;
            }
            /* The opcode bytes in front of each limit stay. */
            assert(!memcmp(image + site->at, site->original, 2));
        }
        assert(fields == 11 && changed == fields); /* 0x1000 -> 0x0400 / 0x0200: the high byte only */
        /* The light's setup now reads: mov dx,limit ; cmp ecx,limit (twice). */
        assert(read16(image + 0xd2b59) == levels[l].limit && image[0xd2b83] == 0x81 && image[0xd2b84] == 0xf9 &&
               read16(image + 0xd2b85) == levels[l].limit && read16(image + 0xd2b87) == 0);
        /* A second apply on the changed image is not the reference build: nothing more happens. */
        memcpy(before, image, IMAGE_SIZE);
        assert(!runtime_quality_apply(image, IMAGE_SIZE) && !memcmp(before, image, IMAGE_SIZE));
        assert(told() == 'p');
    }

    free(image);
    free(before);
    free(reference);
    puts("PASS: graphics quality game patches");
    return 0;
}
