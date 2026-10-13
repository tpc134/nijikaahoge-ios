"""ARM language/ABI syntax check using local NDK Clang, NOT an Apple SDK check."""
import pathlib
import subprocess

ROOT=pathlib.Path(__file__).resolve().parents[1]
LLVM=pathlib.Path(r'E:\Android\Sdk\ndk\28.2.13676358\toolchains\llvm\prebuilt\windows-x86_64')
sources=sorted((ROOT/'Sources').glob('*.cpp'))+[ROOT/'Tests/CoreTests.cpp']
lines=[]
for arch in ('armv7a-linux-androideabi21','aarch64-linux-android21'):
    for source in sources:
        command=[str(LLVM/'bin/clang.exe'),'--target='+arch,'-fintegrated-cc1',
                 '-resource-dir',str(LLVM/'lib/clang/19'),'--sysroot='+str(LLVM/'sysroot'),
                 '-isystem',str(LLVM/'sysroot/usr/include/c++/v1'),'-std=c++11','-fsyntax-only',
                 '-I'+str(ROOT/'Sources'),'-I'+str(ROOT/'Vendor'),str(source)]
        result=subprocess.run(command,capture_output=True,text=True)
        if result.returncode:
            print(result.stdout,result.stderr)
            raise SystemExit(result.returncode)
        line='PASS C++11 %s %s'%(arch,source.name)
        print(line,flush=True);lines.append(line)
out=ROOT/'Tests/out/cpp11-results.txt'
out.write_text('\n'.join(lines)+'\nNOT an Apple SDK compile/link test.\n',encoding='utf-8')
