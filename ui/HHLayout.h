// SPDX-License-Identifier: AGPL-3.0-or-later
// Haunted Hall: screen layout at KICK's 1024 x 600, measured from the art.
// Generated from layout.py; edit there and regenerate, so code and previews agree.
#pragma once

namespace mt::hauntedhall::ui {

struct Rect { int x, y, w, h; };
struct Knob { int cx, cy, well, arc; };   // centre, well radius (cap), radius of the lit tick ring

// Palette (from the parts sheet), 0xRRGGBB
namespace colour {
constexpr unsigned kMint = 0x84fbbc;
constexpr unsigned kMintHi = 0xe4ffe9;
constexpr unsigned kAccent = 0x9dffcc;
constexpr unsigned kSoft = 0x3b705d;
constexpr unsigned kSoftEdge = 0x84ac93;
constexpr unsigned kDimTick = 0x2c4a3e;
constexpr unsigned kLavender = 0xebc8f0;
constexpr unsigned kLavenderMid = 0xd9a7e4;
constexpr unsigned kKnobCap = 0x977b9a;
constexpr unsigned kFaderCap = 0xc5aacd;
constexpr unsigned kFaderLine = 0x785d87;
constexpr unsigned kDark = 0x161422;
constexpr unsigned kText = 0xe1c5e8;
constexpr unsigned kDarkText = 0x101b1b;
constexpr unsigned kMeterYellow = 0xf0dc78;
constexpr unsigned kMeterRed = 0xff6e8c;
constexpr unsigned kWindow = 0x0e0a12;
} // namespace colour

namespace master {
constexpr Rect kOn[4] = {{255, 152, 49, 35}, {255, 244, 49, 35}, {255, 336, 49, 35}, {255, 429, 49, 35}};
constexpr Rect kMute[4] = {{317, 152, 39, 35}, {317, 244, 39, 35}, {317, 336, 39, 35}, {317, 429, 39, 35}};
constexpr Rect kMeter[4] = {{392, 173, 147, 13}, {392, 261, 147, 13}, {392, 353, 147, 13}, {392, 446, 147, 13}};   // inner area
constexpr int kMeterSegments = 12;
constexpr int kFaderX[4] = {650, 746, 839, 936};
constexpr int kFaderTop = 160, kFaderBottom = 266;   // cap centre travel
constexpr int kFaderCapW = 26, kFaderCapH = 12;
constexpr int kBypassLedX = 650, kBypassLedY = 366, kBypassLedR = 7;
constexpr Rect kBypass = {618, 352, 64, 45};
constexpr Rect kTimeDisplay = {726, 357, 132, 33};
constexpr Knob kKnobs[4] = {{646, 440, 21, 31}, {741, 440, 21, 31}, {836, 439, 21, 31}, {931, 440, 21, 31}};   // TIME, FEEDBACK, MIX, LEVEL
constexpr Rect kTabMaster = {207, 528, 301, 48}, kTabDrum = {518, 528, 300, 48};
constexpr int kGhostX = 905, kGhostY = 18;
} // namespace master

namespace step {
constexpr int kColX[16] = {202, 252, 302, 352, 402, 452, 501, 551, 601, 651, 701, 751, 801, 851, 901, 951};   // left edge of each step column
constexpr int kRowY[5] = {204, 244, 284, 324, 364};    // KICK, SNARE, HAT, GHOST, FX
constexpr int kCellW = 45, kCellH = 32;
constexpr int kNumberY = 166, kNumberH = 31;   // step-number buttons (playhead)
constexpr int kBarStripY = 151, kBarStripH = 11;     // bar slots 1-16, one per column
constexpr Rect kBarSelector = {32, 162, 160, 34};   // '< BAR 01/08 >'
constexpr Rect kBars816 = {567, 130, 158, 20};           // left half 8 BARS, right half 16 BARS
constexpr Rect kPatternAB = {733, 130, 106, 20};       // left half A, right half B
constexpr Rect kRowLabel[5] = {{28, 203, 165, 33}, {28, 243, 165, 33}, {28, 283, 165, 33}, {28, 323, 165, 33}, {28, 363, 165, 33}};
constexpr Rect kRowLock[5] = {{34, 206, 14, 14}, {34, 246, 14, 14}, {34, 286, 14, 14}, {34, 326, 14, 14}, {34, 366, 14, 14}};
constexpr Rect kMelodyStrip = {200, 398, 795, 6};
constexpr Knob kKnobs[4] = {{264, 446, 26, 37}, {449, 446, 26, 37}, {647, 446, 26, 37}, {841, 446, 26, 37}};   // DECAY, TONE, SPOOK, SPACE
constexpr Rect kRandom = {45, 525, 177, 51}, kClear = {235, 525, 176, 51}, kCopy = {424, 525, 176, 51},
               kPaste = {614, 525, 176, 51}, kUndo = {803, 525, 176, 51};
constexpr Rect kDecryptBar = {810, 14, 198, 30};
constexpr int kBatX = 128, kBatY = 40;
} // namespace step

namespace browser {
constexpr Rect kTabs[4] = {{311, 16, 148, 57}, {483, 16, 147, 57}, {654, 16, 144, 57}, {820, 16, 147, 57}};   // DRUM, BASS, LEAD, PAD
constexpr Rect kBankPrev = {416, 92, 51, 32}, kBankName = {470, 92, 341, 32}, kBankNext = {814, 92, 51, 32};
constexpr Rect kPresetSlots[10] = {{339, 141, 247, 34}, {339, 181, 247, 34}, {339, 222, 247, 34}, {339, 262, 247, 34}, {339, 303, 247, 34}, {704, 141, 248, 34}, {704, 181, 248, 34}, {704, 222, 248, 34}, {704, 262, 248, 34}, {704, 303, 248, 34}};   // left column 1-5, right 6-10
constexpr Rect kRandom = {600, 180, 90, 95}, kFavorite = {600, 302, 90, 35};
constexpr Rect kCartWindow = {87, 142, 178, 204};
constexpr Rect kCartArt = {87, 148, 178, 192};   // 96 x 96 art drawn at 2x, columns 3-91 (3-4 px trimmed each side)
constexpr Knob kKnobs[4] = {{424, 392, 24, 36}, {569, 392, 24, 36}, {717, 393, 24, 36}, {866, 392, 24, 36}};   // DECAY, TONE, SPOOK, SPACE
constexpr Rect kCarouselPrev = {130, 487, 31, 43}, kCarouselNext = {617, 487, 31, 43};
constexpr Rect kCarouselSlots[6] = {{175, 480, 66, 65}, {247, 480, 65, 65}, {319, 480, 65, 65}, {391, 480, 65, 65}, {463, 480, 65, 65}, {535, 480, 66, 65}};   // 48 x 48 thumbnails, centred
constexpr Rect kLoadPreset = {676, 470, 259, 46}, kBack = {676, 526, 259, 46};
} // namespace browser

} // namespace mt::hauntedhall::ui
