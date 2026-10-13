"""Compare every image and structured field against the supplied v1.2 archive."""
import argparse
import copy
import hashlib
import json
import pathlib
import wave
import zipfile
from PIL import Image
from prepare_assets import ALIASES, generate

ROOT=pathlib.Path(__file__).resolve().parents[1]


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--android-zip',type=pathlib.Path,required=True)
    args=ap.parse_args()
    z=zipfile.ZipFile(args.android_zip)
    prefix=next(n[:-len('game_data.json')] for n in z.namelist() if n.endswith('/assets/game_data.json'))
    source=json.loads(z.read(prefix+'game_data.json'))
    data=json.loads((ROOT/'Tools/data.normalized.json').read_text(encoding='utf-8'))
    names=json.loads((ROOT/'Tools/asset_name_map.json').read_text(encoding='utf-8'))
    expected=copy.deepcopy(source)
    for t in expected['types'].values():
        if 'image' in t:t['image']=names[t['image']]
        for a in t.get('anims',{}).values():
            for f in a['frames']:f['file']=names[f['file']]
    expected['types']={ALIASES.get(k,k):v for k,v in expected['types'].items()}
    for e in expected['instances']:e['type']=ALIASES.get(e['type'],e['type'])
    expected['media']=['%d.wav'%i for i in range(12)]
    assert expected==data,'Structured scene changed beyond documented names/audio extensions'
    resources=ROOT/'Resources/AhogeAssets'
    for original,renamed in names.items():
        assert z.read(prefix+original)==(resources/renamed).read_bytes(),original
    assert z.read(prefix+'small_objects_atlas.png')==(resources/'atlas.png').read_bytes()
    atlas=json.loads((ROOT/'Tools/atlas.normalized.json').read_text(encoding='utf-8'))
    before=(ROOT/'Sources/AHData.generated.cpp').read_bytes()
    generate(data,atlas)
    assert before==(ROOT/'Sources/AHData.generated.cpp').read_bytes(),'Generated C++ differs from bundled data'
    audio=[]
    for i in range(12):
        with wave.open(str(resources/'media'/('%d.wav'%i)),'rb') as w:
            assert w.getframerate()==48000 and w.getsampwidth()==2
            assert w.getnchannels()==(1 if i==0 else 2)
            audio.append({'id':i,'channels':w.getnchannels(),'frames':w.getnframes(),'seconds':w.getnframes()/48000})
    max_texture=max(max(Image.open(p).size) for p in resources.rglob('*.png'))
    assert max_texture<=2048
    forbidden=[p for p in ROOT.rglob('*') if p.suffix in ('.xcodeproj','.xcworkspace','.pbxproj','.ipa')]
    assert not forbidden,forbidden
    result={'input_sha256':hashlib.sha256(args.android_zip.read_bytes()).hexdigest(),
            'scene_fields_identical':True,'original_pngs_identical':len(names),'atlas_identical':True,
            'cpp_generation_reproducible':True,'max_texture_dimension':max_texture,
            'audio':audio,'no_xcode_project_or_ipa':True}
    (ROOT/'Tests/out/asset-results.json').write_text(json.dumps(result,indent=2),encoding='ascii')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
