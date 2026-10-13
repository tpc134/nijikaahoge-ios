"""Render the actual shared ES2 renderer on ANGLE, not a screenshot mock."""
import ctypes as ct
import os
import pathlib
import re
import subprocess
import shutil
import numpy as np
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[1]
VC = pathlib.Path(r'E:\vs2022bt\VC\Tools\MSVC\14.44.35207')
NDK = pathlib.Path(r'E:\Android\Sdk\ndk\28.2.13676358\toolchains\llvm\prebuilt\windows-x86_64')
ANGLE = pathlib.Path(r'E:\Android\Sdk\emulator\lib64\gles_angle')
OUT = ROOT/'Tests/out/gles'


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    includes = OUT/'include'
    for name in ('GLES2', 'EGL', 'KHR'):
        shutil.copytree(NDK/'sysroot/usr/include'/name, includes/name, dirs_exist_ok=True)
    sdk = pathlib.Path(r'C:\Program Files (x86)\Windows Kits\10')
    version = sorted((sdk/'Include').iterdir())[-1].name
    env = dict(os.environ)
    env['INCLUDE'] = ';'.join(map(str,[VC/'include',sdk/'Include'/version/'ucrt',sdk/'Include'/version/'shared',sdk/'Include'/version/'um']))
    env['LIB'] = ';'.join(map(str,[VC/'lib/x64',sdk/'Lib'/version/'ucrt/x64',sdk/'Lib'/version/'um/x64']))
    bin = VC/'bin/Hostx64/x64'
    env['PATH'] = str(bin)+os.pathsep+env['PATH']
    for name in ('libGLESv2','libEGL'):
        exports = subprocess.check_output([str(bin/'dumpbin.exe'),'/EXPORTS',str(ANGLE/(name+'.dll'))],env=env,text=True)
        names = re.findall(r'^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\w+)\s*$',exports,re.M)
        assert names, exports
        definition = OUT/(name+'.def')
        definition.write_text('LIBRARY '+name+'\nEXPORTS\n'+'\n'.join(names)+'\n')
        subprocess.run([str(bin/'lib.exe'),'/nologo','/DEF:'+str(definition),'/MACHINE:X64','/OUT:'+str(OUT/(name+'.lib'))],env=env,check=True)
    sources = sorted((ROOT/'Vendor/Box2D').rglob('*.cpp'))+sorted((ROOT/'Sources').glob('*.cpp'))+[ROOT/'Tests/GLESBridge.cpp']
    rsp = OUT/'sources.rsp'
    rsp.write_text('\n'.join('"%s"'%p for p in sources),encoding='utf-8')
    subprocess.run([str(bin/'cl.exe'),'/nologo','/LD','/EHsc','/std:c++14','/O2','/I'+str(includes),
                    '/I'+str(ROOT/'Sources'),'/I'+str(ROOT/'Vendor'),'/Fe:'+str(OUT/'AhogeGLES.dll'),'@'+str(rsp),
                    '/link',str(OUT/'libGLESv2.lib'),str(OUT/'libEGL.lib')],cwd=OUT,env=env,check=True)


def main():
    build()
    os.add_dll_directory(str(ANGLE))
    ct.WinDLL(str(ANGLE/'libGLESv2.dll'))
    ct.WinDLL(str(ANGLE/'libEGL.dll'))
    dll=ct.CDLL(str(OUT/'AhogeGLES.dll'))
    callback_type=ct.CFUNCTYPE(ct.c_void_p,ct.c_char_p,ct.POINTER(ct.c_int),ct.POINTER(ct.c_int))
    buffers=[]

    @callback_type
    def pixels(path,w,h):
        image=np.array(Image.open(path.decode('utf-8')).convert('RGBA'),dtype=np.uint16)
        image[:,:,:3]=(image[:,:,:3]*image[:,:,3:4]+127)//255
        raw=ct.create_string_buffer(image.astype(np.uint8).tobytes())
        buffers.append(raw);w[0]=image.shape[1];h[0]=image.shape[0]
        return ct.addressof(raw)

    dll.ah_init.argtypes=[ct.c_char_p,ct.c_int,ct.c_int,callback_type]
    dll.ah_error.restype=ct.c_char_p
    dll.ah_draw.argtypes=[ct.c_void_p]
    results=[]
    for name,w,h,scene in [('portrait480',320,480,0),('portrait568',320,568,0),('landscape',568,320,0),
                           ('ipad',768,1024,0),('seika1',640,512,1),('seika2',640,512,2),
                           ('seika3',640,512,3),('seika4',640,512,4),('gold-celebration',640,512,5),
                           ('debris240',640,512,6)]:
        assert dll.ah_init(str(ROOT/'Resources/AhogeAssets').encode(),w,h,pixels),dll.ah_error()
        dll.ah_scene(scene)
        raw=ct.create_string_buffer(w*h*4);calls=dll.ah_draw(raw)
        assert calls>0,dll.ah_error()
        image=np.frombuffer(raw,dtype=np.uint8).reshape(h,w,4)[::-1].copy()
        assert len(np.unique(image.reshape(-1,4),axis=0))>500,'blank frame'
        assert image[:,:,3].min()==255
        if w/h!=1.25:
            np.testing.assert_array_equal(image[0,0], [245,132,132,255])
        Image.fromarray(image).save(OUT/(name+'.png'))
        dll.ah_purge();again=ct.create_string_buffer(w*h*4)
        assert dll.ah_draw(again)>0,dll.ah_error()
        assert raw.raw==again.raw,'texture reload changed image/state'
        results.append('%s: %dx%d, %d draw calls, nonblank, texture reload identical'%(name,w,h,calls))
        dll.ah_close();buffers.clear()
    (OUT/'results.txt').write_text('\n'.join(results)+'\n',encoding='utf-8')
    print('\n'.join(results))


if __name__=='__main__':
    main()
