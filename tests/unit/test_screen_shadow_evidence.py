import unittest
from check_ac4_screen_shadow import validate


class ScreenShadowEvidenceTests(unittest.TestCase):
    good = 'ac4 screen sprite shadow: 8 calls, 8 matched, 0 mismatched, 0 declined, 0 unpaired, 0 open\n'

    def test_complete_original_packet_evidence(self):
        self.assertEqual(validate(self.good), dict(calls=8, matched=8))

    def test_incomplete_or_failed_reports_reject(self):
        reports = ['', self.good * 2, self.good.replace('8 calls', '0 calls'),
                   self.good.replace('8 matched', '7 matched'),
                   self.good.replace('8', '0'), self.good.replace('0 open', '0 open extra')]
        reports += [self.good.replace('0 ' + field, '1 ' + field)
                    for field in ('mismatched', 'declined', 'unpaired', 'open')]
        for report in reports:
            with self.subTest(report=report), self.assertRaises(ValueError):
                validate(report)
