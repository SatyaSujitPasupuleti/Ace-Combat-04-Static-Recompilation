import unittest
from test_ac4_window_cycle import validate_cycle

class WindowCycleEvidence(unittest.TestCase):
    def sample(self):
        text="rn: 2D layer -- 4 elements (4 accepted whole), 8 primitives: 8 accepted, 0 kept emulated; 0 elements not closed by their end record\n"
        text+="vk: 2D overlay composed on 4 frames at 5x, 8 draws lifted onto it\nvk: AC4 pre-display overlay composed on 4 transfers\nvk: AC4 native display: 4 copies, 4 presented, 5x at 2560x2240\n"
        for i,(w,h) in enumerate(((1280,720),(1920,1080),(3840,2160),(1280,720)),1):
            text+=f"vk: replay window resized: requested {w}x{h}, drawable {w}x{h}\n[REPLAY] window cycle pass {i}: {w}x{h}\n"
        return text

    def test_complete_real_window_transitions(self):
        validate_cycle(self.sample())

    def test_missing_duplicate_wrong_extent_or_fallback(self):
        text=self.sample()
        for bad in ('',text+text,text.replace('pass 4','pass 3'),text.replace('drawable 3840x2160','drawable 1920x1080'),text+'overlay allocation failed'):
            with self.assertRaises(ValueError):validate_cycle(bad)
