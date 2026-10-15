from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from svg_skill import diff_svgs


class RenderDiffTests(unittest.TestCase):
    def compare(self, first, second):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            for name, elements in (('first', first), ('second', second)):
                (output / f'{name}.svg').write_text(
                    '<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" '
                    f'viewBox="0 0 16 16">{elements}</svg>', encoding='utf-8')
            report = diff_svgs(str(output / 'first.svg'), str(output / 'second.svg'),
                               str(output / 'diff.png'))
            self.assertTrue(report['ok'], report)
            self.assertTrue((output / 'diff.png').is_file())
            return report

    def test_identical_images(self):
        shape = '<rect width="16" height="16" fill="#000000"/>'
        report = self.compare(shape, shape)
        self.assertFalse(report['changed'])
        self.assertIsNone(report['bbox'])

    def test_rgb_change_with_unchanged_alpha(self):
        report = self.compare('<rect width="16" height="16" fill="#000000"/>',
                              '<rect width="16" height="16" fill="#ff0000"/>')
        self.assertTrue(report['changed'])
        self.assertEqual(report['bbox'], (0, 0, 16, 16))

    def test_bounds_include_separate_rgb_and_alpha_changes(self):
        first = '<rect width="4" height="4" fill="#000000"/>'
        second = ('<rect width="4" height="4" fill="#ff0000"/>'
                  '<rect x="12" y="12" width="4" height="4" fill="#000000"/>')
        report = self.compare(first, second)
        self.assertTrue(report['changed'])
        self.assertEqual(report['bbox'], (0, 0, 16, 16))


if __name__ == '__main__':
    unittest.main()
