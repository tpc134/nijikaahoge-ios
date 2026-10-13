"""Generate legacy launch PNGs and icon sizes from existing supplied artwork."""
import argparse
import json
import pathlib
from PIL import Image

ROOT=pathlib.Path(__file__).resolve().parents[1]


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--icon',type=pathlib.Path,required=True)
    args=ap.parse_args()
    launches=ROOT/'Resources/LegacyLaunch'
    launches.mkdir(exist_ok=True)
    # Traditional iPad launch sizes exclude the legacy status-bar strip.
    for name,size in [('Default.png',(320,480)),('Default@2x.png',(640,960)),
                      ('Default-568h@2x.png',(640,1136)),
                      ('Default-Portrait~ipad.png',(768,1004)),
                      ('Default-Portrait@2x~ipad.png',(1536,2008)),
                      ('Default-Landscape~ipad.png',(1024,748)),
                      ('Default-Landscape@2x~ipad.png',(2048,1496))]:
        Image.new('RGB',size,(245,132,132)).save(launches/name)
    folder=ROOT/'Resources/AppIcon.appiconset'
    folder.mkdir(exist_ok=True)
    original=Image.open(args.icon).convert('RGBA')
    base=Image.new('RGBA',original.size,(245,132,132,255));base.alpha_composite(original);base=base.convert('RGB')
    images=[]
    for idiom,size,scales in [('iphone',20,(2,3)),('iphone',29,(1,2,3)),('iphone',40,(2,3)),
                             ('iphone',57,(1,2)),('iphone',60,(2,3)),
                             ('ipad',20,(1,2)),('ipad',29,(1,2)),('ipad',40,(1,2)),
                             ('ipad',50,(1,2)),('ipad',72,(1,2)),('ipad',76,(1,2)),('ipad',83.5,(2,))]:
        for scale in scales:
            name='%s-%g-%dx.png'%(idiom,size,scale)
            dim=int(size*scale);base.resize((dim,dim),Image.Resampling.LANCZOS).save(folder/name)
            images.append({'idiom':idiom,'size':'%gx%g'%(size,size),'scale':'%dx'%scale,'filename':name})
    (folder/'Contents.json').write_text(json.dumps({'images':images,'info':{'version':1,'author':'xcode'}},indent=2),encoding='ascii')
    print('Generated 7 legacy launch PNGs and %d icon entries.'%len(images))


if __name__=='__main__':
    main()
