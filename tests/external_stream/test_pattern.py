"""CPU-only demonstration validation; standard-library Python, including 3.7."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "examples/external_stream"))
from pattern import Pattern


class PatternTests(unittest.TestCase):
    def test_formats_encode_same_colors_and_alpha(self):
        rgba = Pattern(160, 90, "rgba")
        bgra = Pattern(160, 90, "bgra")
        rgba.update(123)
        bgra.update(123)
        for offset in range(0, len(rgba.buffer), 4):
            red, green, blue, alpha = rgba.buffer[offset:offset + 4]
            self.assertEqual(bytes(bgra.buffer[offset:offset + 4]), bytes((blue, green, red, alpha)))

    def test_frames_reuse_buffer_and_remain_complete_at_small_sizes(self):
        for width, height in ((1, 1), (7, 5), (160, 90)):
            pattern = Pattern(width, height, "rgba")
            native = pattern.update(17)
            expected = bytes(pattern.buffer)
            self.assertIs(pattern.update(999999), native)
            pattern.update(17)
            self.assertEqual(bytes(pattern.buffer), expected)
            self.assertEqual(len(pattern.buffer), width * height * 4)

    def test_rejects_unbounded_dimensions_and_unknown_format(self):
        for width, height, format_name in ((0, 1, "rgba"), (1921, 1080, "rgba"),
                                           (1, 1081, "bgra"), (10, 10, "rgb")):
            with self.assertRaises(ValueError):
                Pattern(width, height, format_name)


if __name__ == "__main__":
    unittest.main()
