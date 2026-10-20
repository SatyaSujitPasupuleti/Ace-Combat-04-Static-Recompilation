import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from test_ac4_cockpit_gpu import validate_cockpit, validate_native_cockpit


class CockpitEvidenceTests(unittest.TestCase):
    report = 'ac4 cockpit audit: 10 activations, 8 packets, 24 vertices, 24 exact, 0 xyz / 0 fog / 0 uv / 0 color mismatches, 2 skipped, 0 rejected'

    def test_exact_subset_keeps_skips_visible(self):
        self.assertEqual(validate_cockpit(self.report)['skipped'], 2)
        self.assertEqual(validate_cockpit(self.report+'\nac4 cockpit GPU: 6 draws', True)['gpu_draws'], 6)

    def test_native_requires_original_coverage(self):
        coverage = validate_cockpit(self.report)
        report = 'ac4 cockpit native: 10 activations, 10 packets, 3060 original instructions replaced'
        self.assertEqual(validate_native_cockpit(report, coverage, coverage), 3060)
        for text in ('', report+'\n'+report, report.replace('10 activations','9 activations'),
                     report.replace('10 packets','9 packets'), report.replace('3060','0')):
            with self.assertRaises(ValueError):
                validate_native_cockpit(text, coverage, coverage)
        for key in ('activations', 'packets', 'vertices', 'skipped'):
            baseline = dict(coverage);baseline[key]+=1
            with self.assertRaises(ValueError):
                validate_native_cockpit(report, coverage, baseline)

    def test_gif_requires_cockpit_specific_coverage(self):
        coverage = validate_cockpit(self.report)
        report = 'ac4 cockpit native: 10 activations, 10 packets, 3060 original instructions replaced'
        gif = 'ac4 cockpit GIF packets: 8 packets, 192 qwords bypassed generic parsing'
        validate_native_cockpit(report+'\n'+gif, coverage, coverage, True)
        for suffix in ('', gif+'\n'+gif, gif.replace('8 packets','0 packets'),
                       gif.replace('8 packets','11 packets'), gif.replace('192 qwords','0 qwords'),
                       gif.replace('192 qwords','9000 qwords'), gif.replace('cockpit GIF','mesh GIF')):
            with self.assertRaises(ValueError):
                validate_native_cockpit(report+'\n'+suffix,coverage,coverage,True)

    def test_state_plan_requires_complete_independent_coverage(self):
        memory = 'ac4 cockpit memory: 10 begun, 10 ended, 10 packets, 0 kick / 0 end mismatches, 0 sequence failures, 0 declined'
        integers = 'ac4 cockpit integers: 10 packets, 10 ended, 0 kick / 0 end mismatches, 0 declined'
        scalars = integers.replace('integers:', 'scalars:')
        vectors = integers.replace('integers:', 'vectors:')
        cycles = 'ac4 cockpit cycles: 10 ended, 0 mismatches, 0 declined'
        flags = 'ac4 cockpit flags: 10 ended, 0 mismatches'
        report = self.report+'\n'+memory+'\n'+integers+'\n'+scalars+'\n'+vectors+'\n'+cycles+'\n'+flags
        validate_cockpit(report, state_plan=True)
        bad = [self.report, report+'\n'+memory, report+'\n'+integers]
        for original, replacement in [('10 begun','9 begun'), ('10 ended','9 ended'),
                ('10 packets','9 packets'), ('0 kick','1 kick'), ('0 end mismatches','1 end mismatches'),
                ('0 sequence','1 sequence'), ('0 declined','1 declined')]:
            bad.append(report.replace(original, replacement, 1))
        for token in ('10 packets', '10 ended', '0 kick', '0 end mismatches', '0 declined'):
            for row in (integers, scalars, vectors):
                bad.append(report.replace(row, row.replace(token, '1 '+token.split(' ',1)[1])))
        for row in (integers, scalars, vectors):
            bad.extend([report+'\n'+row, report.replace('\n'+row, '')])
        bad.extend([report+'\n'+cycles, report.replace('\n'+cycles, '')])
        for a,b in [('10 ended','9 ended'),('0 mismatches','1 mismatches'),('0 declined','1 declined')]:
            bad.append(report.replace(cycles,cycles.replace(a,b)))
        bad.extend([report+'\n'+flags,report.replace('\n'+flags,'')])
        for a,b in [('10 ended','9 ended'),('0 mismatches','1 mismatches')]:
            bad.append(report.replace(flags,flags.replace(a,b)))
        for text in bad:
            with self.subTest(report=text), self.assertRaises(ValueError):
                validate_cockpit(text, state_plan=True)

    def test_rejects_missing_mismatched_and_false_coverage(self):
        for text in ('', self.report+'\n'+self.report,
                     self.report.replace('24 exact', '23 exact'),
                     self.report.replace('0 uv', '1 uv'),
                     self.report.replace('0 rejected', '1 rejected'),
                     self.report.replace('24 vertices, 24 exact', '0 vertices, 0 exact')):
            with self.assertRaises(ValueError):
                validate_cockpit(text)
        for suffix in ('', '\nac4 cockpit GPU: 0 draws', '\nac4 cockpit GPU: 9 draws'):
            with self.assertRaises(ValueError):
                validate_cockpit(self.report+suffix, True)
        with self.assertRaises(ValueError):
            validate_cockpit(self.report+'\nac4 cockpit GPU: 6 draws')

    def test_secondary_coverage(self):
        report = self.report+'\nac4 cockpit secondary: 24 vertices, 24 exact, 0 mismatches, 0 header failures, 0 rejected'
        validate_cockpit(report, secondary=True)
        gpu = report+'\nac4 cockpit GPU: 12 draws\nac4 cockpit secondary GPU: 6 draws'
        self.assertEqual(validate_cockpit(gpu, True, True)['secondary_draws'], 6)
        for text in (report+'\nac4 cockpit GPU: 6 draws',
                     gpu.replace('24 exact, 0 mismatches', '23 exact, 1 mismatches'),
                     gpu.replace('0 header failures', '1 header failures'),
                     gpu.replace('secondary GPU: 6', 'secondary GPU: 7')):
            with self.assertRaises(ValueError):
                validate_cockpit(text, True, True)

    def test_clipping_requires_packet_scratch_and_real_draws(self):
        report = self.report+'\nac4 cockpit clipping: 4 packets, 4 exact, 0 packet / 0 scratch mismatches, 0 declined, 1 empty'
        validate_cockpit(report, clipping=True)
        gpu = report+'\nac4 cockpit GPU: 10 draws\nac4 cockpit clipped GPU: 4 draws'
        self.assertEqual(validate_cockpit(gpu, True, clipping=True)['clipped_draws'], 4)
        for text in (report+'\nac4 cockpit GPU: 6 draws',
                     gpu.replace('0 scratch', '1 scratch'),
                     gpu.replace('0 packet /', '1 packet /'),
                     gpu.replace('1 empty', '4 empty'),
                     gpu.replace('clipped GPU: 4', 'clipped GPU: 40')):
            with self.assertRaises(ValueError):
                validate_cockpit(text, True, clipping=True)
