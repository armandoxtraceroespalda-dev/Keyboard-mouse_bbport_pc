# SPDX-License-Identifier: GPL-3.0-or-later
import tempfile
from pathlib import Path
from unittest import TestCase, main
from unittest.mock import patch
import bbport_integrated_mirror_assets as assets

class MessageViewTests(TestCase):
 def exercise(self, copy_only):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);game=root/'mod-view';cache=root/'cache'
   for i in range(22):
    relative=Path(f'lang{i}')
    source=game/'dvdroot_ps4/msg'/relative;prepared=cache/'dvdroot_ps4/msg'/relative
    source.mkdir(parents=True);prepared.mkdir(parents=True)
    (source/'menu.msgbnd.dcx').write_bytes(b'existing-quit-text-and-mods')
    (source/'item.msgbnd.dcx').write_bytes(b'keep-other-messages')
    (prepared/'menu.msgbnd.dcx').write_bytes(b'pc-menu-with-quit-text-and-mods')
   def build():return assets.prepare_message_view(game,cache)
   if copy_only:
    with patch.object(assets.os,'link',side_effect=OSError('different volume')):view=build()
   else:view=build()
   for i in range(22):
    self.assertEqual((view/f'lang{i}/menu.msgbnd.dcx').read_bytes(),b'pc-menu-with-quit-text-and-mods')
    self.assertEqual((view/f'lang{i}/item.msgbnd.dcx').read_bytes(),b'keep-other-messages')
    self.assertEqual((game/f'dvdroot_ps4/msg/lang{i}/menu.msgbnd.dcx').read_bytes(),b'existing-quit-text-and-mods')
   # Rebuild must remove stale languages and preserve changes to existing mods.
   import shutil
   shutil.rmtree(game/'dvdroot_ps4/msg/lang21');shutil.rmtree(cache/'dvdroot_ps4/msg/lang21')
   (game/'dvdroot_ps4/msg/lang0/item.msgbnd.dcx').write_bytes(b'updated mod')
   build()
   self.assertFalse((view/'lang21').exists())
   self.assertEqual((view/'lang0/item.msgbnd.dcx').read_bytes(),b'updated mod')
 def test_hardlinks_never_modify_original_menus(self):self.exercise(False)
 def test_copy_fallback_preserves_all_other_messages(self):self.exercise(True)

if __name__=='__main__':main(verbosity=2)
