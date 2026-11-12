// Mac-native check of the eye_size conf key and the eye/preview size selection (runtime helpers only)
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
static int ViewportWidth = 1280, ViewportHeight = 720;
static int DirectEyeWidth = 0, DirectEyeHeight = 0;
static int ConfEyeWidth = 0, ConfEyeHeight = 0;
static bool monoRendering = false;
#include "eye-size-fixture.inc"

static void parses(const char* line, int w, int h) {
  int pw = 1, ph = 1;
  assert(parseEyeSize(line, pw, ph) && pw == w && ph == h);
}
static void rejects(const char* line) {
  int pw = 1, ph = 1;
  assert(!parseEyeSize(line, pw, ph) && pw == 1 && ph == 1);
}
static void eye(bool direct, int w, int h) {
  int ew = 0, eh = 0;
  recommendedEyeSize(direct, ew, eh);
  assert(ew == w && eh == h);
}
static void preview(uint32_t ew, uint32_t eh, uint32_t w, uint32_t h) {
  uint32_t pw = 0, ph = 0;
  vulkanPreviewSize(ew, eh, pw, ph);
  assert(pw == w && ph == h);
}

int main() {
  parses("eye_size=480x540", 480, 540);
  parses("eye_size= 480x540 \r", 480, 540);
  parses("eye_size=64x64", 64, 64);
  parses("eye_size=+00064x +4096\t", 64, 4096);
  parses("eye_size=4096x4096", 4096, 4096);
  for (const char* bad : {"eye_size=481x540", "eye_size=480x541", "eye_size=62x540", "eye_size=480x4098",
                          "eye_size=-480x540", "eye_size=480", "eye_size=480x540x2", "eye_size=480x540p",
                          "eye_size=4294967776x540", "eye_size=480x4294967836",
                          "eye_size=999999999999999999999999999999999999x540",
                          "eye_size=", "eye_size=abc", "eye_size 480x540"})
    rejects(bad);
  puts("PASS eye_size parsing: valid sizes accepted, odd/out-of-range/malformed rejected without side effects");

  // Key absent: unchanged half-screen size, the whole screen in mono, direct transport first
  eye(false, 640, 720);
  monoRendering = true; eye(false, 1280, 720); monoRendering = false;
  DirectEyeWidth = 1832; DirectEyeHeight = 1920; eye(true, 1832, 1920);
  preview(640, 720, 1280, 720);
  preview(480, 540, 960, 540);
  puts("PASS key absent: eye 640x720 (mono 1280x720), direct 1832x1920, preview 2*eye x eye");

  ConfEyeWidth = 480; ConfEyeHeight = 540;
  eye(false, 480, 540);
  monoRendering = true; eye(false, 480, 540); monoRendering = false;
  eye(true, 1832, 1920);
  preview(480, 540, 1280, 720);
  preview(640, 720, 1280, 720);
  puts("PASS eye_size=480x540: eye 480x540 (direct still wins), Vulkan preview stays 1280x720");
  return 0;
}
