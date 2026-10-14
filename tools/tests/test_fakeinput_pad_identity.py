"""Brief 007: per-slot pad identity in libfakeinput (host x86_64 round trip, no device)."""
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[2] / "app/src/main/cpp/fakeinput_steam.cpp"

PROGRAM = r"""
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
static int bit(const unsigned char *b, int n) { return (b[n / 8] >> (n % 8)) & 1; }
int main(void) {
  int fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
  if (fd < 0) { perror("open"); return 2; }
  struct input_id id; memset(&id, 0, sizeof(id));
  if (ioctl(fd, EVIOCGID, &id) < 0) { perror("EVIOCGID"); return 3; }
  char name[128] = {0};
  if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) { perror("EVIOCGNAME"); return 4; }
  unsigned char keys[KEY_MAX / 8 + 1]; memset(keys, 0, sizeof(keys));
  if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0) { perror("EVIOCGBIT"); return 5; }
  printf("%04x:%04x bus=%d tl2=%d tr2=%d south=%d name=%s\n", id.vendor, id.product, id.bustype,
         bit(keys, BTN_TL2), bit(keys, BTN_TR2), bit(keys, BTN_SOUTH), name);
  close(fd);
  return 0;
}
"""

RING_EVENTS = 4096
EVENT_SIZE = 24  # struct input_event on x86_64


@unittest.skipUnless(sys.platform == "linux" and shutil.which("gcc") and shutil.which("g++")
                     and struct.calcsize("P") == 8, "needs 64-bit Linux, gcc and g++")
class FakeinputPadIdentityTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.path = Path(cls.tmp.name)
        (cls.path / "program.c").write_text(PROGRAM)
        subprocess.run(["gcc", "-o", str(cls.path / "program"), str(cls.path / "program.c")], check=True)
        subprocess.run(["g++", "-shared", "-fPIC", "-O2", "-Wno-attributes", "-pthread", "-std=c++17",
                        "-static-libstdc++", "-static-libgcc", "-Wl,--exclude-libs,ALL",
                        "-o", str(cls.path / "libfakeinput.so"), str(SOURCE), "-ldl"], check=True)

    def run_slot(self, identity, profiles):
        d = Path(tempfile.mkdtemp(dir=self.path))
        (d / "event0").touch()
        ring = d / "ring0"
        header = struct.pack("<IIIIQQ", 0x46494252, 2, EVENT_SIZE, RING_EVENTS, 0, 1).ljust(64, b"\0")
        ring.write_bytes(header + b"\0" * (RING_EVENTS * EVENT_SIZE))
        if identity is not None:
            (d / "identity0").write_text(identity + "\n")
        env = dict(os.environ, LD_PRELOAD=str(self.path / "libfakeinput.so"), FAKE_EVDEV_DIR=str(d),
                   FAKE_EVDEV_MEMFD_PATHS="0=" + str(ring), FAKE_EVDEV_IDENTITY="xbox360")
        if profiles:
            env["FAKE_EVDEV_PAD_PROFILES"] = "1"
        out = subprocess.run([str(self.path / "program")], env=env, check=True, timeout=20,
                             capture_output=True, text=True)
        return out.stdout.strip()

    def test_dualsense_slot_presents_sony_ids_name_and_l2_r2(self):
        self.assertEqual(self.run_slot("ps5", True),
                         "054c:0ce6 bus=5 tl2=1 tr2=1 south=1 name=Sony Interactive Entertainment DualSense Wireless Controller")

    def test_xbox_series_slot_keeps_xpad_buttons(self):
        self.assertEqual(self.run_slot("xboxone", True),
                         "045e:0b12 bus=5 tl2=0 tr2=0 south=1 name=Microsoft Xbox Series S|X Controller")

    def test_default_and_opt_out_stay_xbox360(self):
        x360 = "045e:028e bus=3 tl2=0 tr2=0 south=1 name=Xbox 360 Controller (0)"
        self.assertEqual(self.run_slot("ps5", False), x360)
        self.assertEqual(self.run_slot("xbox360", True), x360)
        self.assertEqual(self.run_slot(None, True), x360)


if __name__ == "__main__":
    unittest.main()
