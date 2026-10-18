#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Add native UI imports to the original engine; keep its renderer unchanged.

The original function stays intact. No live injection, RWX trampoline allocation,
or renderer replacement takes place inside an already-running process.
"""
import hashlib
from pathlib import Path
import struct
import sys


def align(value, alignment):
    return (value + alignment - 1) // alignment * alignment


class PE:
    def __init__(self, content):
        self.data = bytearray(content)
        self.pe = struct.unpack_from('<I', self.data, 0x3c)[0]
        assert self.data[self.pe:self.pe+4] == b'PE\0\0'
        self.opt = self.pe + 24
        assert struct.unpack_from('<H', self.data, self.opt)[0] == 0x20b
        self.count = struct.unpack_from('<H', self.data, self.pe+6)[0]
        self.table = self.opt + struct.unpack_from('<H', self.data, self.pe+20)[0]
        self.section_align, self.file_align = struct.unpack_from('<II', self.data, self.opt+32)
        self.sections = []
        for index in range(self.count):
            at = self.table + index*40
            name = self.data[at:at+8].rstrip(b'\0').decode()
            size, rva, rawsize, raw = struct.unpack_from('<IIII', self.data, at+8)
            self.sections.append((name,size,rva,rawsize,raw))

    def offset(self, rva):
        for _name,size,va,rawsize,raw in self.sections:
            if va <= rva < va+rawsize:
                return raw+rva-va
        raise ValueError('Unmapped PE RVA: ' + hex(rva))

    def directory(self, index):
        return struct.unpack_from('<II', self.data, self.opt+112+index*8)

    def import_descriptors(self):
        rva,_ = self.directory(1)
        at=self.offset(rva);rows=[]
        while any(self.data[at:at+20]):
            rows.append(bytes(self.data[at:at+20]));at+=20
        return rows

    def imports(self):
        names=[]
        for row in self.import_descriptors():
            address=struct.unpack_from('<I',row,12)[0]
            at=self.offset(address);end=self.data.index(0,at)
            names.append(self.data[at:end].decode())
        return names

    def add_section(self,name,payload,flags):
        headers=struct.unpack_from('<I',self.data,self.opt+60)[0]
        at=self.table+self.count*40
        if at+40>headers:
            raise ValueError('No room for an additional section header')
        rva=align(max(va+size for _,size,va,_,_ in self.sections),self.section_align)
        raw=align(len(self.data),self.file_align)
        rawsize=align(len(payload),self.file_align)
        self.data.extend(b'\0'*(raw-len(self.data)))
        self.data.extend(payload);self.data.extend(b'\0'*(rawsize-len(payload)))
        header=struct.pack('<8sIIIIIIHHI',name.encode().ljust(8,b'\0'),len(payload),rva,
                           rawsize,raw,0,0,0,0,flags)
        self.data[at:at+40]=header
        self.count+=1;struct.pack_into('<H',self.data,self.pe+6,self.count)
        struct.pack_into('<I',self.data,self.opt+56,align(rva+len(payload),self.section_align))
        if flags&0x20:
            struct.pack_into('<I',self.data,self.opt+4,struct.unpack_from('<I',self.data,self.opt+4)[0]+rawsize)
        if flags&0x40:
            struct.pack_into('<I',self.data,self.opt+8,struct.unpack_from('<I',self.data,self.opt+8)[0]+rawsize)
        # A desktop application does not require a PE checksum; remove stale value.
        struct.pack_into('<I',self.data,self.opt+64,0)
        self.sections.append((name,len(payload),rva,rawsize,raw))
        return rva,raw


def symbols(pe):
    pointer,count=struct.unpack_from('<II',pe.data,pe.pe+12)
    if not pointer or not count:raise ValueError('El motor no contiene símbolos COFF; se necesita bin/bb-probe.exe de esta versión para adaptar el parche.')
    strings=pointer+count*18
    result={};i=0
    while i<count:
        at=pointer+i*18
        if pe.data[at:at+4]==b'\0'*4:
            off=struct.unpack_from('<I',pe.data,at+4)[0];start=strings+off
            end=pe.data.index(0,start);name=pe.data[start:end].decode('utf-8','replace')
        else:name=pe.data[at:at+8].rstrip(b'\0').decode('utf-8','replace')
        value,section=struct.unpack_from('<Ih',pe.data,at+8)
        if 0<section<=len(pe.sections):result[name]=pe.sections[section-1][2]+value
        i+=1+pe.data[at+17]
    return result


def patch(source,destination):
    original=Path(source).read_bytes();pe=PE(original);sym=symbols(pe)
    if any(s[0]=='.bbuim' for s in pe.sections):raise ValueError('El motor ya tiene este parche de menús.')
    wanted=['bbnet_set_image','pad_read_state','pad_read',
            '_ZN9BbOverlay13CapturesInputEv',
            '_ZN8Frontend9WindowSDL18UpdateMouseCaptureEv',
            'SDL_PollEvent','SDL_GetKeyboardState','loaded_config',
            'runtime_file_translate','runtime_file_mount','runtime_file_unmount',
            'input_config_load','bbgpu_mouse_take','_ZN4Core13BloodborneCam13OnMouseMotionEff','_ZN10BbSettings4SaveEv']
    missing=[s for s in wanted if s not in sym]
    if missing:raise ValueError('Faltan símbolos del motor original: '+', '.join(missing))
    rvas=[sym[s] for s in wanted]
    # Match the actual capture method's layout rather than transplanting a
    # donor window class or treating its private fields as universal.
    cap=pe.offset(rvas[4]);body=bytes(pe.data[cap:cap+0x100])
    import re
    a=re.search(b'\x0f\xb6\x81(.{4})',body,re.S)
    b=re.search(b'\x80\xbe(.{4})\x00',body,re.S)
    if not a or not b:raise ValueError('Cambió la rutina de captura del cursor; hace falta adaptar esta versión.')
    mode=struct.unpack('<I',a[1])[0];focus=struct.unpack('<I',b[1])[0]
    if not (0x20<mode<0x400 and mode<focus<mode+32):raise ValueError('Distribución de ventana no reconocida.')
    reload_code=pe.offset(sym['_ZN8Frontend9WindowSDL24TakeInputReloadRequestedEv'])
    match=re.search(b'\x86\x81(.{4})',bytes(pe.data[reload_code:reload_code+40]),re.S)
    if not match:raise ValueError('Cambió la señal de recarga de controles.')
    reload_offset=struct.unpack('<I',match[1])[0]
    targets={rvas[0]:'PcImageLoaded',rvas[4]:'PcUpdateCapture',rvas[5]:'PcPollEvent',rvas[12]:'PcMouseTake',rvas[13]:'PcMouseMotion',rvas[14]:'PcRenderSave',rvas[11]:'PcInputConfigLoad'}
    sites={target:[] for target in targets}
    for name,_,va,size,raw in pe.sections:
        if name!='.text':continue
        for at in range(raw,raw+size-5):
            if pe.data[at] in (0xe8,0xe9):
                to=va+at-raw+5+struct.unpack_from('<i',pe.data,at+1)[0]
                if to in sites and (pe.data[at]==0xe8 or to==rvas[14]):sites[to].append((at,va+at-raw))
    if any(len(found) not in ((2,) if target==rvas[5] else (1,2) if target in (rvas[13],rvas[11]) else tuple(range(1,65)) if target==rvas[14] else (1,)) for target,found in sites.items()):raise ValueError('Llamadas del motor original no reconocidas: '+repr(sites))
    # Locate this version's scePad export table from its resolver, then validate
    # both names and original function pointers. Existing ASLR relocations stay.
    resolve=pe.offset(sym['runtime_pad_resolve']);body=bytes(pe.data[resolve:resolve+48]);lea=body.find(b'\x48\x8d\x0d')
    if lea<0:raise ValueError('No se reconoce la tabla del mando.')
    table=sym['runtime_pad_resolve']+lea+7+struct.unpack_from('<i',body,lea+3)[0]
    base=struct.unpack_from('<Q',pe.data,pe.opt+24)[0]
    exports={}
    for i in range(11):
        at=pe.offset(table)+i*16;nameptr,fn=struct.unpack_from('<QQ',pe.data,at)
        n=pe.offset(nameptr-base);end=pe.data.index(0,n);name=pe.data[n:end].decode()
        if name in ('scePadRead','scePadReadState'):exports[name]=(at+8,fn)
    assert set(exports)=={'scePadRead','scePadReadState'}
    assert exports['scePadRead'][1]==base+rvas[2]
    assert exports['scePadReadState'][1]==base+rvas[1]
    # Ensure existing loader relocations will fix the replacement table pointers.
    reloc_rva,reloc_size=pe.directory(5);at=pe.offset(reloc_rva);end=at+reloc_size;relocs=set()
    while at<end:
        page,length=struct.unpack_from('<II',pe.data,at)
        if length<8:raise ValueError('Invalid relocation table')
        for off in range(at+8,at+length,2):
            entry=struct.unpack_from('<H',pe.data,off)[0]
            if entry>>12==10:relocs.add(page+(entry&4095))
        at+=length
    for pointer,_ in exports.values():
        rva=table+(pointer-pe.offset(table))
        if rva not in relocs:raise ValueError('La tabla del mando no tiene relocaciones ASLR.')
    protect_import=sym['__imp_VirtualProtect']
    protect_sites=[]
    start=sites[rvas[0]][0][1]
    for rva in range(start,start+0x400):
        at=pe.offset(rva)
        if pe.data[at:at+2]==b'\xff\x15' and rva+6+struct.unpack_from('<i',pe.data,at+2)[0]==protect_import:
            if pe.data[at-6:at]==b'\x41\xb8\x01\x00\x00\x00':protect_sites.append((at,rva))
    if len(protect_sites)!=1:raise ValueError('No se reconoce la protección final de la imagen del juego.')
    names=list(targets.values())+['PcPadReadState','PcPadRead','PcImageProtect']
    rows=pe.import_descriptors();next_rva=align(max(v+s for _,s,v,_,_ in pe.sections),pe.section_align)
    metadata=bytearray((len(rows)+2)*20)
    for i,row in enumerate(rows):metadata[i*20:i*20+20]=row
    def add(blob,alignment=1):
        metadata.extend(b'\0'*(align(len(metadata),alignment)-len(metadata)));rva=next_rva+len(metadata);metadata.extend(blob);return rva
    dll=add(b'bb-native-ui.dll\0')
    ni=[add(b'\0\0'+n.encode()+b'\0',2) for n in names]
    ilt=add(struct.pack('<'+'Q'*(len(ni)+1),*ni,0),8)
    iat=add(struct.pack('<'+'Q'*(len(ni)+1),*ni,0),8)
    struct.pack_into('<IIIII',metadata,len(rows)*20,ilt,0,0,dll,iat)
    add(struct.pack('<8s'+'I'*19,b'BBUIMAP1',4,*rvas,mode,focus,reload_offset),8)
    meta,_=pe.add_section('.bbuim',metadata,0xc0000040);assert meta==next_rva
    code=align(max(v+s for _,s,v,_,_ in pe.sections),pe.section_align);stubs=bytearray()
    for i in range(len(names)):
        stubs.extend(b'\xff\x25'+struct.pack('<i',iat+8*i-(code+len(stubs)+6))+b'\x90'*10)
    jump,_=pe.add_section('.bbuij',stubs,0x60000020)
    changed=[]
    for i,found in enumerate(sites.values()):
        for at,rva in found:
            pe.data[at:at+5]=original[at:at+1]+struct.pack('<i',jump+16*i-(rva+5));changed.append((at,5))
    for at,rva in protect_sites:
        struct.pack_into('<i',pe.data,at+2,iat+names.index('PcImageProtect')*8-(rva+6));changed.append((at,6))
    for name,index in [('scePadReadState',names.index('PcPadReadState')),('scePadRead',names.index('PcPadRead'))]:
        at,_=exports[name];struct.pack_into('<Q',pe.data,at,base+jump+16*index);changed.append((at,8))
    struct.pack_into('<II',pe.data,pe.opt+120,meta,(len(rows)+2)*20)
    # Every byte of each original section is preserved except the three call
    # sites and the two scePad export pointers. Renderer sections are untouched.
    before=PE(original)
    allowed={n for at,length in changed for n in range(at,at+length)}
    for _,_,_,size,raw in before.sections:
        assert all(pe.data[n]==original[n] or n in allowed for n in range(raw,raw+size))
    output=Path(destination);output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(pe.data)
    return {'original_sha256':hashlib.sha256(original).hexdigest(),
            'patched_sha256':hashlib.sha256(pe.data).hexdigest(),
            'calls':{names[i]:[r for _,r in found] for i,found in enumerate(sites.values())},
            'renderer_preserved':True,'mode_offset':mode}

if __name__=='__main__':
    if len(sys.argv)!=3:raise SystemExit('patch_original_engine.py INPUT_ENGINE OUTPUT_ENGINE')
    print(patch(sys.argv[1],sys.argv[2]))
