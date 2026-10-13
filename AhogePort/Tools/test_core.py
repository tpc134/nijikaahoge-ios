"""Compile real C++/Box2D tests without creating an Xcode project."""
import argparse
import os
import pathlib
import platform
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--msvc', type=pathlib.Path)
    ap.add_argument('--asan', action='store_true')
    args = ap.parse_args()
    out = ROOT/'Tests/out'
    out.mkdir(exist_ok=True)
    sources = sorted((ROOT/'Vendor/Box2D').rglob('*.cpp'))
    sources += [p for p in sorted((ROOT/'Sources').glob('*.cpp')) if p.name != 'AHRenderer.cpp'] + [ROOT/'Tests/CoreTests.cpp']
    env = dict(os.environ)
    if platform.system() == 'Windows':
        vc = args.msvc or pathlib.Path(r'E:\vs2022bt\VC\Tools\MSVC\14.44.35207')
        sdk = pathlib.Path(r'C:\Program Files (x86)\Windows Kits\10')
        version = sorted((sdk/'Include').iterdir())[-1].name
        env['INCLUDE'] = ';'.join(str(p) for p in [vc/'include', sdk/'Include'/version/'ucrt', sdk/'Include'/version/'shared', sdk/'Include'/version/'um'])
        env['LIB'] = ';'.join(str(p) for p in [vc/'lib/x64', sdk/'Lib'/version/'ucrt/x64', sdk/'Lib'/version/'um/x64'])
        compiler = vc/'bin/Hostx64/x64/cl.exe'
        exe = out/'CoreTests.exe'
        command = [str(compiler), '/nologo', '/EHsc', '/std:c++14', '/O2', '/W3',
                   '/I'+str(ROOT/'Sources'), '/I'+str(ROOT/'Vendor'), '/Fe:'+str(exe)]
        if args.asan:
            command += ['/fsanitize=address', '/Zi']
        env['PATH'] = str(compiler.parent) + os.pathsep + env['PATH']
        response = out/'sources.rsp'
        response.write_text('\n'.join('"%s"' % p for p in sources), encoding='utf-8')
        command += ['@'+str(response)]
    else:
        exe = out/'CoreTests'
        command = [shutil.which('clang++') or shutil.which('g++'), '-std=c++11', '-O2', '-pthread',
                   '-I'+str(ROOT/'Sources'), '-I'+str(ROOT/'Vendor'), *map(str,sources), '-o', str(exe)]
        if args.asan:
            command += ['-fsanitize=address,undefined', '-g']
    subprocess.run(command, cwd=out, env=env, check=True)
    run = subprocess.run([str(exe), str(ROOT/'Resources/AhogeAssets')], cwd=out, env=env, text=True, capture_output=True)
    print(run.stdout)
    print(run.stderr)
    (out/('results-asan.txt' if args.asan else 'results.txt')).write_text(run.stdout+run.stderr, encoding='utf-8')
    raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
