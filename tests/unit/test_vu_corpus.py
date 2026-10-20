"""Synthetic scene censuses; no game microcode."""
import copy
import unittest
from vurecomp.__main__ import merge


def program(crc="a", data=b"synthetic", entries=None, lo=0, hi=16):
    return dict(crc=crc, bytes_=data, code_lo=lo, code_hi=hi,
                activations=2, instructions=4, xgkicks=1,
                entries=entries or [dict(pc=0, count=2)])


class VuCorpusTests(unittest.TestCase):
    def test_merge_preserves_scenes_and_unions_entries_without_mutating_inputs(self):
        a={"programs": [program(), program("b", b"other")]}
        b={"programs": [program(entries=[dict(pc=16, count=3), dict(pc=0, count=1)], hi=32)]}
        before=copy.deepcopy((a,b))
        result=merge([a,b])["programs"]
        self.assertEqual(len(result),2)
        self.assertEqual(result[0]["entries"],[dict(pc=0,count=3),dict(pc=16,count=3)])
        self.assertEqual(result[0]["instructions"],8)
        self.assertEqual(result[0]["code_hi"],32)
        self.assertEqual((a,b),before)

    def test_conflicting_image_identity_is_rejected(self):
        with self.assertRaisesRegex(ValueError,"Conflicting microcode"):
            merge([{"programs":[program()]},{"programs":[program(data=b"different")]}])

    def test_empty_input(self):
        self.assertEqual(merge([]),{"programs":[]})


if __name__ == "__main__": unittest.main()
