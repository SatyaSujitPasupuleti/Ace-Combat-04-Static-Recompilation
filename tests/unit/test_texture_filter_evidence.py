import unittest
from test_ac4_texture_filter import validate_filter_report

class TextureFilterEvidence(unittest.TestCase):
    def test_requires_real_consistent_coverage(self):
        base='vk: native mip sampler anisotropy 16.0x\nac4 supplied mip chains: 10 draws\nac4 model textures: 10 native mip draws, 0 allocation fallbacks, 10 unsupported states\n'
        self.assertEqual(validate_filter_report(base,'exact'),10)
        self.assertEqual(validate_filter_report(base+'ac4 supplied filtering: 10 hardware, 0 anisotropic draws','trilinear'),10)
        self.assertEqual(validate_filter_report(base+'ac4 supplied filtering: 10 hardware, 10 anisotropic draws','anisotropic'),10)
        for report,mode in [('', 'exact'),(base,'anisotropic'),(base*2,'exact'),
            (base.replace('16.0x','1.0x')+'ac4 supplied filtering: 10 hardware, 10 anisotropic draws','anisotropic'),
            (base.replace('0 allocation','1 allocation'),'exact'),
            (base.replace('10 native','9 native'),'exact'),
            (base+'ac4 supplied filtering: 0 hardware, 0 anisotropic draws','trilinear'),
            (base+'ac4 supplied filtering: 11 hardware, 11 anisotropic draws','anisotropic'),
            (base+'ac4 supplied filtering: 10 hardware, 0 anisotropic draws','anisotropic'),
            (base+'ac4 supplied filtering: 10 hardware, 10 anisotropic draws','exact')]:
            with self.assertRaises(ValueError):validate_filter_report(report,mode)
