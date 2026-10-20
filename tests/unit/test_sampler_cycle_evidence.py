import unittest
from test_ac4_sampler_cycle import validate_cycle

class SamplerCycleEvidence(unittest.TestCase):
    def test_requires_order_updates_native_textures_and_display(self):
        good=''.join(f'[REPLAY] sampler cycle pass {i}: {v}x\n' for i,v in enumerate((16,0,4,16),1))
        good+=''.join(f'vk: anisotropic filtering {v}x\n' for v in (0,4,16))
        good+='rn: 2D layer -- 8 elements (8 accepted whole), 8 primitives: 8 accepted, 0 kept emulated; 0 elements not closed by their end record\n'
        good+='vk: 2D overlay composed on 4 frames at 5x, 14 draws lifted onto it\n'
        good+='vk: AC4 pre-display overlay composed on 4 transfers\n'
        good+='vk: AC4 native display: 4 copies, 4 presented, 5x at 3200x2240\n'
        good+='ac4 model textures: 10 native mip draws, 0 allocation fallbacks, 10 unsupported states\n'
        validate_cycle(good)
        for bad in ('',good*2,good.replace('pass 2: 0x','pass 2: 4x'),
                    good.replace('filtering 4x','filtering 8x'),
                    good.replace('10 native mip','0 native mip'),
                    good.replace('0 allocation','1 allocation'),
                    good.replace('4 presented','0 presented')):
            with self.assertRaises(ValueError):validate_cycle(bad)
