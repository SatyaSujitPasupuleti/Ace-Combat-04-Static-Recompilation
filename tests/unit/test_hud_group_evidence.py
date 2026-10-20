import unittest
from analyze_ac4_hud_groups import SITES, validate

class HudGroupEvidence(unittest.TestCase):
    def sample(self):
        return "\n".join(f"ac4 HUD group: site {site:08x} calls 3 matched 2 empty 1 declined 0 packets 5" for site in SITES) + "\nac4 HUD group balance: 0 open, 0 unpaired"

    def test_complete_coverage(self):
        report = validate(self.sample())
        self.assertEqual(len(report), 12)
        self.assertEqual(report["001633c8"]["name"], "TIME")

    def test_missing_duplicate_unbalanced_or_declined(self):
        sample = self.sample()
        for broken in ("", sample + "\n" + sample, sample.replace("0 open", "1 open"),
                       sample.replace("0 unpaired", "1 unpaired"), sample.replace("declined 0", "declined 1"),
                       sample.replace("calls 3", "calls 4"), sample.replace("packets 5", "packets 1"),
                       sample.replace("matched 2", "matched 0"), sample.replace("00162958", "00162959")):
            with self.subTest(broken=broken):
                with self.assertRaises(ValueError):
                    validate(broken)

    def test_native_ownership_requires_all_matched_groups(self):
        sample = self.sample() + "\nac4 HUD group ownership: 24 queued, 0 declined"
        self.assertEqual(len(validate(sample, native=True)), 12)
        for broken in (self.sample(), sample.replace("24 queued", "23 queued"),
                       sample.replace("0 declined", "1 declined"),
                       sample + "\nac4 HUD group ownership: 24 queued, 0 declined"):
            with self.assertRaises(ValueError):
                validate(broken, native=True)

    def test_alternate_view_coverage_is_explicit(self):
        sample = self.sample()
        for site in (0x160e78, 0x161720):
            sample = sample.replace(f"site {site:08x} calls 3 matched 2 empty 1 declined 0 packets 5", f"site {site:08x} calls 0 matched 0 empty 0 declined 0 packets 0")
        self.assertEqual(len(validate(sample)), 12)
        with self.assertRaises(ValueError):
            validate(sample, views=True)

    def test_observer_candidates_are_not_native_coverage(self):
        text = self.sample() + "\nac4 HUD group ownership: 24 queued, 0 declined"
        text += "\nac4 HUD candidate: site 0016bc68 calls 4 matched 0 empty 0 declined 4 packets 0"
        self.assertEqual(len(validate(text, native=True, views=True)), 12)

    def test_alternate_map_observation_is_required_only_when_requested(self):
        candidate = "\nac4 HUD candidate: site 0016bc68 calls 4 matched 3 empty 1 declined 0 packets 9"
        text = self.sample() + "\nac4 HUD group ownership: 24 queued, 0 declined"
        self.assertEqual(len(validate(text+candidate,native=True,map_candidate=True)),12)
        for bad in ('',candidate+candidate,candidate.replace('matched 3','matched 0'),
                    candidate.replace('declined 0','declined 1'),candidate.replace('calls 4','calls 5'),
                    candidate.replace('packets 9','packets 1')):
            with self.subTest(candidate=bad),self.assertRaises(ValueError):
                validate(text+bad,native=True,map_candidate=True)
