# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import json,sys
source,destination=map(Path,sys.argv[1:])
destination.parent.mkdir(parents=True,exist_ok=True)
destination.write_text(json.dumps(source.read_text(encoding='utf-8'),ensure_ascii=False),encoding='utf-8')
