import struct
import unittest
from check_ac4_sky_audit import validate

class SkyEvidenceTests(unittest.TestCase):
    def test_native_band_evidence_is_separate(self):
        native='\nac4 native sky bands: 7 replaced, 0 declined'
        shadow='\nac4 sky band continuation: 0 matched, 0 mismatched'
        self.assertEqual(validate(self.report()+native+shadow,self.fixture(),require_native=True)['bands'],7)
        for bad in ('',native,native*2+shadow,native+shadow*2,
                    (native+shadow).replace('7 replaced','6 replaced'),
                    (native+shadow).replace('0 declined','1 declined'),
                    (native+shadow).replace('0 matched','7 matched'),
                    (native+shadow).replace('0 mismatched','1 mismatched')):
            with self.subTest(report=bad),self.assertRaises(ValueError):validate(self.report()+bad,self.fixture(),require_native=True)
        with self.assertRaises(ValueError):validate(self.report()+native+shadow,self.fixture(),require_native=True,require_continuation=True)
    def test_continuation_requires_complete_separate_evidence(self):
        good='\nac4 sky band continuation: 7 matched, 0 mismatched'
        self.assertEqual(validate(self.report()+good,self.fixture(),require_continuation=True)['bands'],7)
        for bad in ('',good*2,good.replace('7 matched','6 matched'),good.replace('0 mismatched','1 mismatched')):
            with self.subTest(report=bad),self.assertRaises(ValueError):
                validate(self.report()+bad,self.fixture(),require_continuation=True)
    def fixture(self):
        return struct.pack('<6I',0x31594b53,0x440,128,0x70000000,0x1000,0x1080)+bytes(0x440+128)
    def report(self,bands=7,matched=7,bad=0,declined=0,domes=1,hazes=0):
        return f'ac4 sky audit: {bands} bands, {matched} matched, {bad} mismatched, {declined} declined; {domes} domes, {hazes} hazes'
    def test_positive_dome_evidence_does_not_require_unseen_haze(self):
        self.assertEqual(validate(self.report(),self.fixture())['hazes'],0)
    def test_missing_duplicate_or_zero_coverage_fails(self):
        for text in ['',self.report()+self.report(),self.report(0,0),self.report(domes=0)]:
            with self.subTest(text=text),self.assertRaises(ValueError):validate(text,self.fixture())
    def test_incomplete_or_mismatched_bands_fail(self):
        for text in [self.report(matched=6),self.report(bad=1),self.report(declined=1)]:
            with self.subTest(text=text),self.assertRaises(ValueError):validate(text,self.fixture())
    def test_truncated_extra_or_invalid_fixture_fails(self):
        f=self.fixture()
        for b in [f[:20],f[:-1],f+b'x',b'bad!'+f[4:]]:
            with self.subTest(size=len(b)),self.assertRaises(ValueError):validate(self.report(),b)
    def test_wrapped_or_out_of_bounds_guest_ranges_fail(self):
        for offset,value in [(12,0x70003ff0),(16,0xfffffff0),(20,0x1070),(8,0)]:
            b=bytearray(self.fixture());struct.pack_into('<I',b,offset,value)
            with self.subTest(offset=offset),self.assertRaises(ValueError):validate(self.report(),b)

    def test_dome_plan_requires_exact_complete_coverage(self):
        report=self.report(domes=2)
        good='ac4 sky dome plan: 2 matched, 0 mismatched, 0 declined\n'
        self.assertEqual(validate(good+report,self.fixture(),True)['domes'],2)
        for prefix in ['',good+good,good.replace('2 matched','1 matched'),good.replace('0 mismatched','1 mismatched'),good.replace('0 declined','1 declined')]:
            with self.subTest(prefix=prefix),self.assertRaises(ValueError):validate(prefix+report,self.fixture(),True)

    def test_haze_plan_requires_positive_complete_coverage(self):
        report=self.report(hazes=2)
        good='ac4 sky haze plan: 2 matched, 0 mismatched, 0 declined\n'
        self.assertEqual(validate(good+report,self.fixture(),require_haze=True)['hazes'],2)
        for prefix in ['',good+good,good.replace('2 matched','1 matched'),good.replace('0 mismatched','1 mismatched'),good.replace('0 declined','1 declined')]:
            with self.subTest(prefix=prefix),self.assertRaises(ValueError):validate(prefix+report,self.fixture(),require_haze=True)
        with self.assertRaises(ValueError):
            validate(good.replace('2 matched','0 matched')+self.report(),self.fixture(),require_haze=True)
