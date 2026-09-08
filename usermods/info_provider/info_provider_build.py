Import('env')

from pathlib import Path
import re

module_dir = Path(env['PROJECT_DIR']) / 'usermods' / 'info_provider'
birthday_file = module_dir / 'birthdays.txt'
generated_header = module_dir / 'info_provider_birthdays.h'

entries = []
line_pattern = re.compile(r'^(0[1-9]|[12][0-9]|3[01])\.(0[1-9]|1[0-2])\|(.+)$')
for line_number, raw_line in enumerate(birthday_file.read_text(encoding='utf-8').splitlines(), 1):
    line = raw_line.strip()
    if not line or line.startswith('#'):
        continue
    match = line_pattern.match(line)
    if not match:
        raise ValueError(f'Invalid birthday entry at {birthday_file}:{line_number}')
    name = match.group(3).replace('\\', '\\\\').replace('"', '\\"')
    entries.append((int(match.group(1)), int(match.group(2)), name))

if len(entries) > 64:
    raise ValueError(f'Too many birthday entries in {birthday_file}; maximum is 64')

with generated_header.open('w', encoding='ascii', newline='\n') as header:
    header.write('#pragma once\n\n')
    header.write('struct InfoProviderBirthdayDefault { uint8_t day; uint8_t month; const char *name; };\n')
    header.write(f'constexpr uint8_t INFO_PROVIDER_BIRTHDAY_DEFAULT_COUNT = {len(entries)};\n')
    header.write('static const InfoProviderBirthdayDefault INFO_PROVIDER_BIRTHDAY_DEFAULTS[] = {\n')
    for day, month, name in entries:
        header.write(f'  {{{day}, {month}, "{name}"}},\n')
    header.write('};\n')

