import unittest
from test_ac4_sky_gpu import validate_sky_report


class SkyGpuEvidenceTests(unittest.TestCase):
    good = 'ac4 native sky: 8 intents, 1440 raw GPU triangles, 0 rejected, 0 unused triangles'

    def test_sprite_gif_needs_measured_bounded_coverage(self):
        report='ac4 sprite GIF packets: 3 packets, 27 qwords bypassed generic parsing\n'
        self.assertEqual(validate_sky_report(report+self.good,require_sprite_gif=True),(8,1440))
        for bad in ('',report*2,report.replace('3 packets','0 packets'),
                    report.replace('27 qwords','8 qwords'),report.replace('27 qwords','388 qwords'),
                    report.replace('27 qwords','26 qwords')):
            with self.subTest(report=bad),self.assertRaises(ValueError):
                validate_sky_report(bad+self.good,require_sprite_gif=True)

    def test_sprites_need_separate_complete_gpu_coverage(self):
        sprites='ac4 native sprites: 3 intents, 12 raw GPU pieces, 0 rejected, 0 unused pieces\n'
        self.assertEqual(validate_sky_report(sprites+self.good,require_sprites=True),(8,1440))
        for report in ('',sprites*2,sprites.replace('3 intents','0 intents'),sprites.replace('12 raw','0 raw'),
                       sprites.replace('0 rejected','1 rejected'),sprites.replace('0 unused','1 unused')):
            with self.subTest(report=report),self.assertRaises(ValueError):
                validate_sky_report(report+self.good,require_sprites=True)

    def test_clouds_need_their_own_complete_coverage(self):
        clouds='ac4 native clouds: 3 intents, 800 raw GPU triangles, 0 rejected, 0 unused triangles\n'
        self.assertEqual(validate_sky_report(clouds+self.good,require_clouds=True),(8,1440))
        for report in ('',clouds*2,clouds.replace('3 intents','0 intents'),clouds.replace('800 raw','0 raw'),
                       clouds.replace('0 rejected','1 rejected'),clouds.replace('0 unused','1 unused')):
            with self.subTest(report=report),self.assertRaises(ValueError):
                validate_sky_report(report+self.good,require_clouds=True)

    def test_haze_requires_positive_consistent_single_report(self):
        haze='ac4 native haze: 3 intents, 800 raw GPU triangles\n'
        self.assertEqual(validate_sky_report(haze+self.good, True),(8,1440))
        for report in ('', haze+haze, haze.replace('3 intents','0 intents'),
                       haze.replace('800 raw','0 raw'), haze.replace('3 intents','9 intents'),
                       haze.replace('800 raw','1441 raw')):
            with self.subTest(report=report),self.assertRaises(ValueError):
                validate_sky_report(report+self.good,True)

    def test_complete_positive_coverage(self):
        self.assertEqual(validate_sky_report(self.good), (8, 1440))

    def test_missing_duplicate_or_zero_coverage(self):
        for report in ('', self.good + '\n' + self.good,
                       self.good.replace('8 intents', '0 intents'),
                       self.good.replace('1440 raw', '0 raw')):
            with self.subTest(report=report), self.assertRaises(ValueError):
                validate_sky_report(report)

    def test_fallback_is_not_full_coverage(self):
        for report in (self.good.replace('0 rejected', '1 rejected'),
                       self.good.replace('0 unused', '1 unused')):
            with self.subTest(report=report), self.assertRaises(ValueError):
                validate_sky_report(report)
