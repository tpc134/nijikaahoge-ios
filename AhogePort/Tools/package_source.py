"""Package importable source/resources/evidence, never IDE projects or host binaries."""
import hashlib
import pathlib
import shutil
import zipfile

ROOT=pathlib.Path(__file__).resolve().parents[1]
DIST=ROOT.parent/'dist'
evidence=ROOT/'Verification'
evidence.mkdir(exist_ok=True)
for source in [ROOT/'Tests/out/results.txt',ROOT/'Tests/out/results-asan.txt',
               ROOT/'Tests/out/cpp11-results.txt',ROOT/'Tests/out/asset-results.json',
               ROOT/'Tests/out/bundle-results.txt',
               ROOT/'Tests/out/gles/results.txt']:
    name='gles-results.txt' if source.parent.name=='gles' else source.name
    shutil.copyfile(source,evidence/name)
shots=evidence/'Screenshots'
shots.mkdir(exist_ok=True)
for source in (ROOT/'Tests/out/gles').glob('*.png'):
    shutil.copyfile(source,shots/source.name)
files=[p for p in ROOT.rglob('*') if p.is_file() and not any(s in ('out','__pycache__','.git') for s in p.relative_to(ROOT).parts)
       and p.name!='SHA256SUMS.txt']
for p in files:
    assert p.suffix.lower() not in ('.exe','.dll','.obj','.pdb','.lib','.exp','.pbxproj','.ipa')
    assert not any(s.endswith(('.xcodeproj','.xcworkspace')) for s in p.parts)
manifest=ROOT/'SHA256SUMS.txt'
manifest.write_text('\n'.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(ROOT).as_posix()
                              for p in sorted(files))+'\n',encoding='utf-8')
files.append(manifest)
DIST.mkdir(exist_ok=True)
dest=DIST/'NijikaAhoge-ios6-source.zip'
with zipfile.ZipFile(dest,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in sorted(files):z.write(p,'NijikaAhoge-ios6-source/'+p.relative_to(ROOT).as_posix())
with zipfile.ZipFile(dest) as z:
    assert z.testzip() is None
    assert len(z.namelist())==len(files)
digest=hashlib.sha256(dest.read_bytes()).hexdigest()
(DIST/'NijikaAhoge-ios6-source.zip.sha256').write_text(digest+'  '+dest.name+'\n',encoding='ascii')
print('Archive:',dest,'\nFiles:',len(files),'\nBytes:',dest.stat().st_size,'\nSHA256:',digest)
