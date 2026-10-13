"""Host-side tests of bundle inspection, not an Apple build or installation test."""
import copy
import pathlib
import plistlib
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Tools'))
from check_app_bundle import png_size, settings_errors

ORIENTATIONS = ['UIInterfaceOrientationPortrait', 'UIInterfaceOrientationPortraitUpsideDown',
                'UIInterfaceOrientationLandscapeLeft', 'UIInterfaceOrientationLandscapeRight']
GOOD = {'MinimumOSVersion': '6.0', 'UIStatusBarHidden': True, 'UIDeviceFamily': [1, 2],
        'UIRequiredDeviceCapabilities': ['armv7', 'opengles-2'], 'CFBundleExecutable': 'Ahoge',
        'UISupportedInterfaceOrientations': ORIENTATIONS, 'UISupportedInterfaceOrientations~ipad': ORIENTATIONS}


class BundleChecks(unittest.TestCase):
    def test_good_settings(self):
        self.assertEqual(settings_errors(GOOD), [])

    def test_wrong_minimum(self):
        info = dict(GOOD, MinimumOSVersion='9.3')
        self.assertTrue(any('MinimumOSVersion' in e for e in settings_errors(info)))

    def test_stale_storyboard_and_launch_array(self):
        for key in ('UIMainStoryboardFile', 'UILaunchStoryboardName', 'UILaunchImages', 'UILaunchImages~ipad'):
            info = dict(GOOD)
            info[key] = '' if 'Images' not in key else []
            self.assertTrue(any(key in e for e in settings_errors(info)))

    def test_dictionary_capabilities(self):
        info = dict(GOOD, UIRequiredDeviceCapabilities={'armv7': True, 'opengles-2': True})
        self.assertEqual(settings_errors(info), [])
        info['UIRequiredDeviceCapabilities']['armv7'] = False
        self.assertTrue(any('armv7' in e for e in settings_errors(info)))

    def test_missing_tablet_orientations(self):
        info = copy.deepcopy(GOOD)
        del info['UISupportedInterfaceOrientations~ipad']
        self.assertTrue(any('~ipad' in e for e in settings_errors(info)))

    def test_real_png_dimensions(self):
        self.assertEqual(png_size(ROOT / 'Resources/AhogeAssets/atlas.png'), (512, 512))
        self.assertEqual(png_size(ROOT / 'Resources/LegacyLaunch/Default-568h@2x.png'), (640, 1136))

    def test_complete_and_incomplete_resource_imports(self):
        fixture_root = ROOT / 'Tests/out'
        fixture_root.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ahoge-bundle-test-', dir=fixture_root) as temporary:
            pathlib.Path(temporary).resolve().relative_to(fixture_root.resolve())
            app = pathlib.Path(temporary) / 'HostFixture.app'
            app.mkdir()
            shutil.copytree(ROOT / 'Resources/AhogeAssets', app / 'AhogeAssets')
            for image in (ROOT / 'Resources/LegacyLaunch').glob('*.png'):
                shutil.copyfile(image, app / image.name)
            (app / 'Ahoge').write_bytes(b'host test fixture, not a Mach-O executable')
            (app / 'Info.plist').write_bytes(plistlib.dumps(GOOD, fmt=plistlib.FMT_BINARY))
            command = [sys.executable, str(ROOT / 'Tools/check_app_bundle.py'), str(app)]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            (app / 'AhogeAssets/frames/frame000.png').unlink()
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Missing resource', result.stdout)


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(BundleChecks)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    out = ROOT / 'Tests/out/bundle-results.txt'
    out.write_text('Host bundle checker: %d tests; failures=%d errors=%d\nNot an Apple SDK build.\n' %
                   (result.testsRun, len(result.failures), len(result.errors)), encoding='ascii')
    raise SystemExit(not result.wasSuccessful())
