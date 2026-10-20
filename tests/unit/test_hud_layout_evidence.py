import unittest
from analyze_ac4_hud_layout import read_layout, caller_functions, summarize


class HudLayoutEvidence(unittest.TestCase):
    def test_bounds_are_observations_not_classifications(self):
        rows = read_layout('3 00163660 0 20 24 54 40 1 1\n4 00163660 0 21 24 55 40 1 0')
        report = summarize(rows, {'00163660': '001633C8'})['00163660']
        self.assertEqual(report['frames'], [3, 4])
        self.assertEqual(len(report['bounds']), 2)
        self.assertEqual(report['native'], 1)
        self.assertNotIn('screen_fixed', report)

    def test_rejects_invalid_evidence(self):
        for text in ('', 'bad', '3 00163660 0 nan 24 54 40 1 1',
                     '3 00163660 0 55 24 54 40 1 1', '3 00163660 0 20 24 54 40 0 1'):
            with self.assertRaises(ValueError):
                read_layout(text)

    def test_generated_callsite_mapping_keeps_function_boundaries(self):
        text = '''PS2_NOIPA void func_001633C8(ps2_ctx *ctx) {
    ctx->r[31].ud[0] = (u64)(0x00163660ll);
    func_0015FEE0(ctx);
PS2_NOIPA void func_00163818(ps2_ctx *ctx) {
    func_0015FEE0(ctx);
'''
        self.assertEqual(caller_functions(text), {'00163660': '001633C8'})
