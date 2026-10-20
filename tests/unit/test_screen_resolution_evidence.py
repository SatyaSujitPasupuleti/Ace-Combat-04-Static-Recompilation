import unittest
from test_ac4_screen_resolution import validate_resolution


def ppm(w, h):
    return f'P6\n{w} {h}\n255\n'.encode() + bytes([0, 0, 0, 255, 255, 255]) * (w*h//2)


class ScreenResolutionEvidenceTests(unittest.TestCase):
    report = ('rn: 2D layer -- 8 elements (8 accepted whole), 8 primitives: 8 accepted, 0 kept emulated; 0 elements not closed by their end record\n'
              'vk: 2D overlay composed on 1 frames at 2x, 8 draws lifted onto it\n'
              'vk: AC4 pre-display overlay composed on 1 transfers\n'
              'vk: AC4 native display: 1 copies, 1 presented, 2x at 8x4\n')

    def test_output_and_actual_target_must_agree(self):
        self.assertEqual(validate_resolution(ppm(2,2), ppm(4,4), self.report, 4)['scale'], 2)
        self.assertEqual(validate_resolution(ppm(2,2), ppm(4,4), self.report, 9, 2)['scale'], 2)
        for image, report in ((ppm(2,2), self.report),
                              (ppm(4,4), self.report.replace('8x4', '8x2')),
                              (ppm(4,4), self.report.replace('2x at', '3x at')),
                              (ppm(4,4), self.report.replace('1 presented', '0 presented'))):
            with self.subTest(report=report), self.assertRaises(ValueError):
                validate_resolution(ppm(2,2), image, report, 4)
