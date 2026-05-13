import os, glob

replacements = {
    '"#1e1e2e"': 'Theme.bgPrimary',
    '"#181825"': 'Theme.bgCard',
    '"#313244"': 'Theme.bgInput',
    '"#cdd6f4"': 'Theme.textPrimary',
    '"#a6adc8"': 'Theme.textSecondary',
    '"#6c7086"': 'Theme.textMuted',
    '"#89b4fa"': 'Theme.accentPrimary',
    '"#f38ba8"': 'Theme.accentError',
    '"#a6e3a1"': 'Theme.accentSuccess',
    '"#f9e2af"': 'Theme.accentWarning',
    '"#45475a"': 'Theme.borderNormal',
}

qml_dir = r'f:\project\my\LabelTorchV\src\features\dataset\qml'
for fpath in glob.glob(os.path.join(qml_dir, '*.qml')):
    with open(fpath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    for old, new in replacements.items():
        content = content.replace(old, new)
        
    if 'import LabelTorch.Shell' not in content:
        content = content.replace('import QtQuick.Controls\n', 'import QtQuick.Controls\nimport LabelTorch.Shell\n')
        
    with open(fpath, 'w', encoding='utf-8') as f:
        f.write(content)
print('Done!')
