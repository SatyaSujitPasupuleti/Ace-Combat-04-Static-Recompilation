import unittest
from check_ac4_cloud_audit import validate


class CloudEvidenceTests(unittest.TestCase):
    coverage='ac4 cloud audit: 0 sprite projections, 9 plane vertices, 7 non-rejected plane vertices\n'
    plan='ac4 cloud point plan: 9 matched, 0 mismatched, 0 declined\n'

    def test_native_rows_are_separate_from_original_comparisons(self):
        base=self.coverage+self.plan+'ac4 cloud sprite rows: 3 calls, 3 matched, 0 mismatched, 0 declined, 9 pieces\n'
        native='ac4 native rows: 3 replaced, 0 declined\n'
        shadow='ac4 cloud row continuation: 0 matched, 0 mismatched\n'
        self.assertEqual(validate(base+native+shadow,require_native_rows=True)['row_calls'],3)
        for bad in ('',native,native+shadow*2,native*2+shadow,
                    (native+shadow).replace('3 replaced','2 replaced'),
                    (native+shadow).replace('0 declined','1 declined'),
                    (native+shadow).replace('0 matched','3 matched'),
                    (native+shadow).replace('0 mismatched','1 mismatched')):
            with self.subTest(report=bad),self.assertRaises(ValueError):validate(base+bad,require_native_rows=True)
        with self.assertRaises(ValueError):validate(base+native+shadow,require_native_rows=True,require_continuation=True)

    def test_row_continuation_requires_every_call(self):
        base=self.coverage+self.plan+'ac4 cloud sprite rows: 3 calls, 3 matched, 0 mismatched, 0 declined, 9 pieces\n'
        good='ac4 cloud row continuation: 3 matched, 0 mismatched\n'
        self.assertEqual(validate(base+good,require_continuation=True)['row_calls'],3)
        for report in ('',good*2,good.replace('3 matched','2 matched'),good.replace('0 mismatched','1 mismatched')):
            with self.subTest(report=report),self.assertRaises(ValueError):validate(base+report,require_continuation=True)

    def test_sprite_mapping_requires_all_rows(self):
        base=self.coverage+self.plan+'ac4 cloud sprite rows: 3 calls, 3 matched, 0 mismatched, 0 declined, 9 pieces\n'
        good='ac4 cloud sprite mapping: 3 mapped, 0 unmapped rows\n'
        self.assertEqual(validate(base+good,require_mapping=True)['row_calls'],3)
        for report in ('',good*2,good.replace('3 mapped','2 mapped'),good.replace('0 unmapped','1 unmapped')):
            with self.subTest(report=report),self.assertRaises(ValueError):validate(base+report,require_mapping=True)

    def test_sprite_rows_require_complete_positive_packet_coverage(self):
        base=self.coverage+self.plan
        good='ac4 cloud sprite rows: 3 calls, 3 matched, 0 mismatched, 0 declined, 9 pieces\n'
        self.assertEqual(validate(base+good,require_rows=True)['sprite_pieces'],9)
        for report in ('',good*2,good.replace('3 matched','2 matched'),good.replace('0 mismatched','1 mismatched'),good.replace('0 declined','1 declined'),good.replace('9 pieces','0 pieces'),good.replace('9 pieces','193 pieces'),good.replace('3 calls','0 calls')):
            with self.subTest(report=report),self.assertRaises(ValueError):validate(base+report,require_rows=True)

    def test_sprite_projection_requires_every_call(self):
        base=self.coverage.replace('0 sprite','8 sprite')+self.plan
        good='ac4 cloud sprite plan: 8 matched, 0 mismatched, 0 declined\n'
        self.assertEqual(validate(base+good,require_sprites=True)['sprite_calls'],8)
        for report in ('',good*2,good.replace('8 matched','7 matched'),good.replace('0 mismatched','1 mismatched'),good.replace('0 declined','1 declined')):
            with self.subTest(report=report),self.assertRaises(ValueError):validate(base+report,require_sprites=True)
        with self.assertRaises(ValueError):validate(self.coverage+self.plan+good,require_sprites=True)

    def test_packet_mapping_requires_complete_positive_coverage(self):
        good='ac4 cloud packets: 2 batches, 2 mapped, 0 declined, 4 quads\n'
        self.assertEqual(validate(self.coverage+self.plan+good,True)['quads'],4)
        for report in ('',good*2,good.replace('2 mapped','1 mapped'),good.replace('0 declined','1 declined'),
                       good.replace('4 quads','0 quads'),good.replace('2 batches','0 batches')):
            with self.subTest(report=report),self.assertRaises(ValueError):validate(self.coverage+self.plan+report,True)

    def test_exact_plane_coverage_does_not_claim_sprites(self):
        self.assertEqual(validate(self.coverage+self.plan),dict(sprite_calls=0,plane_calls=9,non_rejected=7))

    def test_missing_duplicate_partial_or_failed_reports_reject(self):
        for text in ('',self.coverage,self.plan,self.coverage*2+self.plan,self.coverage+self.plan*2,
                     self.coverage+self.plan.replace('9 matched','8 matched'),
                     self.coverage+self.plan.replace('0 mismatched','1 mismatched'),
                     self.coverage+self.plan.replace('0 declined','1 declined'),
                     (self.coverage+self.plan).replace('9 plane','0 plane'),
                     (self.coverage+self.plan).replace('7 non-rejected','0 non-rejected'),
                     (self.coverage+self.plan).replace('7 non-rejected','10 non-rejected')):
            with self.subTest(text=text),self.assertRaises(ValueError):validate(text)
