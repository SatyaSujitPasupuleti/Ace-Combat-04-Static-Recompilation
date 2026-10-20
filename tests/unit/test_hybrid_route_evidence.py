import unittest
from test_ac4_hybrid_route import validate_route_report
from analyze_ac4_hud_groups import SITES


REPORT = '''rn: 2D layer -- 10 elements (10 accepted whole), 10 primitives: 10 accepted, 0 kept emulated; 0 elements not closed by their end record
vk: 2D overlay composed on 2 frames at 5x, 10 draws lifted onto it
vk: AC4 pre-display overlay composed on 2 transfers
vk: AC4 native display: 2 copies, 2 presented, 5x at 3200x2240
ac4 screen sprite shadow: 10 calls, 10 matched, 0 mismatched, 0 declined, 0 unpaired, 0 open
ac4 lit native: 5 activations
ac4 unlit native: 5 activations
ac4 terrain native: 5 activations
ac4 native sky: 2 intents
ac4 native clouds: 2 intents
ac4 supplied mip chains: 20 draws
ac4 supplied filtering: 10 hardware, 10 anisotropic draws
ac4 model textures: 30 native mip draws, 1 allocation fallbacks, 2 unsupported states
vk: 40 native mesh draws (mesh.vert), 30 recorded with the native material (mesh.frag); 0 batches refused for arena space
vk: native mip sampler anisotropy 16.0x
alive: vblank pump at field 600 -- 49.3 fields/s over the last 600
'''


class HybridRouteEvidence(unittest.TestCase):
    def test_hybrid_hud_requires_complete_owned_groups(self):
        hud = ''.join(f'ac4 HUD group: site {site:08x} calls 1 matched 1 empty 0 declined 0 packets 1\n' for site in SITES)
        hud += f'ac4 HUD group balance: 0 open, 0 unpaired\nac4 HUD group ownership: {len(SITES)} queued, 0 declined\n'
        self.assertEqual(len(validate_route_report(REPORT+hud,hud=True)['hud']),len(SITES))
        for text in (REPORT,REPORT+hud+hud,(REPORT+hud).replace('12 queued','11 queued'),
                     (REPORT+hud).replace('site 00169010','site 00169011')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                validate_route_report(text,hud=True)

    def test_cockpit_route_requires_all_native_paths(self):
        native='ac4 cockpit native: 10 activations, 20 packets, 1000 original instructions replaced\n'
        gif='ac4 cockpit GIF packets: 15 packets, 300 qwords bypassed generic parsing\n'
        gpu='ac4 cockpit GPU: 30 draws\n'
        report=REPORT+native+gif+gpu
        self.assertEqual(validate_route_report(report,True)['cockpit']['draws'],30)
        for text in (REPORT,report+native,report+gif,report+gpu,
                     report.replace(native,''),report.replace(gif,''),report.replace(gpu,''),
                     report.replace('10 activations','0 activations'),report.replace('20 packets','9 packets'),
                     report.replace('1000 original','0 original'),report.replace('15 packets','21 packets'),
                     report.replace('300 qwords','0 qwords'),report.replace('30 draws','0 draws')):
            with self.subTest(text=text),self.assertRaises(ValueError):
                validate_route_report(text,True)

    def test_records_fallbacks_and_rates_without_claiming_performance(self):
        report = validate_route_report(REPORT)
        self.assertEqual(report['textures']['fallbacks'], 1)
        self.assertEqual(report['interval_fields_per_second'], [49.3])

    def test_rejects_incomplete_or_inconsistent_route(self):
        for text in ('', REPORT * 2, REPORT.replace('5 activations', '0 activations'),
                     REPORT.replace('10 accepted whole', '0 accepted whole'),
                     REPORT.replace('0 mismatched', '1 mismatched'),
                     REPORT.replace('10 matched', '9 matched'),
                     REPORT.replace('2 presented', '0 presented'),
                     REPORT.replace('10 anisotropic', '0 anisotropic'),
                     REPORT.replace('10 hardware', '21 hardware'),
                     REPORT.replace('0 batches refused', '1 batches refused'),
                     REPORT.replace('30 recorded', '0 recorded'),
                     REPORT.replace('30 recorded', '41 recorded'),
                     REPORT.replace('16.0x', '1.0x'),
                     REPORT.replace('30 native mip', '19 native mip'),
                     REPORT.replace('30 native mip', '0 native mip')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                validate_route_report(text)
