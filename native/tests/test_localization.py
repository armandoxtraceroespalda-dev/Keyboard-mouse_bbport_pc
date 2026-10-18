# SPDX-License-Identifier: GPL-3.0-or-later
import struct,unittest
import bbport_integrated_localization as loc
import bbport_integrated_mirror_assets as assets
class Localization(unittest.TestCase):
 def bundle(self):
  rows={119020:'Key Bindings',124020:'PC Enhancements',118020:'PC Controls',121020:'PC Camera',124001:"Hunter's Dream Mirror",888:'Original game text',889:None}
  files=[(30,loc.fmg_write({70000006:'Put on Disguise',70000007:'Keep this text'})),(200,loc.fmg_write(rows)),(201,loc.fmg_write({119020:'',124001:'Requires a restart.'})),(999,b'untouched bytes')]
  head=bytearray(64);head[:4]=b'BND4';struct.pack_into('<I',head,12,len(files));struct.pack_into('<Q',head,32,36)
  names=b''.join(f'{group}.fmg\0'.encode() for group,_ in files);start=64+len(files)*36+len(names);struct.pack_into('<Q',head,40,start);out=head;blob=b'';nameoff=64+len(files)*36
  for group,data in files:
   out+=struct.pack('<QQQIII',0,len(data),len(data),start+len(blob),group,nameoff);blob+=data;nameoff+=len(f'{group}.fmg')+1
  return bytes(out+names+blob)
 def entries(self,b):
  return {loc.u32(b,e+28):b[loc.u32(b,e+24):loc.u32(b,e+24)+loc.u64(b,e+8)] for e in range(64,64+loc.u32(b,12)*loc.u64(b,32),loc.u64(b,32))}
 def test_language_is_game_bundle(self):
  b=self.bundle()
  for lang in ('frafr','deude','rusru','jpnjp','itait','porbr'):
   f=self.entries(loc.localize_bundle(b,lang));index=loc.language_index(lang)
   self.assertEqual(loc.fmg_read(f[200])[119020],loc.TRANSLATIONS['Key Bindings'][index])
   self.assertEqual(loc.fmg_read(f[30])[70000006],loc.TRANSLATIONS['Change appearance'][index])
   self.assertEqual(loc.fmg_read(f[200])[888],'Original game text')
   self.assertEqual(f[999],b'untouched bytes')
  for lang in ('engus','enggb'):
   f=self.entries(loc.localize_bundle(b,lang));self.assertEqual(loc.fmg_read(f[30])[70000006],'Change appearance');self.assertEqual(loc.fmg_read(f[200])[119020],'Key Bindings')
  for lang in ('spaes','spaar'):
   f=self.entries(loc.localize_bundle(b,lang));rows=loc.fmg_read(f[200])
   self.assertEqual(rows[119020],'Asignación de teclas');self.assertEqual(rows[124020],'Mejoras de PC');self.assertEqual(rows[118020],'Controles de PC');self.assertEqual(rows[121020],'Ajustes de cámara')
   self.assertEqual(rows[888],'Original game text');self.assertIsNone(rows[889]);self.assertEqual(f[999],b'untouched bytes')
   self.assertEqual(loc.fmg_read(f[30])[70000006],'Cambiar aspecto');self.assertEqual(loc.fmg_read(f[30])[70000007],'Keep this text')
   self.assertIn('Requiere reiniciar',loc.fmg_read(f[201])[124001])
 def test_mirror_only_does_not_translate_pc_options(self):
  for lang,label in [('spaes','Cambiar aspecto'),('spaar','Cambiar aspecto'),('engus','Change appearance'),('enggb','Change appearance')]:
   f=self.entries(loc.localize_bundle(self.bundle(),lang,pc_options=False))
   self.assertEqual(loc.fmg_read(f[30])[70000006],label)
   self.assertEqual(loc.fmg_read(f[200])[119020],'Key Bindings')
 def test_roundtrip_accents_null_and_dcx(self):
  rows={1:'Cámara y ratón',2:None,3:'',19:'¿Sí?\nEspañol'}
  b=loc.fmg_write(rows);self.assertEqual(loc.fmg_read(b),rows);self.assertEqual(assets.dcx_unpack(assets.dcx_pack(b)),b)
 def test_bad_headers_rejected(self):
  with self.assertRaises(ValueError):loc.localize_bundle(b'bad','spaes')
if __name__=='__main__':unittest.main()
