# SPDX-License-Identifier: GPL-3.0-or-later
import unittest,re
from pathlib import Path
import bbport_integrated_localization as loc
class GameLanguages(unittest.TestCase):
 def test_every_native_action_has_eight_labels_and_action_descriptions(self):
  source=(Path(__file__).parents[1]/'pc_ui/pages/bindings.cpp').read_text()
  actions=re.findall(r'\{"(\w+)", "([^"]+)", "([^"]+)"',source)
  self.assertEqual(len(actions),31)
  for _,label,helptext in actions:
   for text in (label,helptext):
    self.assertIn(text,loc.TRANSLATIONS)
    row=loc.TRANSLATIONS[text];self.assertEqual(len(row),8)
    self.assertTrue(all(row));self.assertEqual(row[0],text)
   self.assertFalse(helptext.startswith(('Cross.','Circle.','Square.','Triangle.','R1.','R2.','L1.','L2.','D-pad')))
 def test_game_language_aliases_and_active_messages_are_complete(self):
  for index,language in enumerate(('engus','spaes','rusru','jpnjp','frafr','deude','itait','porbr')):
   self.assertEqual(loc.language_index(language),index)
   messages=loc.message_catalog(language)
   for mid,(caption,helptext) in loc.ACTIVE_MESSAGES.items():
    self.assertEqual(messages[mid],tuple(loc.TRANSLATIONS[v][index] if v else '' for v in (caption,helptext)))
  self.assertEqual(loc.message_catalog('spaar'),loc.message_catalog('spaes'))
  self.assertEqual(loc.message_catalog('enggb'),loc.message_catalog('engus'))
  self.assertEqual(loc.message_catalog('korkr'),{})
 def test_cpp_catalog_matches_source(self):
  native=(Path(__file__).parents[1]/'pc_ui/pages/language_text.h').read_text()
  for values in loc.TRANSLATIONS.values():
   for value in values:
    # C++ string literals escape line breaks; message bundle text remains UTF-16.
    import json
    self.assertIn(json.dumps(value,ensure_ascii=False),native)
if __name__=='__main__':unittest.main()
