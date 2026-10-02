import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(__file__))
import mkfont  # noqa: E402


class FontTest(unittest.TestCase):
    def setUp(self):
        self.g = mkfont.build()

    def test_256_glyphs_of_8_hex_rows(self):
        self.assertEqual(len(self.g), 256)
        for code, glyph in enumerate(self.g):
            self.assertEqual(len(glyph), 8, hex(code))
            int(glyph, 16)

    def test_printable_ascii_leaves_gap_column(self):
        for code in range(0x21, 0x7E):  # '~' (0x7E) deliberately uses 4 columns
            for row in self.g[code]:
                self.assertEqual(int(row, 16) & 1, 0, chr(code))

    def test_horizontal_lines_span_cell(self):
        self.assertEqual(self.g[0xC4], "000F0000")  # single
        self.assertEqual(self.g[0xCD], "00F0F000")  # double

    def test_vertical_lines_span_cell(self):
        self.assertEqual(self.g[0xB3], "44444444")  # single
        self.assertEqual(self.g[0xBA], "AAAAAAAA")  # double

    def test_cross_joins_all_four_sides(self):
        cross = self.g[0xC5]
        self.assertEqual(cross[0], "4")
        self.assertEqual(cross[7], "4")
        self.assertEqual(cross[3], "F")

    def test_blocks_and_shades(self):
        self.assertEqual(self.g[0xDB], "FFFFFFFF")
        self.assertEqual(self.g[0xDC], "0000FFFF")
        self.assertEqual(self.g[0xDF], "FFFF0000")
        self.assertEqual(self.g[0xB1], "A5A5A5A5")

    def test_composed_accents_keep_base_letter(self):
        self.assertEqual(self.g[0x84], "0A0" + self.g[ord("a")][3:])  # a umlaut
        self.assertEqual(self.g[0x94], "0A0" + self.g[ord("o")][3:])  # o umlaut
        self.assertEqual(self.g[0x86], "4A4" + self.g[ord("a")][3:])  # a ring

    def test_pair_encoding(self):
        self.assertEqual(mkfont.to_pairs("8" * 8)[:2], [0x10, 0x00])
        self.assertEqual(mkfont.to_pairs("F" * 8)[:2], [0x11, 0x11])
        self.assertEqual(len(mkfont.to_pairs("0" * 8)), 16)

    def test_c_output(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "font_data.c")
            mkfont.write_c(self.g, path)
            text = open(path).read()
        self.assertIn("const uint8_t font_data[256 * 16]", text)
        self.assertEqual(text.count("0x"), 256 * 16)


if __name__ == "__main__":
    unittest.main()
