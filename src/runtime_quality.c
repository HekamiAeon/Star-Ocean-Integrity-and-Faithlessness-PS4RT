/* Star Ocean: Integrity and Faithlessness (CUSA03219) graphics quality, the option for slow GPUs.
 *
 * PS4RT_SO_QUALITY=max|high|low (ps4rt.ini graphics_quality, applied on restart). Max, or unset,
 * is the game as shipped and changes nothing. High and Low limit the size of the game's shadow
 * maps, the part of a frame it lets go of with nothing else changing:
 *
 *   High   shadow maps up to 1024 a side
 *   Low    shadow maps up to 512 a side (and the renderer leaves out the ambient occlusion
 *          passes, star_ocean_quality.h)
 *
 * A light keeps the size of its shadow map in two 16-bit fields (+0x1a6 width, +0x1a8 height of
 * the light object); the sun's is 2048, one map per depth zone in an atlas of up to three. The
 * game writes them in two places, each with its own upper limit of 4096:
 *
 *   0xd2b57   the light's setup from the scene's data: mov dx,0x1000, then the width and the
 *             height each compared with 0x1000 (0xd2b83, 0xd2b9c) and replaced by dx when larger
 *   0x670ef   the light's parameter setter (width and height, each on its shrink and its grow
 *   0x67121   path): cmp edx,0x1000 ; mov ax,0x1000 ; cmovbe ax,dx
 *   0x675d1
 *   0x675fb
 *
 * The limit becomes 1024 or 512 at all of them, so a larger map is created at the limit and a
 * smaller one stays as it is. The game reads the fields every frame for the atlas, the zone
 * viewports and the filter, so nothing else follows from the size.
 *
 * Measured on a Radeon integrated GPU (Ryzen 7 9700X) at 1024x576, GPU time a frame: a town inn
 * 17.2 ms (56 frames a second) as shipped, 15.4 ms at High and 13.5 ms at Low (60 frames a second
 * both); a forest 13.3, 11.7 and 8.7 ms. With smaller maps shadow edges get softer and nothing
 * else changes. One depth zone fewer saved 0.6 to 1.4 ms more but drops distant shadows, and the
 * game's own switches for ambient occlusion and bloom need shader variants it does not ship
 * (objects stop drawing, the screen turns to its missing-shader pattern).
 *
 * Done by changing the game's code in memory after it is loaded; no game file is read or written
 * for it. Every site is compared with the reference build first: if anything differs, nothing is
 * changed and the launch runs at Max. The renderer and the menu then have to know, so this runs
 * before they start and a refused request is taken out of the environment they read:
 * PS4RT_SO_QUALITY is cleared and PS4RT_SO_QUALITY_PROBLEM is set. */
#define _GNU_SOURCE
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t at;
    unsigned char size, original[10];
    unsigned char limits[2]; /* offsets of the 16-bit limit inside the site; 0 = none */
} QualitySite;

#define GAME_LIMIT 0x1000u
static const QualitySite quality_sites[] = {
    {0xd2b57, 4, {0x66, 0xba, 0x00, 0x10}, {2, 0}},                                      /* mov dx,0x1000 */
    {0xd2b83, 6, {0x81, 0xf9, 0x00, 0x10, 0x00, 0x00}, {2, 0}},                          /* cmp ecx,0x1000 */
    {0xd2b9c, 6, {0x81, 0xf9, 0x00, 0x10, 0x00, 0x00}, {2, 0}},                          /* cmp ecx,0x1000 */
    {0x670ef, 10, {0x81, 0xfa, 0x00, 0x10, 0x00, 0x00, 0x66, 0xb8, 0x00, 0x10}, {2, 8}}, /* cmp edx ; mov ax */
    {0x67121, 10, {0x81, 0xfa, 0x00, 0x10, 0x00, 0x00, 0x66, 0xb8, 0x00, 0x10}, {2, 8}},
    {0x675d1, 10, {0x81, 0xfa, 0x00, 0x10, 0x00, 0x00, 0x66, 0xb8, 0x00, 0x10}, {2, 8}},
    {0x675fb, 10, {0x81, 0xfa, 0x00, 0x10, 0x00, 0x00, 0x66, 0xb8, 0x00, 0x10}, {2, 8}},
};
#define QUALITY_SITES (sizeof(quality_sites) / sizeof(*quality_sites))

static void quality_env(const char *name, const char *value) {
#ifdef _WIN32
    _putenv_s(name, value ? value : "");
#else
    if (value) setenv(name, value, 1);
    else unsetenv(name);
#endif
}

int runtime_quality_parse(const char *text) {
    if (!text || !*text || !strcmp(text, "max")) return 0;
    if (!strcmp(text, "high")) return 1;
    if (!strcmp(text, "low")) return 2;
    return -1;
}

uint32_t runtime_quality_shadow_limit(int quality) {
    return quality == 1 ? 1024u : quality == 2 ? 512u : GAME_LIMIT;
}

int runtime_quality_apply(unsigned char *image, uint64_t image_size) {
    const char *request = getenv("PS4RT_SO_QUALITY");
    const int quality = runtime_quality_parse(request);
    quality_env("PS4RT_SO_QUALITY_PROBLEM", NULL); /* a marker inherited from an earlier launch */
    if (quality < 0) {
        fprintf(stderr, "Runtime: PS4RT_SO_QUALITY '%s' is not max, high or low; using max\n", request);
        quality_env("PS4RT_SO_QUALITY", NULL);
    }
    if (quality <= 0) return 0;
    for (size_t i = 0; i < QUALITY_SITES; ++i) {
        const QualitySite *site = &quality_sites[i];
        if (image_size < (uint64_t)site->at + site->size || memcmp(image + site->at, site->original, site->size)) {
            fputs("Runtime: graphics quality not applied (code differs from the reference build); using max\n",
                  stderr);
            quality_env("PS4RT_SO_QUALITY", NULL);
            quality_env("PS4RT_SO_QUALITY_PROBLEM", "1");
            return 0;
        }
    }
    const uint16_t limit = (uint16_t)runtime_quality_shadow_limit(quality);
    for (size_t i = 0; i < QUALITY_SITES; ++i) {
        const QualitySite *site = &quality_sites[i];
        for (int k = 0; k < 2; ++k)
            if (site->limits[k]) memcpy(image + site->at + site->limits[k], &limit, 2);
    }
    printf("Runtime: graphics quality %s, shadow maps up to %u\n", quality == 1 ? "high" : "low", (unsigned)limit);
    return quality;
}
