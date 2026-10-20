import unittest
import struct
from test_ac4_model_gpu import require_visible_ppm, validate_unlit_report, validate_culling_report, validate_clipping_report, validate_mixed_execution, validate_lit_secondary, validate_lit_clipping, validate_lit_memory, validate_lit_integers, validate_lit_scalar, validate_lit_vectors, validate_lit_cycles, validate_lit_flags, validate_lit_execution, validate_second_pass, validate_terrain_preparation, validate_terrain_packets, validate_terrain_gpu, require_dma_capture, validate_terrain_culling, validate_terrain_polygons, validate_clipped_terrain_gpu, validate_terrain_memory, validate_terrain_integers, validate_terrain_scalars, validate_terrain_instructions, validate_terrain_vectors, validate_terrain_flags, validate_terrain_continuation, validate_terrain_execution, validate_mesh_gif

class EvidenceTests(unittest.TestCase):
    def test_whole_mesh_gif_requires_positive_complete_packet_coverage(self):
        self.assertEqual(validate_mesh_gif('ac4 mesh GIF packets: 3 packets, 30 qwords bypassed generic parsing'),(3,30))
        for report in ('','ac4 model GIF: 30 payload qwords bypassed generic decoding',
                       'ac4 mesh GIF packets: 0 packets, 30 qwords bypassed generic parsing',
                       'ac4 mesh GIF packets: 3 packets, 3 qwords bypassed generic parsing'):
            with self.assertRaises(ValueError):
                validate_mesh_gif(report)

    def test_terrain_continuation_requires_all_components_and_exact_totals(self):
        base = '\n'.join((
            'ac4 terrain preparation: 2 seen, 2 compared, 14 vertices, 0 mismatches, 0 declined',
            'ac4 terrain mode 2: 1','ac4 terrain mode 4: 1',
            'ac4 terrain memory: 2 built, 2 ended, 0 mismatches, 0 declined',
            'ac4 terrain submissions: 2 packets, 0 memory mismatches, 0 sequence mismatches',
            'ac4 terrain integers: 2 built, 2 ended, 2 kicks, 0 end / 0 kick / 0 sequence mismatches, 0 declined',
            'ac4 terrain scalars: 2 built, 2 ended, 2 kicks, 0 end / 0 kick / 0 sequence mismatches, 0 declined',
            'ac4 terrain vectors: 2 built, 2 ended, 2 kicks, 0 end / 0 kick mismatches, 0 excluded',
            'ac4 terrain flags: 2 built, 2 ended, 0 mismatches, 0 excluded',
            'ac4 terrain instructions: 2 built, 2 ended, 0 mismatches, 0 excluded, 900 predicted / 900 observed'))+'\n'
        template='ac4 terrain continuation: {} built, {} ended, {} packets, {} instructions, {} mismatches, {} declined'
        good=template.format(2,2,2,900,0,0)
        self.assertEqual(validate_terrain_continuation(base+good),(2,2,900))
        native='ac4 terrain native: 2 activations, 2 packets, 900 original instructions replaced'
        self.assertEqual(validate_terrain_execution(base+good,native),(2,2,900))
        for invalid in ('',native.replace('2 activations','1 activations'),native.replace('2 packets','1 packets'),native.replace('900','899')):
            with self.assertRaises(ValueError):
                validate_terrain_execution(base+good,invalid)
        for values in ((1,1,1,900,0,0),(2,1,2,900,0,0),(2,2,1,900,0,0),(2,2,2,899,0,0),(2,2,2,900,1,0),(2,2,2,900,0,1)):
            with self.assertRaises(ValueError):
                validate_terrain_continuation(base+template.format(*values))
        for replacement in ('1 packets, 0 memory mismatches, 0 sequence mismatches',
                            '2 packets, 1 memory mismatches, 0 sequence mismatches',
                            '2 packets, 0 memory mismatches, 1 sequence mismatches'):
            with self.assertRaises(ValueError):
                validate_terrain_continuation(base.replace('2 packets, 0 memory mismatches, 0 sequence mismatches',replacement)+good)
        for component in ('memory','integers','scalars','vectors','flags','instructions','submissions'):
            with self.assertRaises(ValueError):
                validate_terrain_continuation(base.replace('ac4 terrain '+component+':','missing:')+good)

    def test_terrain_flags_require_all_activations(self):
        base = 'ac4 terrain preparation: 2 seen, 2 compared, 14 vertices, 0 mismatches, 0 declined\nac4 terrain mode 2: 1\nac4 terrain mode 4: 1\n'
        template = 'ac4 terrain flags: {} built, {} ended, {} mismatches, {} excluded'
        self.assertEqual(validate_terrain_flags(base+template.format(2,2,0,0)),(2,0))
        for values in ((1,1,0,1),(2,1,0,0),(2,2,1,0),(0,0,0,2)):
            with self.assertRaises(ValueError):
                validate_terrain_flags(base+template.format(*values))
        with self.assertRaises(ValueError):
            validate_terrain_flags(base)
        mixed = base.replace('2 seen, 2 compared','4 seen, 4 compared')+'ac4 terrain mode 1: 2\nac4 terrain culling: 2 seen, 2 ended, 1 predicted empty, 1 observed empty, 0 mismatches, 1 partial, 0 invalid\nac4 terrain polygons: 2 built, 0 declined, 3 packets, 0 mismatches, 0 sequence mismatches\n'
        self.assertEqual(validate_terrain_flags(mixed+template.format(4,4,0,0)),(4,0))
        with self.assertRaises(ValueError):
            validate_terrain_flags(mixed+template.format(2,2,0,2))

    def test_terrain_vectors_require_all_integer_coverage(self):
        base = 'ac4 terrain preparation: 2 seen, 2 compared, 14 vertices, 0 mismatches, 0 declined\nac4 terrain mode 2: 1\nac4 terrain mode 4: 1\n'
        base += 'ac4 terrain integers: 2 built, 2 ended, 2 kicks, 0 end / 0 kick / 0 sequence mismatches, 0 declined\n'
        template = 'ac4 terrain vectors: {} built, {} ended, {} kicks, {} end / {} kick mismatches, {} excluded'
        self.assertEqual(validate_terrain_vectors(base+template.format(2,2,2,0,0,0)),(2,2,0))
        for values in ((1,1,1,0,0,1),(2,1,2,0,0,0),(2,2,1,0,0,0),(2,2,2,1,0,0),(2,2,2,0,1,0)):
            with self.assertRaises(ValueError):
                validate_terrain_vectors(base+template.format(*values))
        with self.assertRaises(ValueError):
            validate_terrain_vectors(base)
        mixed = base.replace('2 seen, 2 compared','4 seen, 4 compared')+'ac4 terrain mode 1: 2\nac4 terrain culling: 2 seen, 2 ended, 1 predicted empty, 1 observed empty, 0 mismatches, 1 partial, 0 invalid\nac4 terrain polygons: 2 built, 0 declined, 3 packets, 0 mismatches, 0 sequence mismatches\n'
        mixed = mixed.replace('integers: 2 built, 2 ended, 2 kicks','integers: 4 built, 4 ended, 5 kicks')
        self.assertEqual(validate_terrain_vectors(mixed+template.format(4,4,5,0,0,0)),(4,5,0))
        with self.assertRaises(ValueError):
            validate_terrain_vectors(mixed+template.format(2,2,2,0,0,2))

    def test_terrain_instruction_coverage_and_totals(self):
        base = 'ac4 terrain preparation: 2 seen, 2 compared, 14 vertices, 0 mismatches, 0 declined\nac4 terrain mode 2: 1\nac4 terrain mode 4: 1\n'
        template = 'ac4 terrain instructions: {} built, {} ended, {} mismatches, {} excluded, {} predicted / {} observed'
        self.assertEqual(validate_terrain_instructions(base+template.format(2,2,0,0,900,900)),(2,900,0))
        for values in ((0,0,0,0,0,0),(1,1,0,1,900,900),(2,1,0,0,900,900),(2,2,1,0,900,900),(2,2,0,0,900,901),(2,2,0,0,0,0)):
            with self.assertRaises(ValueError):
                validate_terrain_instructions(base+template.format(*values))
        with self.assertRaises(ValueError):
            validate_terrain_instructions(base)
        mixed = base.replace('2 seen, 2 compared','4 seen, 4 compared') + 'ac4 terrain mode 1: 2\nac4 terrain culling: 2 seen, 2 ended, 1 predicted empty, 1 observed empty, 0 mismatches, 1 partial, 0 invalid\nac4 terrain polygons: 2 built, 0 declined, 3 packets, 0 mismatches, 0 sequence mismatches\n'
        self.assertEqual(validate_terrain_instructions(mixed+template.format(4,4,0,0,1000,1000)),(4,1000,0))
        with self.assertRaises(ValueError):
            validate_terrain_instructions(mixed+template.format(2,2,0,2,900,900))

    def test_terrain_scalars_require_all_integer_coverage(self):
        base = 'ac4 terrain preparation: 2 seen, 2 compared, 14 vertices, 0 mismatches, 0 declined\nac4 terrain mode 2: 1\nac4 terrain mode 4: 1\nac4 terrain integers: 2 built, 2 ended, 2 kicks, 0 end / 0 kick / 0 sequence mismatches, 0 declined\n'
        template = 'ac4 terrain scalars: {} built, {} ended, {} kicks, {} end / {} kick / {} sequence mismatches, {} declined'
        self.assertEqual(validate_terrain_scalars(base+template.format(2,2,2,0,0,0,0)), (2,2))
        for values in ((1,1,1,0,0,0,0),(2,1,2,0,0,0,0),(2,2,1,0,0,0,0),
                       (2,2,2,1,0,0,0),(2,2,2,0,1,0,0),(2,2,2,0,0,1,0),(2,2,2,0,0,0,1)):
            with self.assertRaises(ValueError):
                validate_terrain_scalars(base+template.format(*values))
        with self.assertRaises(ValueError):
            validate_terrain_scalars(base)

    def test_terrain_integers_require_complete_exit_and_kick_coverage(self):
        base = 'ac4 terrain preparation: 2 seen, 2 compared, 14 vertices, 0 mismatches, 0 declined\nac4 terrain mode 2: 1\nac4 terrain mode 4: 1\n'
        template = 'ac4 terrain integers: {} built, {} ended, {} kicks, {} end / {} kick / {} sequence mismatches, {} declined'
        self.assertEqual(validate_terrain_integers(base+template.format(2,2,2,0,0,0,0)), (2,2,0))
        for values in ((0,0,0,0,0,0,0),(1,1,1,0,0,0,0),(2,1,2,0,0,0,0),(2,2,1,0,0,0,0),
                       (2,2,2,1,0,0,0),(2,2,2,0,1,0,0),(2,2,2,0,0,1,0),(2,2,2,0,0,0,1)):
            with self.assertRaises(ValueError):
                validate_terrain_integers(base+template.format(*values))
        mixed = base.replace('2 seen, 2 compared', '4 seen, 4 compared') + 'ac4 terrain mode 1: 2\nac4 terrain culling: 2 seen, 2 ended, 1 predicted empty, 1 observed empty, 0 mismatches, 1 partial, 0 invalid\n'
        mixed += 'ac4 terrain polygons: 2 built, 0 declined, 3 packets, 0 mismatches, 0 sequence mismatches\n'
        self.assertEqual(validate_terrain_integers(mixed+template.format(4,4,5,0,0,0,0)), (4,5,0))
        with self.assertRaises(ValueError):
            validate_terrain_integers(mixed+template.format(3,3,2,0,0,0,1))
        with self.assertRaises(ValueError):
            validate_terrain_integers(base)

    def test_terrain_memory_requires_every_activation(self):
        base = 'ac4 terrain preparation: 2 seen, 2 compared, 14 vertices, 0 mismatches, 0 declined\nac4 terrain mode 2: 1\nac4 terrain mode 4: 1\n'
        template = 'ac4 terrain memory: {} built, {} ended, {} mismatches, {} declined'
        self.assertEqual(validate_terrain_memory(base+template.format(2,2,0,0)), (2,0))
        for values in ((0,0,0,0),(1,1,0,0),(2,1,0,0),(2,2,1,0),(2,2,0,1)):
            with self.assertRaises(ValueError):
                validate_terrain_memory(base+template.format(*values))
        with self.assertRaises(ValueError):
            validate_terrain_memory(base)
        mixed = base.replace('2 seen, 2 compared', '4 seen, 4 compared') + 'ac4 terrain mode 1: 2\nac4 terrain culling: 2 seen, 2 ended, 1 predicted empty, 1 observed empty, 0 mismatches, 1 partial, 0 invalid\n'
        self.assertEqual(validate_terrain_memory(mixed+template.format(4,4,0,0)), (4,0))
        with self.assertRaises(ValueError):
            validate_terrain_memory(mixed+template.format(3,3,0,1))
        with self.assertRaises(ValueError):
            validate_terrain_memory(mixed+template.format(2,2,0,2))

    def test_clipped_terrain_gpu_requires_both_streams(self):
        self.assertEqual(validate_clipped_terrain_gpu('ac4 clipped terrain GPU: 4 primary, 4 secondary draws'), (4,4))
        for report in ('', 'ac4 clipped terrain GPU: 0 primary, 4 secondary draws', 'ac4 clipped terrain GPU: 4 primary, 0 secondary draws'):
            with self.assertRaises(ValueError):
                validate_clipped_terrain_gpu(report)

    def test_combined_terrain_coverage_accounts_for_culled_and_clipped(self):
        report = '\n'.join((
            'ac4 terrain preparation: 4 seen, 4 compared, 20 vertices, 0 mismatches, 0 declined',
            'ac4 terrain mode 1: 2', 'ac4 terrain mode 2: 1', 'ac4 terrain mode 4: 1',
            'ac4 terrain packets: 2 compared, 0 packet / 0 bank mismatches, 0 declined',
            'ac4 terrain topology: 2 patches, 12 triangles, 0 declined',
            'ac4 terrain culling: 2 seen, 2 ended, 1 predicted empty, 1 observed empty, 0 mismatches, 1 partial, 0 invalid',
            'ac4 terrain polygons: 2 built, 0 declined, 1 packets, 0 mismatches, 0 sequence mismatches',
            'ac4 terrain clipped topology: 1 packets, 4 triangles, 0 declined'))
        self.assertEqual(validate_terrain_packets(report), (4,20))
        for before, after in (('mode 1: 2', 'mode 1: 3'), ('packets: 2 compared', 'packets: 4 compared'),
                              ('clipped topology: 1 packets', 'clipped topology: 0 packets'),
                              ('4 triangles, 0 declined', '4 triangles, 1 declined'),
                              ('1 packets, 0 mismatches', '1 packets, 1 mismatches')):
            with self.assertRaises(ValueError):
                validate_terrain_packets(report.replace(before, after))

    def test_terrain_polygons_require_complete_exact_packet_sequences(self):
        cull = 'ac4 terrain culling: 129 seen, 129 ended, 122 predicted empty, 122 observed empty, 0 mismatches, 7 partial, 0 invalid\n'
        template = 'ac4 terrain polygons: {} built, {} declined, {} packets, {} mismatches, {} sequence mismatches'
        self.assertEqual(validate_terrain_polygons(cull+template.format(129,0,22,0,0)), (129,122,22))
        for values in ((128,0,22,0,0),(129,1,22,0,0),(129,0,0,0,0),(129,0,22,1,0),(129,0,22,0,1)):
            with self.assertRaises(ValueError):
                validate_terrain_polygons(cull+template.format(*values))
        with self.assertRaises(ValueError):
            validate_terrain_polygons(cull)

    def test_terrain_culling_requires_complete_predictions(self):
        template = 'ac4 terrain culling: {} seen, {} ended, {} predicted empty, {} observed empty, {} mismatches, {} partial, {} invalid'
        self.assertEqual(validate_terrain_culling(template.format(129,129,122,122,0,7,0)), (129,122,7))
        for values in ((129,128,122,122,0,7,0), (7,7,0,0,0,7,0),
                       (129,129,122,121,0,7,0), (129,129,122,122,1,7,0),
                       (129,129,122,122,0,6,0), (129,129,122,122,0,7,1)):
            with self.assertRaises(ValueError):
                validate_terrain_culling(template.format(*values))
        with self.assertRaises(ValueError):
            validate_terrain_culling('')

    def test_geometry_acceptance_requires_dma_capture(self):
        require_dma_capture(b'GSCAP2\0\0' + struct.pack('<II', 2, 1))
        for header in (b'', b'GSCAP2', b'WRONG!\0\0' + struct.pack('<II', 2, 1),
                       b'GSCAP2\0\0' + struct.pack('<II', 2, 0),
                       b'GSCAP2\0\0' + struct.pack('<II', 3, 1)):
            with self.assertRaises(ValueError):
                require_dma_capture(header)

    def test_terrain_gpu_requires_both_material_passes(self):
        self.assertEqual(validate_terrain_gpu('ac4 terrain GPU: 12 primary, 8 secondary draws'), (12,8))
        for report in ('', 'ac4 terrain GPU: 0 primary, 8 secondary draws',
                       'ac4 terrain GPU: 12 primary, 0 secondary draws'):
            with self.assertRaises(ValueError):
                validate_terrain_gpu(report)

    def test_terrain_packets_require_matching_coverage(self):
        base = 'ac4 terrain preparation: 2 seen, 2 compared, 14 vertices, 0 mismatches, 0 declined\nac4 terrain mode 2: 1\nac4 terrain mode 4: 1\n'
        base += 'ac4 terrain topology: 2 patches, 12 triangles, 0 declined\n'
        template = 'ac4 terrain packets: {} compared, {} packet / {} bank mismatches, {} declined'
        self.assertEqual(validate_terrain_packets(base+template.format(2,0,0,0)), (2,14))
        for values in ((0,0,0,0),(1,0,0,0),(2,1,0,0),(2,0,1,0),(2,0,0,1)):
            with self.assertRaises(ValueError):
                validate_terrain_packets(base+template.format(*values))
        with self.assertRaises(ValueError):
            validate_terrain_packets(base)

    def test_terrain_topology_requires_full_coverage(self):
        base = 'ac4 terrain preparation: 2 seen, 2 compared, 14 vertices, 0 mismatches, 0 declined\nac4 terrain mode 2: 1\nac4 terrain mode 4: 1\nac4 terrain packets: 2 compared, 0 packet / 0 bank mismatches, 0 declined\n'
        for suffix in ('', 'ac4 terrain topology: 1 patches, 12 triangles, 0 declined',
                       'ac4 terrain topology: 2 patches, 0 triangles, 0 declined',
                       'ac4 terrain topology: 2 patches, 12 triangles, 1 declined'):
            with self.assertRaises(ValueError):
                validate_terrain_packets(base + suffix)

    def test_terrain_preparation_needs_both_patch_modes(self):
        template = 'ac4 terrain preparation: {} seen, {} compared, {} vertices, {} mismatches, {} declined'
        modes = '\nac4 terrain mode 2: 1\nac4 terrain mode 4: 1'
        self.assertEqual(validate_terrain_preparation(template.format(2,2,14,0,0)+modes), (2,14))
        for values in ((0,0,0,0,0),(2,1,14,0,0),(2,2,0,0,0),(2,2,14,1,0),(2,2,14,0,1)):
            with self.assertRaises(ValueError):
                validate_terrain_preparation(template.format(*values)+modes)
        for suffix in ('','\nac4 terrain mode 2: 1','\nac4 terrain mode 4: 1'):
            with self.assertRaises(ValueError):
                validate_terrain_preparation(template.format(2,2,14,0,0)+suffix)

    def test_secondary_gpu_requires_positive_coverage(self):
        self.assertEqual(validate_second_pass('ac4 model second GPU: 9 draws'), 9)
        for report in ('', 'ac4 model second GPU: 0 draws'):
            with self.assertRaises(ValueError):
                validate_second_pass(report)

    def test_lit_flags_require_complete_coverage(self):
        base = 'ac4 lit memory: 2 activations, 3 packets, 0 memory / 0 sequence mismatches, 0 declined, 0 packet mismatches\nac4 lit integers: 3 packets, 2 activations, 0 kick / 0 continuation mismatches, 0 declined\nac4 lit scalar: 3 packets, 2 activations, 0 kick / 0 continuation mismatches, 0 declined\nac4 lit vectors: 3 packets, 2 activations, 0 kick / 0 continuation mismatches, 0 skipped packets\nac4 lit cycles: 2 activations, 0 mismatches, 0 declined\n'
        template = 'ac4 lit flags: {} activations, {} mismatches, 0 declined'
        complete = base + 'ac4 lit flags: 2 activations, 0 mismatches, 0 declined\nac4 lit instructions: 100 predicted'
        native = 'ac4 lit native: {} activations, {} packets, {} original instructions replaced'
        self.assertEqual(validate_lit_execution(complete, native.format(2,3,100)), (2,3,100))
        for values in ((0,3,100),(2,0,100),(2,3,0),(1,3,100),(2,3,99)):
            with self.assertRaises(ValueError):
                validate_lit_execution(complete, native.format(*values))

        self.assertEqual(validate_lit_flags(base + template.format(2,0)), (2,3))
        for values in ((0,0),(1,0),(2,1)):
            with self.assertRaises(ValueError):
                validate_lit_flags(base + template.format(*values))
        with self.assertRaises(ValueError):
            validate_lit_flags(base)
        with self.assertRaises(ValueError):
            validate_lit_flags(base + 'ac4 lit flags: 2 activations, 0 mismatches, 1 declined')

    def test_lit_cycles_require_complete_coverage(self):
        base = 'ac4 lit memory: 2 activations, 3 packets, 0 memory / 0 sequence mismatches, 0 declined, 0 packet mismatches\nac4 lit integers: 3 packets, 2 activations, 0 kick / 0 continuation mismatches, 0 declined\nac4 lit scalar: 3 packets, 2 activations, 0 kick / 0 continuation mismatches, 0 declined\nac4 lit vectors: 3 packets, 2 activations, 0 kick / 0 continuation mismatches, 0 skipped packets\n'
        template = 'ac4 lit cycles: {} activations, {} mismatches, {} declined'
        self.assertEqual(validate_lit_cycles(base + template.format(2,0,0)), (2,3))
        for values in ((0,0,0),(1,0,0),(2,1,0),(2,0,1)):
            with self.assertRaises(ValueError):
                validate_lit_cycles(base + template.format(*values))
        with self.assertRaises(ValueError):
            validate_lit_cycles(base)

    def test_lit_vectors_require_complete_coverage(self):
        base = 'ac4 lit memory: 2 activations, 3 packets, 0 memory / 0 sequence mismatches, 0 declined, 0 packet mismatches\nac4 lit integers: 3 packets, 2 activations, 0 kick / 0 continuation mismatches, 0 declined\nac4 lit scalar: 3 packets, 2 activations, 0 kick / 0 continuation mismatches, 0 declined\n'
        template = 'ac4 lit vectors: {} packets, {} activations, {} kick / {} continuation mismatches, {} skipped packets'
        self.assertEqual(validate_lit_vectors(base + template.format(3,2,0,0,0)), (2,3))
        for values in ((0,2,0,0,0),(3,0,0,0,0),(2,2,0,0,0),(3,1,0,0,0),(3,2,1,0,0),(3,2,0,1,0),(3,2,0,0,1)):
            with self.assertRaises(ValueError):
                validate_lit_vectors(base + template.format(*values))
        with self.assertRaises(ValueError):
            validate_lit_vectors(base)

    def test_lit_scalar_requires_matching_complete_coverage(self):
        base = 'ac4 lit memory: 2 activations, 3 packets, 0 memory / 0 sequence mismatches, 0 declined, 0 packet mismatches\nac4 lit integers: 3 packets, 2 activations, 0 kick / 0 continuation mismatches, 0 declined\n'
        template = 'ac4 lit scalar: {} packets, {} activations, {} kick / {} continuation mismatches, {} declined'
        self.assertEqual(validate_lit_scalar(base + template.format(3,2,0,0,0)), (2,3))
        for values in ((0,2,0,0,0),(3,0,0,0,0),(2,2,0,0,0),(3,1,0,0,0),(3,2,1,0,0),(3,2,0,1,0),(3,2,0,0,1)):
            with self.assertRaises(ValueError):
                validate_lit_scalar(base + template.format(*values))
        with self.assertRaises(ValueError):
            validate_lit_scalar(base)

    def test_lit_integer_coverage_must_match_memory(self):
        base = 'ac4 lit memory: 2 activations, 3 packets, 0 memory / 0 sequence mismatches, 0 declined, 0 packet mismatches\n'
        template = 'ac4 lit integers: {} packets, {} activations, {} kick / {} continuation mismatches, {} declined'
        self.assertEqual(validate_lit_integers(base + template.format(3,2,0,0,0)), (2,3))
        for values in ((0,2,0,0,0),(3,0,0,0,0),(2,2,0,0,0),(3,1,0,0,0),(3,2,1,0,0),(3,2,0,1,0),(3,2,0,0,1)):
            with self.assertRaises(ValueError):
                validate_lit_integers(base + template.format(*values))
        with self.assertRaises(ValueError):
            validate_lit_integers(base)

    def test_lit_memory_requires_complete_positive_evidence(self):
        template = 'ac4 lit memory: {} activations, {} packets, {} memory / {} sequence mismatches, {} declined, {} packet mismatches'
        self.assertEqual(validate_lit_memory(template.format(2,3,0,0,0,0)), (2,3))
        for values in ((0,3,0,0,0,0),(2,0,0,0,0,0),(2,3,1,0,0,0),(2,3,0,1,0,0),(2,3,0,0,1,0),(2,3,0,0,0,1)):
            with self.assertRaises(ValueError):
                validate_lit_memory(template.format(*values))
        with self.assertRaises(ValueError):
            validate_lit_memory('')

    def test_blank_and_uniform_are_rejected(self):
        for pixel in (bytes((0,0,0)), bytes((128,64,32))):
            with self.assertRaisesRegex(ValueError, 'Uniform'):
                require_visible_ppm(b'P6\n2 1\n255\n' + pixel * 2)

    def test_nonuniform_is_accepted(self):
        require_visible_ppm(b'P6\n2 1\n255\n' + bytes((0,0,0,1,2,3)))

    def test_truncated_and_bad_header_rejected(self):
        for data in (b'P6\n2 1\n255\nabc', b'P3 2 1 255', b'P6\n0 1\n255\n'):
            with self.assertRaises(ValueError):
                require_visible_ppm(data)

    def test_unlit_plan_requires_nonzero_complete_matching_evidence(self):
        template = 'ac4 unlit plan: {} built, 0 declined, {} packets, {} packet mismatches, {} ended, {} memory mismatches, {} missing sequences, {} control mismatches\nac4 unlit state: 0 register mismatches\nac4 unlit flags: 0 pipeline mismatches\nac4 unlit cycles: 0 instruction-count mismatches'
        self.assertEqual(validate_unlit_report(template.format(2,4,0,2,0,0,0)), (2,0,4))
        cases = [(0,0,0,0,0,0,0),(2,0,0,2,0,0,0),(2,4,1,2,0,0,0),
                 (2,4,0,1,0,0,0),(2,4,0,2,1,0,0),(2,4,0,2,0,1,0),(2,4,0,2,0,0,1)]
        for case in cases:
            with self.assertRaises(ValueError):
                validate_unlit_report(template.format(*case))
        with self.assertRaises(ValueError):
            validate_unlit_report('unrelated log')

    def test_unlit_register_evidence_is_required(self):
        base = 'ac4 unlit plan: 1 built, 0 declined, 1 packets, 0 packet mismatches, 1 ended, 0 memory mismatches, 0 missing sequences, 0 control mismatches'
        for tail in ('', '\nac4 unlit state: 1 register mismatches'):
            with self.assertRaisesRegex(ValueError, 'register'):
                validate_unlit_report(base + tail)

    def test_unlit_pipeline_evidence_is_required(self):
        base = 'ac4 unlit plan: 1 built, 0 declined, 1 packets, 0 packet mismatches, 1 ended, 0 memory mismatches, 0 missing sequences, 0 control mismatches\nac4 unlit state: 0 register mismatches'
        for tail in ('', '\nac4 unlit flags: 1 pipeline mismatches'):
            with self.assertRaisesRegex(ValueError, 'pipeline'):
                validate_unlit_report(base + tail)

    def test_unlit_cycle_evidence_is_required(self):
        base = 'ac4 unlit plan: 1 built, 0 declined, 1 packets, 0 packet mismatches, 1 ended, 0 memory mismatches, 0 missing sequences, 0 control mismatches\nac4 unlit state: 0 register mismatches\nac4 unlit flags: 0 pipeline mismatches'
        for tail in ('', '\nac4 unlit cycles: 1 instruction-count mismatches'):
            with self.assertRaisesRegex(ValueError, 'cycle'):
                validate_unlit_report(base+tail)

    def test_culling_requires_actual_matching_coverage(self):
        template = 'ac4 unlit culling: {} planned activations, {} executed natively'
        self.assertEqual(validate_culling_report(template.format(5,0)),5)
        self.assertEqual(validate_culling_report(template.format(5,5),native=True),5)
        for report, native in (('',False),(template.format(0,0),False),
                               (template.format(5,0),True),(template.format(5,4),True),
                               (template.format(5,5),False)):
            with self.assertRaises(ValueError):
                validate_culling_report(report,native=native)

    def test_clipping_requires_packets_and_scratch_parity(self):
        template = 'ac4 unlit clip: {} packets compared, {} exact, {} mismatched, {} declined\nac4 unlit clip scratch: {} mismatched packets\nac4 unlit clip control: 0 integer-state mismatches\nac4 unlit clip vector: 0 state mismatches\nac4 unlit clip scalar: 0 state mismatches; 0 drain mismatches\nac4 unlit clip cycles: 2 activations, 0 mismatches\nac4 unlit clip flags: 0 state mismatches\nac4 unlit mixed memory: 0 mismatched activations, 0 packet mismatches, 0 sequence mismatches\nac4 unlit mixed registers: 0 state mismatches'
        self.assertEqual(validate_clipping_report(template.format(4,4,0,0,0)),4)
        for values in ((0,0,0,0,0),(4,3,0,0,0),(4,4,1,0,0),(4,4,0,1,0),(4,4,0,0,1)):
            with self.assertRaises(ValueError):
                validate_clipping_report(template.format(*values))
        for text in ('', 'ac4 unlit clip: 4 packets compared, 4 exact, 0 mismatched, 0 declined'):
            with self.assertRaises(ValueError):
                validate_clipping_report(text)

    def test_clipping_requires_integer_state_evidence(self):
        base = 'ac4 unlit clip: 4 packets compared, 4 exact, 0 mismatched, 0 declined\nac4 unlit clip scratch: 0 mismatched packets'
        for tail in ('', '\nac4 unlit clip control: 1 integer-state mismatches'):
            with self.assertRaisesRegex(ValueError, 'integer'):
                validate_clipping_report(base+tail)

    def test_clipping_requires_vector_state_evidence(self):
        base = 'ac4 unlit clip: 4 packets compared, 4 exact, 0 mismatched, 0 declined\nac4 unlit clip scratch: 0 mismatched packets\nac4 unlit clip control: 0 integer-state mismatches'
        for tail in ('', '\nac4 unlit clip vector: 1 state mismatches'):
            with self.assertRaisesRegex(ValueError, 'vector'):
                validate_clipping_report(base+tail)

    def test_clipping_requires_scalar_and_drain_evidence(self):
        base = 'ac4 unlit clip: 4 packets compared, 4 exact, 0 mismatched, 0 declined\nac4 unlit clip scratch: 0 mismatched packets\nac4 unlit clip control: 0 integer-state mismatches\nac4 unlit clip vector: 0 state mismatches'
        for tail in ('', '\nac4 unlit clip scalar: 1 state mismatches; 0 drain mismatches',
                     '\nac4 unlit clip scalar: 0 state mismatches; 1 drain mismatches'):
            with self.assertRaisesRegex(ValueError, 'scalar/drain'):
                validate_clipping_report(base+tail)

    def test_clipping_requires_nonzero_matching_cycle_evidence(self):
        base = 'ac4 unlit clip: 4 packets compared, 4 exact, 0 mismatched, 0 declined\nac4 unlit clip scratch: 0 mismatched packets\nac4 unlit clip control: 0 integer-state mismatches\nac4 unlit clip vector: 0 state mismatches\nac4 unlit clip scalar: 0 state mismatches; 0 drain mismatches'
        for tail in ('', '\nac4 unlit clip cycles: 0 activations, 0 mismatches',
                     '\nac4 unlit clip cycles: 2 activations, 1 mismatches'):
            with self.assertRaisesRegex(ValueError, 'cycle'):
                validate_clipping_report(base+tail)

    def test_clipping_requires_arithmetic_state_evidence(self):
        base = 'ac4 unlit clip: 4 packets compared, 4 exact, 0 mismatched, 0 declined\nac4 unlit clip scratch: 0 mismatched packets\nac4 unlit clip control: 0 integer-state mismatches\nac4 unlit clip vector: 0 state mismatches\nac4 unlit clip scalar: 0 state mismatches; 0 drain mismatches\nac4 unlit clip cycles: 2 activations, 0 mismatches'
        for tail in ('', '\nac4 unlit clip flags: 1 state mismatches'):
            with self.assertRaisesRegex(ValueError, 'arithmetic'):
                validate_clipping_report(base+tail)

    def test_clipping_requires_full_memory_and_sequence_evidence(self):
        base = 'ac4 unlit clip: 4 packets compared, 4 exact, 0 mismatched, 0 declined\nac4 unlit clip scratch: 0 mismatched packets\nac4 unlit clip control: 0 integer-state mismatches\nac4 unlit clip vector: 0 state mismatches\nac4 unlit clip scalar: 0 state mismatches; 0 drain mismatches\nac4 unlit clip cycles: 2 activations, 0 mismatches\nac4 unlit clip flags: 0 state mismatches'
        tails = [''] + [f'\nac4 unlit mixed memory: {a} mismatched activations, {b} packet mismatches, {c} sequence mismatches' for a,b,c in ((1,0,0),(0,1,0),(0,0,1))]
        for tail in tails:
            with self.assertRaisesRegex(ValueError, 'memory/sequence'):
                validate_clipping_report(base+tail)

    def test_clipping_requires_complete_register_evidence(self):
        base = 'ac4 unlit clip: 4 packets compared, 4 exact, 0 mismatched, 0 declined\nac4 unlit clip scratch: 0 mismatched packets\nac4 unlit clip control: 0 integer-state mismatches\nac4 unlit clip vector: 0 state mismatches\nac4 unlit clip scalar: 0 state mismatches; 0 drain mismatches\nac4 unlit clip cycles: 2 activations, 0 mismatches\nac4 unlit clip flags: 0 state mismatches\nac4 unlit mixed memory: 0 mismatched activations, 0 packet mismatches, 0 sequence mismatches'
        for tail in ('', '\nac4 unlit mixed registers: 1 state mismatches'):
            with self.assertRaisesRegex(ValueError, 'mixed-clipping register'):
                validate_clipping_report(base+tail)

    def test_mixed_execution_requires_matching_nonzero_shadow_coverage(self):
        shadow = 'ac4 mixed eligible: 2 activations, 4 packets, 2000 instructions'
        native = 'ac4 mixed native: 2 activations, 4 packets, 2000 original instructions replaced'
        self.assertEqual(validate_mixed_execution(shadow,native),(2,4,2000))
        for a,b in (('',native),(shadow,''),(shadow,native.replace('2000','1999')),
                    (shadow,native.replace('4 packets','3 packets')),
                    (shadow.replace('2 activations','0 activations'),native.replace('2 activations','0 activations'))):
            with self.assertRaises(ValueError):validate_mixed_execution(a,b)

    def test_lit_secondary_requires_matching_covered_output(self):
        template = 'ac4 lit second: {} vertices, {} header / {} stq / {} colour / {} geometry mismatches, {} rejected; 5 clipped submissions skipped\nac4 lit ADC: 2 marked vertices, 0 mismatches\nac4 lit packets: 4 compared, 0 mismatched'
        self.assertEqual(validate_lit_secondary(template.format(12,0,0,0,0,0)),12)
        for values in ((0,0,0,0,0,0),(12,1,0,0,0,0),(12,0,1,0,0,0),
                       (12,0,0,1,0,0),(12,0,0,0,1,0),(12,0,0,0,0,1)):
            with self.assertRaises(ValueError):validate_lit_secondary(template.format(*values))
        with self.assertRaises(ValueError):validate_lit_secondary('unrelated log')

    def test_lit_requires_adc_evidence(self):
        base = 'ac4 lit second: 12 vertices, 0 header / 0 stq / 0 colour / 0 geometry mismatches, 0 rejected; 5 clipped submissions skipped'
        for tail in ('', '\nac4 lit ADC: 2 marked vertices, 1 mismatches'):
            with self.assertRaisesRegex(ValueError,'ADC'):validate_lit_secondary(base+tail)

    def test_lit_requires_complete_packet_evidence(self):
        base = 'ac4 lit second: 12 vertices, 0 header / 0 stq / 0 colour / 0 geometry mismatches, 0 rejected; 5 clipped submissions skipped\nac4 lit ADC: 2 marked vertices, 0 mismatches'
        for tail in ('', '\nac4 lit packets: 0 compared, 0 mismatched', '\nac4 lit packets: 4 compared, 1 mismatched'):
            with self.assertRaisesRegex(ValueError,'packet'):validate_lit_secondary(base+tail)

    def test_lit_clipping_requires_matching_packets_and_scratch(self):
        template = 'ac4 lit clipping: {} packets, 0 empty, {} packet / {} scratch mismatches, {} declined'
        self.assertEqual(validate_lit_clipping(template.format(3,0,0,0)),3)
        for values in ((0,0,0,0),(3,1,0,0),(3,0,1,0),(3,0,0,1)):
            with self.assertRaises(ValueError):validate_lit_clipping(template.format(*values))
        with self.assertRaises(ValueError):validate_lit_clipping('unrelated log')
