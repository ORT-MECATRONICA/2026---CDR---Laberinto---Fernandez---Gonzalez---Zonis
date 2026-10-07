import re
import os

with open('audit_report.md', 'r', encoding='utf-8') as f:
    text = f.read()

def_blocks = re.findall(r'(#### DEF-[^\n]+)(.*?)(?=(?:#### DEF-|\n### |\Z))', text, re.DOTALL)
print(f'Total defect blocks: {len(def_blocks)}')

results = []

for header, block in def_blocks:
    bug_id_m = re.search(r'\*\*Bug ID:\*\*\s*`([^`]+)`', block)
    bug_id = bug_id_m.group(1) if bug_id_m else header.strip()
    
    file_path_m = re.search(r'\*\*File Path:\*\*\s*([^\n]+)', block)
    if not file_path_m:
        results.append((bug_id, False, 'No file path found'))
        continue
    
    fp_str = file_path_m.group(1).strip()
    
    # Extract paths and lines. Could have multiple e.g. "src/main.cpp (Lines 66-98) vs src/config.h (Lines 66-75)"
    # Or "src/puenteH.cpp:39-56"
    matches = re.findall(r'([a-zA-Z0-9_\-\./\\]+\.(?:cpp|h|ini|txt))\s*(?:\(?[Ll]ines?\s*([0-9]+)(?:[\s–\-]*(?:[0-9]+))?\)?)?', fp_str)
    
    if not matches:
        # try colon format e.g. path:123
        matches = re.findall(r'([a-zA-Z0-9_\-\./\\]+\.(?:cpp|h|ini|txt)):([0-9]+)', fp_str)
        
    if not matches:
        results.append((bug_id, False, f'Could not parse file path from: {fp_str}'))
        continue
        
    all_ok = True
    details = []
    for m in matches:
        fname = m[0].replace('\\', '/')
        line_num = int(m[1]) if len(m) > 1 and m[1] else None
        
        # Check if file exists
        if not os.path.exists(fname):
            all_ok = False
            details.append(f'File {fname} does NOT exist on disk!')
            continue
            
        # Check lines
        with open(fname, 'r', encoding='utf-8', errors='ignore') as src_f:
            src_lines = src_f.readlines()
            
        if line_num is not None:
            if line_num > len(src_lines) or line_num < 1:
                all_ok = False
                details.append(f'Line {line_num} out of bounds for {fname} (total {len(src_lines)})')
            else:
                details.append(f'{fname}:{line_num} exists (file has {len(src_lines)} lines)')
        else:
            details.append(f'{fname} exists ({len(src_lines)} lines)')
            
    results.append((bug_id, all_ok, '; '.join(details)))

passed = 0
failed = 0
for bug_id, ok, det in results:
    if ok:
        passed += 1
        print(f'[PASS] {bug_id}: {det}')
    else:
        failed += 1
        print(f'[FAIL] {bug_id}: {det}')

print(f'\nTotal: {passed} passed, {failed} failed.')
