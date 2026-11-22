/* Star Ocean: Integrity and Faithlessness (CUSA03219) internal render resolution.
 *
 * The game draws for a 1920x1080 screen. PS4RT_SO_RESOLUTION=<w>x<h> makes it draw the 3D scene
 * at that size, while its 2D layer stays a 1920x1080 canvas. A 16:9 size changes the number of
 * pixels and nothing else. A size wider than 16:9 keeps the same vertical view and shows more to
 * the sides (Hor+), with the 2D canvas centred on the screen. Unset, or 1920x1080, leaves the
 * game untouched.
 *
 * This is done by changing the game's code in memory after it is loaded; no game file is read or
 * written for it. The changes for every size:
 *
 *   Screen size    The setup at 0x3de8a9 gives the 3D size ("screen B", fields +0x38/+0x3a of the
 *                  screen object) as 1920x1080 constants; they become <w> and <h>. The screen
 *                  buffers (0x1ef80, 0x1efca) and the display view (0x1f189) are created from the
 *                  canvas size (+0x28/+0x2a) and now take the 3D size.
 *   GPU heap       FUN_00091620 reserves a fixed 1920 MiB heap for all GPU memory. Larger
 *                  screen buffers come out of it (136 bytes per pixel, measured), so above
 *                  1920x1080 it grows by that much, rounded up to 64 MiB, and the direct memory
 *                  pool (PS4RT_DMEM_MB) grows with it. Without this a shadow atlas can fail to
 *                  allocate for a frame.
 *
 * and for a size wider than 16:9 only (all of them leave a 16:9 picture as it is):
 *
 *   Camera         The 16:9 aspect constant at 0x851098 becomes w/h. The lens code at 0xda8b8
 *                  derives the vertical view from a fixed horizontal one, which would crop the
 *                  top and bottom on a wide screen; hor_plus keeps the 16:9 vertical view.
 *   2D layer       ui_view_size keeps the 1920x1080 canvas centred at 1:1 height. marker_scale and
 *                  target_limits let markers that follow 3D positions use the whole width, and
 *                  note_widget with cull_test keep those markers from being culled at the
 *                  canvas edge while every other widget keeps the game's own test.
 *   Sun shadows    The sun's shadow has three depth zones. The game drops a zone that no object
 *                  touches, or whose clipped box equals the previous one. With the wider view
 *                  that happens as the camera turns, the layer count flips between 2 and 3 and
 *                  distant shadows pop in and out. Every zone is kept.
 *
 * Every site of both groups is compared with the reference build first, and the space used for
 * new code and for the widget list must be unused (zero). If anything differs, nothing is
 * changed and the game runs at 1920x1080. */
#define _GNU_SOURCE
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE_WIDTH 1920u
#define BASE_HEIGHT 1080u
#define MIN_HEIGHT 360u
#define OFF_SIZE 0x3de8a9u      /* mov edx, 1920 ; mov ecx, 1080 */
#define OFF_ASPECT 0x851098u    /* float 16/9 */
#define OFF_HEAP_RESERVE 0x916e8u /* mov edx, 0x78000000 (allocate) */
#define OFF_HEAP_MAP 0x91712u     /* mov esi, 0x78000000 (map) */
#define OFF_HEAP_META 0x91730u    /* mov edi, 0x77fffc00 (heap metadata) */
#define OFF_HEAP_INIT 0x91770u    /* mov edx, 0x77fffc00 (heap init) */
#define HEAP_BYTES UINT64_C(0x78000000)
#define HEAP_HEADER 0x400u
#define HEAP_MAX UINT64_C(0xf0000000)
#define HEAP_BYTES_PER_PIXEL 136u
#define HEAP_STEP (UINT64_C(64) << 20)
#define POOL_MB 5056u             /* retail direct memory pool (runtime_memory.c) */
/* Widget list written by note_widget and read by cull_test: unused bytes after .bss. */
#define OFF_WIDGETS 0xb85e40u
#define WIDGETS_END 0xb86000u

/* 0x1ef80: movzx esi,word [r14+0x28] -> [r14+0x38] */
static const unsigned char buffer_a_width_old[] = {
    0x41, 0x0f, 0xb7, 0x76, 0x28,
};
static const unsigned char buffer_a_width_new[] = {
    0x41, 0x0f, 0xb7, 0x76, 0x38,
};
/* 0x1ef85: movzx edx,word [r14+0x2a] -> [r14+0x3a] */
static const unsigned char buffer_a_height_old[] = {
    0x41, 0x0f, 0xb7, 0x56, 0x2a,
};
static const unsigned char buffer_a_height_new[] = {
    0x41, 0x0f, 0xb7, 0x56, 0x3a,
};
/* 0x1efca: movzx esi,word [r14+0x28] -> [r14+0x38] */
static const unsigned char buffer_b_width_old[] = {
    0x41, 0x0f, 0xb7, 0x76, 0x28,
};
static const unsigned char buffer_b_width_new[] = {
    0x41, 0x0f, 0xb7, 0x76, 0x38,
};
/* 0x1efcf: movzx edx,word [r14+0x2a] -> [r14+0x3a] */
static const unsigned char buffer_b_height_old[] = {
    0x41, 0x0f, 0xb7, 0x56, 0x2a,
};
static const unsigned char buffer_b_height_new[] = {
    0x41, 0x0f, 0xb7, 0x56, 0x3a,
};
/* 0x1f189: movzx edx,word [rbx+0x28] -> [rbx+0x38] */
static const unsigned char view_width_old[] = {
    0x0f, 0xb7, 0x53, 0x28,
};
static const unsigned char view_width_new[] = {
    0x0f, 0xb7, 0x53, 0x38,
};
/* 0x1f18d: movzx ecx,word [rbx+0x2a] -> [rbx+0x3a] */
static const unsigned char view_height_old[] = {
    0x0f, 0xb7, 0x4b, 0x2a,
};
static const unsigned char view_height_new[] = {
    0x0f, 0xb7, 0x4b, 0x3a,
};
/* 0x1f1bc: mov cx,[rbx+0x28] -> [rbx+0x38] */
static const unsigned char view_width2_old[] = {
    0x66, 0x8b, 0x4b, 0x28,
};
static const unsigned char view_width2_new[] = {
    0x66, 0x8b, 0x4b, 0x38,
};
/* 0x1f1c9: mov cx,[rbx+0x2a] -> [rbx+0x3a] */
static const unsigned char view_height2_old[] = {
    0x66, 0x8b, 0x4b, 0x2a,
};
static const unsigned char view_height2_new[] = {
    0x66, 0x8b, 0x4b, 0x3a,
};
/* 0x5b04b: je 0x5b13c -> je keep_empty_zone */
static const unsigned char zone_empty_old[] = {
    0x0f, 0x84, 0xeb, 0x00, 0x00, 0x00,
};
static const unsigned char zone_empty_new[] = {
    0x0f, 0x84, 0x2f, 0x0b, 0x90, 0x00,
};
/* 0x5b0cd: jne 0x5b13c -> 2-byte nop */
static const unsigned char zone_equal_old[] = {
    0x75, 0x6d,
};
static const unsigned char zone_equal_new[] = {
    0x66, 0x90,
};
/* 0xda8b8: jmp hor_plus ; nops */
static const unsigned char lens_old[] = {
    0xc5, 0xfa, 0x10, 0x4f, 0x14, 0xc5, 0xf2, 0x59, 0x4f, 0x24, 0xc5, 0xf2, 0x5e, 0xd8,
};
static const unsigned char lens_new[] = {
    0xe9, 0x43, 0x12, 0x88, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
};
/* 0xdca66: jmp marker_scale ; nops */
static const unsigned char marker_old[] = {
    0xc5, 0xfa, 0x59, 0x83, 0xc0, 0x0e, 0x00, 0x00, 0xc4, 0xe3, 0x69, 0x21, 0xc8, 0x00, 0xc4, 0xe3,
    0x71, 0x21, 0xc0, 0x30,
};
static const unsigned char marker_new[] = {
    0xe9, 0x95, 0xed, 0x87, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    0x90, 0x90, 0x90, 0x90,
};
/* 0x1d1c8a: jmp target_limits ; nops */
static const unsigned char target_clamp_old[] = {
    0xc5, 0xf8, 0x11, 0x43, 0x50, 0x48, 0x83, 0xc4, 0x08,
};
static const unsigned char target_clamp_new[] = {
    0xe9, 0xf1, 0x9b, 0x78, 0x00, 0x90, 0x90, 0x90, 0x90,
};
/* 0x202d40: jmp note_widget */
static const unsigned char set_position_old[] = {
    0xc5, 0xfa, 0x11, 0x47, 0x70,
};
static const unsigned char set_position_new[] = {
    0xe9, 0xbb, 0x8c, 0x75, 0x00,
};
/* 0x203530: jmp cull_test ; nops */
static const unsigned char cull_old[] = {
    0x48, 0x8d, 0x05, 0xb9, 0x65, 0x83, 0x00,
};
static const unsigned char cull_new[] = {
    0xe9, 0x9b, 0x83, 0x75, 0x00, 0x90, 0x90,
};
/* 0x3e0e38: jmp ui_view_size ; nops */
static const unsigned char ui_view_old[] = {
    0x0f, 0xb7, 0x49, 0x28, 0xc5, 0xfa, 0x2a, 0xc1, 0xc5, 0xfa, 0x11, 0x80, 0xe8, 0x0d, 0x00, 0x00,
};
static const unsigned char ui_view_new[] = {
    0xe9, 0x03, 0xaa, 0x57, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
};

/* marker_scale (from 0xdca66): row 0 of the camera's UI matrix. The scale becomes half the
 * visible canvas width (0.5 * A_h * aspect) around the same centre (0.5 * A_w), so a marker
 * projected from a 3D position lands on it across the whole screen.
 *
 *   95b800: vmulss xmm0,xmm0,DWORD PTR [rbx+0xec0]
 *   95b808: vmovss xmm1,DWORD PTR [rbx+0xec4]
 *   95b810: vmulss xmm1,xmm1,DWORD PTR [rip: 0x851098]
 *   95b818: vmulss xmm1,xmm1,DWORD PTR [rip: 0x8606ac]
 *   95b820: vinsertps xmm0,xmm1,xmm0,0x30
 *   95b826: jmp 0xdca7a
 */
static const unsigned char marker_scale[] = {
    0xc5, 0xfa, 0x59, 0x83, 0xc0, 0x0e, 0x00, 0x00, 0xc5, 0xfa, 0x10, 0x8b, 0xc4, 0x0e, 0x00, 0x00,
    0xc5, 0xf2, 0x59, 0x0d, 0x80, 0x58, 0xef, 0xff, 0xc5, 0xf2, 0x59, 0x0d, 0x8c, 0x4e, 0xf0, 0xff,
    0xc4, 0xe3, 0x71, 0x21, 0xc0, 0x30, 0xe9, 0x4f, 0x12, 0x78, 0xff,
};

/* ui_view_size (from 0x3e0e38): the 2D camera's view is 1080 * aspect wide with the screen's
 * aspect, so the 1920x1080 canvas (camera centre 960,540) stays centred at 1:1 height.
 *
 *   95b840: movzx ecx,WORD PTR [rcx+0x2a]
 *   95b844: vcvtsi2ss xmm0,xmm0,ecx
 *   95b848: vmulss xmm0,xmm0,DWORD PTR [rip: 0x851098]
 *   95b850: vmovss DWORD PTR [rax+0xde8],xmm0
 *   95b858: vmovss xmm0,DWORD PTR [rip: 0x851098]
 *   95b860: vmovss DWORD PTR [rax+0xebc],xmm0
 *   95b868: jmp 0x3e0e48
 */
static const unsigned char ui_view_size[] = {
    0x0f, 0xb7, 0x49, 0x2a, 0xc5, 0xfa, 0x2a, 0xc1, 0xc5, 0xfa, 0x59, 0x05, 0x48, 0x58, 0xef, 0xff,
    0xc5, 0xfa, 0x11, 0x80, 0xe8, 0x0d, 0x00, 0x00, 0xc5, 0xfa, 0x10, 0x05, 0x38, 0x58, 0xef, 0xff,
    0xc5, 0xfa, 0x11, 0x80, 0xbc, 0x0e, 0x00, 0x00, 0xe9, 0xdb, 0x55, 0xa8, 0xff,
};

/* target_limits (from 0x1d1c8a): the rectangle the TARGET marker is held in. Its x limits sit 5%
 * inside the visible canvas (A_w/2 -+ 0.45 * A_h * aspect) instead of the 16:9 canvas; the
 * values are the game's own at 16:9.
 *
 *   95b880: vcvtsi2ss xmm1,xmm1,eax
 *   95b884: vmulss xmm1,xmm1,DWORD PTR [rip: 0x851098]
 *   95b88c: vmulss xmm2,xmm1,DWORD PTR [rip: 0x872570]
 *   95b894: vmulss xmm1,xmm1,DWORD PTR [rip: 0x8606ac]
 *   95b89c: vsubss xmm1,xmm1,xmm2
 *   95b8a0: vcvtsi2ss xmm3,xmm3,ecx
 *   95b8a4: vmulss xmm3,xmm3,DWORD PTR [rip: 0x8606ac]
 *   95b8ac: vsubss xmm4,xmm3,xmm1
 *   95b8b0: vaddss xmm5,xmm3,xmm1
 *   95b8b4: vinsertps xmm0,xmm0,xmm4,0x0
 *   95b8ba: vinsertps xmm0,xmm0,xmm5,0x20
 *   95b8c0: vmovups XMMWORD PTR [rbx+0x50],xmm0
 *   95b8c5: add rsp,0x8
 *   95b8c9: jmp 0x1d1c93
 */
static const unsigned char target_limits[] = {
    0xc5, 0xf2, 0x2a, 0xc8, 0xc5, 0xf2, 0x59, 0x0d, 0x0c, 0x58, 0xef, 0xff, 0xc5, 0xf2, 0x59, 0x15,
    0xdc, 0x6c, 0xf1, 0xff, 0xc5, 0xf2, 0x59, 0x0d, 0x10, 0x4e, 0xf0, 0xff, 0xc5, 0xf2, 0x5c, 0xca,
    0xc5, 0xe2, 0x2a, 0xd9, 0xc5, 0xe2, 0x59, 0x1d, 0x00, 0x4e, 0xf0, 0xff, 0xc5, 0xe2, 0x5c, 0xe1,
    0xc5, 0xe2, 0x58, 0xe9, 0xc4, 0xe3, 0x79, 0x21, 0xc4, 0x00, 0xc4, 0xe3, 0x79, 0x21, 0xc5, 0x20,
    0xc5, 0xf8, 0x11, 0x43, 0x50, 0x48, 0x83, 0xc4, 0x08, 0xe9, 0xc5, 0x63, 0x87, 0xff,
};

/* cull_test (replaces 0x203530): a widget in the list (or a child of one, up to 8 levels) is
 * tested against the visible canvas [A_w/2 -+ A_h * aspect / 2]; every other widget keeps the
 * game's [0, A_w] test. Identical to the original at 16:9.
 *
 *   95b8d0: mov rcx,QWORD PTR [rip: 0xa39af0]
 *   95b8d7: mov r9,rdi
 *   95b8da: xor r10d,r10d
 *   95b8dd: lea r11,[rip: 0xb85e40]
 *   95b8e4: test r9,r9
 *   95b8e7: je 0x95b923
 *   95b8e9: xor eax,eax
 *   95b8eb: mov rdx,rax
 *   95b8ee: shl rdx,0x4
 *   95b8f2: cmp QWORD PTR [r11+rdx*1+0x10],r9
 *   95b8f7: jne 0x95b90f
 *   95b8f9: mov esi,DWORD PTR [r11+rdx*1+0x18]
 *   95b8fe: cmp esi,DWORD PTR [r9+0x70]
 *   95b902: jne 0x95b90f
 *   95b904: mov esi,DWORD PTR [r11+rdx*1+0x1c]
 *   95b909: cmp esi,DWORD PTR [r9+0x74]
 *   95b90d: je 0x95b92c
 *   95b90f: inc eax
 *   95b911: cmp eax,0x1a
 *   95b914: jb 0x95b8eb
 *   95b916: mov r9,QWORD PTR [r9+0x10]
 *   95b91a: inc r10d
 *   95b91d: cmp r10d,0x8
 *   95b921: jb 0x95b8e4
 *   95b923: movzx esi,WORD PTR [rcx+0x28]
 *   95b927: xor r8d,r8d
 *   95b92a: jmp 0x95b964
 *   95b92c: movzx esi,WORD PTR [rcx+0x2a]
 *   95b930: vcvtsi2ss xmm2,xmm2,esi
 *   95b934: vmulss xmm2,xmm2,DWORD PTR [rip: 0x851098]
 *   95b93c: vmulss xmm2,xmm2,DWORD PTR [rip: 0x8606ac]
 *   95b944: movzx esi,WORD PTR [rcx+0x28]
 *   95b948: vcvtsi2ss xmm3,xmm3,esi
 *   95b94c: vmulss xmm3,xmm3,DWORD PTR [rip: 0x8606ac]
 *   95b954: vaddss xmm4,xmm3,xmm2
 *   95b958: vsubss xmm5,xmm3,xmm2
 *   95b95c: vcvttss2si esi,xmm4
 *   95b960: vcvttss2si r8d,xmm5
 *   95b964: mov al,0x1
 *   95b966: vmovss xmm0,DWORD PTR [rdi+0x80]
 *   95b96e: vcvttss2si edx,xmm0
 *   95b972: cmp edx,esi
 *   95b974: jge 0x95b9aa
 *   95b976: vmovss xmm1,DWORD PTR [rdi+0x84]
 *   95b97e: movzx ecx,WORD PTR [rcx+0x2a]
 *   95b982: vcvttss2si edx,xmm1
 *   95b986: cmp edx,ecx
 *   95b988: jge 0x95b9aa
 *   95b98a: vaddss xmm0,xmm0,DWORD PTR [rdi+0xb0]
 *   95b992: vcvttss2si ecx,xmm0
 *   95b996: cmp ecx,r8d
 *   95b999: jl 0x95b9aa
 *   95b99b: vaddss xmm0,xmm1,DWORD PTR [rdi+0xb4]
 *   95b9a3: vcvttss2si eax,xmm0
 *   95b9a7: shr eax,0x1f
 *   95b9aa: ret
 */
static const unsigned char cull_test[] = {
    0x48, 0x8b, 0x0d, 0x19, 0xe2, 0x0d, 0x00, 0x49, 0x89, 0xf9, 0x45, 0x31, 0xd2, 0x4c, 0x8d, 0x1d,
    0x5c, 0xa5, 0x22, 0x00, 0x4d, 0x85, 0xc9, 0x74, 0x3a, 0x31, 0xc0, 0x48, 0x89, 0xc2, 0x48, 0xc1,
    0xe2, 0x04, 0x4d, 0x39, 0x4c, 0x13, 0x10, 0x75, 0x16, 0x41, 0x8b, 0x74, 0x13, 0x18, 0x41, 0x3b,
    0x71, 0x70, 0x75, 0x0b, 0x41, 0x8b, 0x74, 0x13, 0x1c, 0x41, 0x3b, 0x71, 0x74, 0x74, 0x1d, 0xff,
    0xc0, 0x83, 0xf8, 0x1a, 0x72, 0xd5, 0x4d, 0x8b, 0x49, 0x10, 0x41, 0xff, 0xc2, 0x41, 0x83, 0xfa,
    0x08, 0x72, 0xc1, 0x0f, 0xb7, 0x71, 0x28, 0x45, 0x31, 0xc0, 0xeb, 0x38, 0x0f, 0xb7, 0x71, 0x2a,
    0xc5, 0xea, 0x2a, 0xd6, 0xc5, 0xea, 0x59, 0x15, 0x5c, 0x57, 0xef, 0xff, 0xc5, 0xea, 0x59, 0x15,
    0x68, 0x4d, 0xf0, 0xff, 0x0f, 0xb7, 0x71, 0x28, 0xc5, 0xe2, 0x2a, 0xde, 0xc5, 0xe2, 0x59, 0x1d,
    0x58, 0x4d, 0xf0, 0xff, 0xc5, 0xe2, 0x58, 0xe2, 0xc5, 0xe2, 0x5c, 0xea, 0xc5, 0xfa, 0x2c, 0xf4,
    0xc5, 0x7a, 0x2c, 0xc5, 0xb0, 0x01, 0xc5, 0xfa, 0x10, 0x87, 0x80, 0x00, 0x00, 0x00, 0xc5, 0xfa,
    0x2c, 0xd0, 0x39, 0xf2, 0x7d, 0x34, 0xc5, 0xfa, 0x10, 0x8f, 0x84, 0x00, 0x00, 0x00, 0x0f, 0xb7,
    0x49, 0x2a, 0xc5, 0xfa, 0x2c, 0xd1, 0x39, 0xca, 0x7d, 0x20, 0xc5, 0xfa, 0x58, 0x87, 0xb0, 0x00,
    0x00, 0x00, 0xc5, 0xfa, 0x2c, 0xc8, 0x44, 0x39, 0xc1, 0x7c, 0x0f, 0xc5, 0xf2, 0x58, 0x87, 0xb4,
    0x00, 0x00, 0x00, 0xc5, 0xfa, 0x2c, 0xc0, 0xc1, 0xe8, 0x1f, 0xc3,
};

/* note_widget (from 0x202d40, SetPos): stores the position as before; when the caller is one of
 * the six world-to-screen sites, it also remembers (widget, x, y) in a 26-entry ring. The code is
 * followed by the six return addresses, as image offsets: 0x1c8f17, 0x1cea40, 0x1d21e6,
 * 0x21d3c2, 0x21dc27, 0x38303a.
 *
 *   95ba00: vmovss DWORD PTR [rdi+0x70],xmm0
 *   95ba05: vmovss DWORD PTR [rdi+0x74],xmm1
 *   95ba0a: mov rax,QWORD PTR [rsp]
 *   95ba0e: lea r9,[rip: 0x0]
 *   95ba15: sub rax,r9
 *   95ba18: lea rdx,[rip: 0x95ba78]
 *   95ba1f: xor ecx,ecx
 *   95ba21: cmp eax,DWORD PTR [rdx+rcx*4]
 *   95ba24: je 0x95ba2e
 *   95ba26: inc ecx
 *   95ba28: cmp ecx,0x6
 *   95ba2b: jb 0x95ba21
 *   95ba2d: ret
 *   95ba2e: lea r9,[rip: 0xb85e40]
 *   95ba35: xor ecx,ecx
 *   95ba37: mov rdx,rcx
 *   95ba3a: shl rdx,0x4
 *   95ba3e: cmp QWORD PTR [r9+rdx*1+0x10],rdi
 *   95ba43: je 0x95ba63
 *   95ba45: inc ecx
 *   95ba47: cmp ecx,0x1a
 *   95ba4a: jb 0x95ba37
 *   95ba4c: mov ecx,DWORD PTR [r9]
 *   95ba4f: lea edx,[rcx+0x1]
 *   95ba52: cmp edx,0x1a
 *   95ba55: jb 0x95ba59
 *   95ba57: xor edx,edx
 *   95ba59: mov DWORD PTR [r9],edx
 *   95ba5c: mov rdx,rcx
 *   95ba5f: shl rdx,0x4
 *   95ba63: mov QWORD PTR [r9+rdx*1+0x10],rdi
 *   95ba68: vmovss DWORD PTR [r9+rdx*1+0x18],xmm0
 *   95ba6f: vmovss DWORD PTR [r9+rdx*1+0x1c],xmm1
 *   95ba76: ret
 *   95ba77: nop
 */
static const unsigned char note_widget[] = {
    0xc5, 0xfa, 0x11, 0x47, 0x70, 0xc5, 0xfa, 0x11, 0x4f, 0x74, 0x48, 0x8b, 0x04, 0x24, 0x4c, 0x8d,
    0x0d, 0xeb, 0x45, 0x6a, 0xff, 0x4c, 0x29, 0xc8, 0x48, 0x8d, 0x15, 0x59, 0x00, 0x00, 0x00, 0x31,
    0xc9, 0x3b, 0x04, 0x8a, 0x74, 0x08, 0xff, 0xc1, 0x83, 0xf9, 0x06, 0x72, 0xf4, 0xc3, 0x4c, 0x8d,
    0x0d, 0x0b, 0xa4, 0x22, 0x00, 0x31, 0xc9, 0x48, 0x89, 0xca, 0x48, 0xc1, 0xe2, 0x04, 0x49, 0x39,
    0x7c, 0x11, 0x10, 0x74, 0x1e, 0xff, 0xc1, 0x83, 0xf9, 0x1a, 0x72, 0xeb, 0x41, 0x8b, 0x09, 0x8d,
    0x51, 0x01, 0x83, 0xfa, 0x1a, 0x72, 0x02, 0x31, 0xd2, 0x41, 0x89, 0x11, 0x48, 0x89, 0xca, 0x48,
    0xc1, 0xe2, 0x04, 0x49, 0x89, 0x7c, 0x11, 0x10, 0xc4, 0xc1, 0x7a, 0x11, 0x44, 0x11, 0x18, 0xc4,
    0xc1, 0x7a, 0x11, 0x4c, 0x11, 0x1c, 0xc3, 0x90, 0x17, 0x8f, 0x1c, 0x00, 0x40, 0xea, 0x1c, 0x00,
    0xe6, 0x21, 0x1d, 0x00, 0xc2, 0xd3, 0x21, 0x00, 0x27, 0xdc, 0x21, 0x00, 0x3a, 0x30, 0x38, 0x00,
};

/* hor_plus (from 0xda8b8, projection builder FUN_000da4c0): the lens gives the horizontal scale
 * m00 and the game sets m11 = m00 * aspect. m00 is scaled by min(1, (16/9) / aspect), so a wide
 * screen keeps the 16:9 vertical view and adds to the sides. The code is followed by the two
 * constants 16/9 and 1.0.
 *
 *   95bb00: vmovss xmm1,DWORD PTR [rdi+0x14]
 *   95bb05: vmulss xmm1,xmm1,DWORD PTR [rdi+0x24]
 *   95bb0a: vdivss xmm3,xmm1,xmm0
 *   95bb0e: vmovss xmm0,DWORD PTR [rip: 0x95bb34]
 *   95bb16: vdivss xmm0,xmm0,DWORD PTR [r12+0xeb0]
 *   95bb20: vminss xmm0,xmm0,DWORD PTR [rip: 0x95bb38]
 *   95bb28: vmulss xmm3,xmm3,xmm0
 *   95bb2c: jmp 0xda8c6
 */
static const unsigned char hor_plus[] = {
    0xc5, 0xfa, 0x10, 0x4f, 0x14, 0xc5, 0xf2, 0x59, 0x4f, 0x24, 0xc5, 0xf2, 0x5e, 0xd8, 0xc5, 0xfa,
    0x10, 0x05, 0x1e, 0x00, 0x00, 0x00, 0xc4, 0xc1, 0x7a, 0x5e, 0x84, 0x24, 0xb0, 0x0e, 0x00, 0x00,
    0xc5, 0xfa, 0x5d, 0x05, 0x10, 0x00, 0x00, 0x00, 0xc5, 0xe2, 0x59, 0xd8, 0xe9, 0x95, 0xed, 0x77,
    0xff, 0x0f, 0x1f, 0x00, 0x39, 0x8e, 0xe3, 0x3f, 0x00, 0x00, 0x80, 0x3f,
};

/* keep_empty_zone (from the je at 0x5b04b, zone loop of FUN_000591b0): a zone no object touches
 * is stored with its own box (max at [r14-0x10], min at [r15-0x10]) and an empty object list,
 * then joins the normal keep path at 0x5b0cf.
 *
 *   95bb80: movsxd r12,r9d
 *   95bb83: mov rax,r12
 *   95bb86: shl rax,0x5
 *   95bb8a: vmovaps xmm0,XMMWORD PTR [r14-0x10]
 *   95bb90: vmovaps xmm1,XMMWORD PTR [r15-0x10]
 *   95bb96: vmovaps XMMWORD PTR [rsp+rax*1+0x900],xmm1
 *   95bb9f: vmovaps XMMWORD PTR [rsp+rax*1+0x800],xmm0
 *   95bba8: jmp 0x5b0cf
 */
static const unsigned char keep_empty_zone[] = {
    0x4d, 0x63, 0xe1, 0x4c, 0x89, 0xe0, 0x48, 0xc1, 0xe0, 0x05, 0xc4, 0xc1, 0x78, 0x28, 0x46, 0xf0,
    0xc4, 0xc1, 0x78, 0x28, 0x4f, 0xf0, 0xc5, 0xf8, 0x29, 0x8c, 0x04, 0x00, 0x09, 0x00, 0x00, 0xc5,
    0xf8, 0x29, 0x84, 0x04, 0x00, 0x08, 0x00, 0x00, 0xe9, 0x22, 0xf5, 0x6f, 0xff,
};

typedef struct {
    uint32_t at, size;
    const unsigned char *original; /* NULL: unused space, expected zero */
    const unsigned char *patched;
} Patch;
#define SITE(at, name) {at, sizeof(name##_new) + 0 * sizeof(char[sizeof(name##_old) == sizeof(name##_new) ? 1 : -1]), name##_old, name##_new}
#define CAVE(at, name) {at, sizeof(name), NULL, name}
/* Any size: the 3D screen takes its own size. */
static const Patch size_patches[] = {
    SITE(0x1ef80, buffer_a_width),
    SITE(0x1ef85, buffer_a_height),
    SITE(0x1efca, buffer_b_width),
    SITE(0x1efcf, buffer_b_height),
    SITE(0x1f189, view_width),
    SITE(0x1f18d, view_height),
    SITE(0x1f1bc, view_width2),
    SITE(0x1f1c9, view_height2),
};
/* Wider than 16:9 only: camera, 2D layer and sun shadows. */
static const Patch wide_patches[] = {
    SITE(0x5b04b, zone_empty),
    SITE(0x5b0cd, zone_equal),
    SITE(0xda8b8, lens),
    SITE(0xdca66, marker),
    SITE(0x1d1c8a, target_clamp),
    SITE(0x202d40, set_position),
    SITE(0x203530, cull),
    SITE(0x3e0e38, ui_view),
    CAVE(0x95b800, marker_scale),
    CAVE(0x95b840, ui_view_size),
    CAVE(0x95b880, target_limits),
    CAVE(0x95b8d0, cull_test),
    CAVE(0x95ba00, note_widget),
    CAVE(0x95bb00, hor_plus),
    CAVE(0x95bb80, keep_empty_zone),
};
#define COUNT(table) (sizeof(table) / sizeof(*(table)))

static int unchanged(const unsigned char *image, const Patch *table, size_t count);
static void write_patches(unsigned char *image, const Patch *table, size_t count);

static int all_zero(const unsigned char *at, uint64_t size) {
    for (uint64_t i = 0; i < size; ++i)
        if (at[i]) return 0;
    return 1;
}

static int has_imm32(const unsigned char *at, unsigned char opcode, uint32_t value) {
    return at[0] == opcode && !memcmp(at + 1, &value, 4);
}

static void set_env(const char *name, const char *value) {
#ifdef _WIN32
    _putenv_s(name, value ? value : "");
#else
    if (value) setenv(name, value, 1);
    else unsetenv(name);
#endif
}

static int unchanged(const unsigned char *image, const Patch *table, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const Patch *p = &table[i];
        if (!(p->original ? !memcmp(image + p->at, p->original, p->size) : all_zero(image + p->at, p->size))) return 0;
    }
    return 1;
}

static void write_patches(unsigned char *image, const Patch *table, size_t count) {
    for (size_t i = 0; i < count; ++i) memcpy(image + table[i].at, table[i].patched, table[i].size);
}

int runtime_resolution_parse(const char *text, uint32_t *width, uint32_t *height) {
    if (!text || !*text) return 0;
    char *end;
    const unsigned long w = strtoul(text, &end, 10);
    const unsigned long h = *end == 'x' ? strtoul(end + 1, &end, 10) : 0;
    if (*end || w > 16384 || h > 16384) return -1;
    if (w == BASE_WIDTH && h == BASE_HEIGHT) return 0;
    /* 16:9 or wider (a narrower screen would cut the 2D canvas), at least MIN_HEIGHT lines, and
     * within the heap's 32-bit size. */
    if (h < MIN_HEIGHT || w * 9 < h * 16 || runtime_resolution_heap_bytes((uint32_t)w, (uint32_t)h) > HEAP_MAX)
        return -1;
    *width = (uint32_t)w;
    *height = (uint32_t)h;
    return 1;
}

uint64_t runtime_resolution_heap_bytes(uint32_t width, uint32_t height) {
    const uint64_t pixels = (uint64_t)width * height, base = (uint64_t)BASE_WIDTH * BASE_HEIGHT;
    const uint64_t extra = pixels > base ? (pixels - base) * HEAP_BYTES_PER_PIXEL : 0;
    return HEAP_BYTES + (extra + HEAP_STEP - 1) / HEAP_STEP * HEAP_STEP;
}

int runtime_resolution_apply(unsigned char *image, uint64_t image_size) {
    /* A pool size this function set in an earlier launch comes back through an in-game restart:
     * it is not the caller's, and the new launch may use another size or none. */
    const char *pool = getenv("PS4RT_DMEM_MB"), *own_pool = getenv("PS4RT_SO_DMEM_MB");
    if (own_pool) {
        if (pool && !strcmp(pool, own_pool)) set_env("PS4RT_DMEM_MB", NULL);
        set_env("PS4RT_SO_DMEM_MB", NULL);
    }
    const char *request = getenv("PS4RT_SO_RESOLUTION");
    uint32_t width = 0, height = 0;
    const int wanted = runtime_resolution_parse(request, &width, &height);
    if (wanted < 0)
        fprintf(stderr, "Runtime: PS4RT_SO_RESOLUTION '%s' is not a supported <w>x<h> (16:9 or wider); using 1920x1080\n",
                request);
    if (wanted <= 0) return 0;

    const uint32_t heap = (uint32_t)HEAP_BYTES, heap_usable = (uint32_t)HEAP_BYTES - HEAP_HEADER;
    const float base_aspect = 16.0f / 9.0f;
    int matches = image_size >= WIDGETS_END && all_zero(image + OFF_WIDGETS, WIDGETS_END - OFF_WIDGETS) &&
                  has_imm32(image + OFF_SIZE, 0xba, BASE_WIDTH) && has_imm32(image + OFF_SIZE + 5, 0xb9, BASE_HEIGHT) &&
                  !memcmp(image + OFF_ASPECT, &base_aspect, 4) && has_imm32(image + OFF_HEAP_RESERVE, 0xba, heap) &&
                  has_imm32(image + OFF_HEAP_MAP, 0xbe, heap) && has_imm32(image + OFF_HEAP_META, 0xbf, heap_usable) &&
                  has_imm32(image + OFF_HEAP_INIT, 0xba, heap_usable);
    matches = matches && unchanged(image, size_patches, COUNT(size_patches)) &&
              unchanged(image, wide_patches, COUNT(wide_patches));
    if (!matches) {
        fputs("Runtime: internal resolution not applied (code differs from the reference build); using 1920x1080\n",
              stderr);
        return 0;
    }

    write_patches(image, size_patches, COUNT(size_patches));
    memcpy(image + OFF_SIZE + 1, &width, 4);
    memcpy(image + OFF_SIZE + 6, &height, 4);
    const float aspect = (float)width / (float)height;
    if ((uint64_t)width * 9 > (uint64_t)height * 16) {
        write_patches(image, wide_patches, COUNT(wide_patches));
        memcpy(image + OFF_ASPECT, &aspect, 4);
    }
    const uint64_t grown = runtime_resolution_heap_bytes(width, height);
    const uint32_t new_heap = (uint32_t)grown, new_usable = (uint32_t)grown - HEAP_HEADER;
    memcpy(image + OFF_HEAP_RESERVE + 1, &new_heap, 4);
    memcpy(image + OFF_HEAP_MAP + 1, &new_heap, 4);
    memcpy(image + OFF_HEAP_META + 1, &new_usable, 4);
    memcpy(image + OFF_HEAP_INIT + 1, &new_usable, 4);
    /* The pool grows by what the heap grew; an explicit PS4RT_DMEM_MB from the caller wins. */
    if (grown > HEAP_BYTES && !getenv("PS4RT_DMEM_MB")) {
        char pool_mb[16];
        snprintf(pool_mb, sizeof(pool_mb), "%u", POOL_MB + (unsigned)((grown - HEAP_BYTES) >> 20));
        set_env("PS4RT_DMEM_MB", pool_mb);
        set_env("PS4RT_SO_DMEM_MB", pool_mb);
    }
    const char *pool_now = getenv("PS4RT_DMEM_MB");
    printf("Runtime: internal resolution %ux%u (aspect %.4f), GPU heap %u MiB", width, height, aspect,
           (unsigned)(grown >> 20));
    if (pool_now) printf(", direct memory %s MiB", pool_now);
    putchar('\n');
    return 1;
}
