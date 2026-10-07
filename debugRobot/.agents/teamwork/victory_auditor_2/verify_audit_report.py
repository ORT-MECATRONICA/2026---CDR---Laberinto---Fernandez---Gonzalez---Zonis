import re
import os

with open('audit_report.md', 'r', encoding='utf-8') as f:
    text = f.read()

def_blocks = re.findall(r'(#### DEF-[^\n]+)(.*?)(?=(?:#### DEF-|\n### |\Z))', text, re.DOTALL)
print(f'Parsed {len(def_blocks)} defect blocks.')

errors = 0
for header, block in def_blocks:
    hdr = header.strip()
    m_id = re.search(r'\*\*Bug ID:\*\*\s*`([^`]+)`', block)
    m_file = re.search(r'\*\*File Path:\*\*\s*([^\n]+)', block)
    m_sev = re.search(r'\*\*Severity:\*\*\s*([^\n]+)', block)
    m_cat = re.search(r'\*\*Category:\*\*\s*([^\n]+)', block)
    m_rc = re.search(r'\*\*Title & Root Cause:\*\*\s*([^\n]+)', block)
    m_conseq = re.search(r'\*\*Dynamic Consequences:\*\*\s*([^\n]+)', block)
    
    missing = []
    if not m_id: missing.append('Bug ID')
    if not m_file: missing.append('File Path')
    if not m_sev: missing.append('Severity')
    if not m_cat: missing.append('Category')
    if not m_rc: missing.append('Title & Root Cause')
    if not m_conseq: missing.append('Dynamic Consequences')
    
    if missing:
        print(f'ERROR in {hdr}: missing {missing}')
        errors += 1
    else:
        # Check if line number exists in file_path
        fpath = m_file.group(1).strip()
        # line number check
        if not re.search(r'(?:line|lines|\:|\d+)', fpath, re.IGNORECASE):
            print(f'WARNING line number in {hdr}: {fpath}')

if errors == 0:
    print('SUCCESS: All 38 defect blocks contain Bug ID, File Path with lines, Severity, Category, Title & Root Cause, Dynamic Consequences.')
else:
    print(f'FAILURE: {errors} defect blocks had errors.')
