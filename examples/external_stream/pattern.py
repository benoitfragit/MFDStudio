"""Deterministic CPU test pattern: top marker, moving bar, alpha and frame bits."""
import ctypes


class Pattern:
    """Reuse one tightly packed byte buffer; generation needs no third-party package."""

    DIGITS = ((7, 5, 5, 5, 7), (2, 6, 2, 2, 7), (7, 1, 7, 4, 7),
              (7, 1, 7, 1, 7), (5, 5, 7, 1, 1), (7, 4, 7, 1, 7),
              (7, 4, 7, 5, 7), (7, 1, 1, 1, 1), (7, 5, 7, 5, 7),
              (7, 5, 7, 1, 7))

    def __init__(self, width, height, pixel_format, phase=0):
        if not (1 <= width <= 1920 and 1 <= height <= 1080):
            raise ValueError("Resolution must be within 1..1920 x 1..1080")
        if pixel_format not in ("rgba", "bgra"):
            raise ValueError("Pixel format must be rgba or bgra")
        self.width, self.height, self.phase = width, height, phase
        self.pixel_format = pixel_format
        colors = [(225, 65, 65), (240, 190, 45), (60, 210, 120),
                  (45, 180, 235), (100, 90, 230), (215, 75, 205)]
        row = b"".join(self.pixel(*(colors[min(5, x * 6 // width)]), 255)
                       for x in range(width))
        self.background = row * height
        self.buffer = bytearray(self.background)
        self.native = (ctypes.c_ubyte * len(self.buffer)).from_buffer(self.buffer)

    def pixel(self, red, green, blue, alpha):
        """Encode one straight-alpha pixel in the negotiated byte order."""
        return bytes((red, green, blue, alpha) if self.pixel_format == "rgba"
                     else (blue, green, red, alpha))

    def rectangle(self, x, y, width, height, color):
        """Clip and fill a rectangle without allocating a full-frame temporary."""
        right, bottom = min(self.width, x + width), min(self.height, y + height)
        x, y = max(0, x), max(0, y)
        if right <= x or bottom <= y:
            return
        row = self.pixel(*color) * (right - x)
        for line in range(y, bottom):
            offset = (line * self.width + x) * 4
            self.buffer[offset:offset + len(row)] = row

    def update(self, frame):
        """Render an identifiable complete frame; top-left marker is white."""
        self.buffer[:] = self.background
        x = (frame * 3 + self.phase) % self.width
        self.rectangle(x, 0, max(1, self.width // 40), self.height, (255, 255, 255, 255))
        self.rectangle(0, 0, max(1, self.width // 16), max(1, self.height // 12), (255, 255, 255, 255))
        self.rectangle(0, self.height * 3 // 4, self.width, self.height // 4, (0, 0, 0, 100))
        bit_width = max(1, self.width // 36)
        for bit in range(32):
            color = (255, 255, 255, 255) if frame & (1 << bit) else (0, 0, 0, 255)
            self.rectangle((bit + 2) * bit_width, self.height // 12, bit_width,
                           max(1, self.height // 12), color)
        self.counter(frame)
        return self.native

    def counter(self, frame):
        """Draw six decimal digits so a paused frame is immediately identifiable."""
        scale = max(1, min(self.width // 120, self.height // 50))
        origin_x, origin_y = 2 * scale, self.height // 4
        self.rectangle(origin_x - scale, origin_y - scale, 25 * scale, 7 * scale, (0, 0, 0, 255))
        for index, digit in enumerate(str(frame % 1000000).zfill(6)):
            for row, bits in enumerate(self.DIGITS[int(digit)]):
                for column in range(3):
                    if bits & (1 << (2 - column)):
                        self.rectangle(origin_x + (index * 4 + column) * scale,
                                       origin_y + row * scale, scale, scale, (255, 255, 255, 255))
