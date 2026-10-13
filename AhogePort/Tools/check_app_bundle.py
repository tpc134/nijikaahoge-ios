"""Inspect a user-built .app for common legacy-iOS import mistakes; no project creation."""
import argparse
import pathlib
import plistlib
import struct

ROOT = pathlib.Path(__file__).resolve().parents[1]


def settings_errors(info):
    errors = []
    if info.get('MinimumOSVersion') != '6.0':
        errors.append('Built MinimumOSVersion must be 6.0, not merely the Project setting.')
    for key in ('UIMainStoryboardFile', 'UIMainStoryboardFile~ipad', 'UILaunchStoryboardName',
                'UILaunchStoryboardName~ipad', 'UILaunchImages', 'UILaunchImages~ipad'):
        if key in info:
            errors.append('Remove %s when using the supplied programmatic entry/legacy launch PNGs.' % key)
    if info.get('UIStatusBarHidden') is not True:
        errors.append('UIStatusBarHidden must be YES for iOS 6.')
    if sorted(info.get('UIDeviceFamily', [])) != [1, 2]:
        errors.append('Targeted Device Family should be Universal (iPhone/iPod + iPad).')
    if info.get('UIBackgroundModes'):
        errors.append('Do not enable background modes for this game.')
    capabilities = info.get('UIRequiredDeviceCapabilities', [])
    for key in ('armv7', 'opengles-2'):
        present = capabilities.get(key) is True if isinstance(capabilities, dict) else key in capabilities
        if not present:
            errors.append('Required device capabilities must include %s.' % key)
    for suffix in ('', '~ipad'):
        orientations = info.get('UISupportedInterfaceOrientations' + suffix, [])
        expected = {'UIInterfaceOrientationPortrait', 'UIInterfaceOrientationPortraitUpsideDown',
                    'UIInterfaceOrientationLandscapeLeft', 'UIInterfaceOrientationLandscapeRight'}
        if not expected.issubset(orientations):
            errors.append('Enable all four orientations in UISupportedInterfaceOrientations%s.' % suffix)
    executable = info.get('CFBundleExecutable')
    if not executable or pathlib.PurePath(executable).name != executable:
        errors.append('CFBundleExecutable is missing or invalid.')
    return errors


def png_size(path):
    # IHDR also remains readable in Xcode's CgBI-optimized PNGs.
    data = path.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Not a PNG: %s' % path)
    offset = 8
    while offset + 12 <= len(data):
        size = struct.unpack_from('>I', data, offset)[0]
        if size > len(data) - offset - 12:
            break
        if data[offset + 4:offset + 8] == b'IHDR' and size == 13:
            return struct.unpack_from('>II', data, offset + 8)
        offset += size + 12
    raise ValueError('Missing IHDR: %s' % path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('app', type=pathlib.Path)
    args = parser.parse_args()
    app = args.app
    with (app / 'Info.plist').open('rb') as stream:
        info = plistlib.load(stream)
    errors = settings_errors(info)
    if not (app / info.get('CFBundleExecutable', '')).is_file():
        errors.append('Built executable is missing.')
    source = ROOT / 'Resources'
    for p in (source / 'AhogeAssets').rglob('*'):
        if p.is_file():
            relative = p.relative_to(source)
            target = app / relative
            if not target.is_file():
                errors.append('Missing resource: %s (AhogeAssets must be a blue folder reference).' % relative)
            elif p.suffix == '.png' and png_size(p) != png_size(target):
                errors.append('PNG dimensions changed: %s' % relative)
    for p in (source / 'LegacyLaunch').glob('*.png'):
        target = app / p.name
        if not target.is_file() or png_size(target) != png_size(p):
            errors.append('Missing/wrong launch PNG at bundle root: %s' % p.name)
    if errors:
        for error in errors:
            print('FAIL:', error)
        raise SystemExit(1)
    print('PASS built plist, executable presence, AhogeAssets paths/dimensions and legacy launch PNGs.')
    print('Not a signing, Mach-O architecture, GPU, audio or iOS device test.')


if __name__ == '__main__':
    main()
