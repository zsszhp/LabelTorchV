import re

with open(r'E:\z\project\my\LabelTorchV\src\features\training\qml\TrainingPage.qml', 'r', encoding='utf-8') as f:
    lines = f.readlines()

depth = 0
for i, line in enumerate(lines, 1):
    stripped = line.strip()
    if stripped.startswith('//'):
        continue
    code = re.sub(r'//.*$', '', line)
    code = re.sub(r"'.*?'", "''", code)
    code = re.sub(r'".*?"', '""', code)
    for ch in code:
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth < 0:
                print(f'ERROR: depth went negative at line {i}: {stripped}')
                break
    if depth < 0:
        break
    if i % 100 == 0:
        print(f'Line {i}: depth={depth}')

print(f'Final depth: {depth}')
if depth > 0:
    print(f'Missing {depth} closing braces')
elif depth == 0:
    print('All braces matched')
