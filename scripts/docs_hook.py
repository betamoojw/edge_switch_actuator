"""Keep historical evidence explicit and prevent silent current-page fallback."""
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
POLICY = json.loads((ROOT / 'docs-languages.json').read_text(encoding='utf-8'))


def on_config(config):
    for path in (ROOT / 'docs').glob('*.md'):
        name = path.stem.removesuffix('.zh-TW').removesuffix('.zh')
        if name not in POLICY['current'] + POLICY['historical']:
            raise ValueError(f'Classify documentation in docs-languages.json: {path.name}')
    for name in POLICY['current']:
        for locale in ('en', 'zh', 'zh-TW'):
            suffix = '' if locale == 'en' else '.' + locale
            path = ROOT / 'docs' / (name + suffix + '.md')
            if not path.is_file():
                raise ValueError(f'Missing current documentation translation: {path.name}')
    return config


def on_page_markdown(markdown, page, config, files):
    path = page.file.src_uri
    is_history = path.startswith('tasks/') or Path(path).stem in POLICY['historical']
    if is_history:
        locale = config.plugins['i18n'].current_language
        messages = {
            'en': 'Historical record — retained in its original language. Dates, versions and test results below describe the original work, not current validation. Private network addresses and device identities may be replaced with documentation placeholders. Use the current guides for operating instructions.',
            'zh': '历史记录：以下内容保留原始语言，未作为当前文档的译文。日期、版本及测试结果仅对应当时的工作；私有网络地址和设备标识可能已替换为文档占位值。实际操作请以当前使用指南为准。',
            'zh-TW': '歷史紀錄：以下保留原文，並非目前文件的翻譯版本。文中的日期、版本與測試結果僅適用於當時的工作；私人網路位址與裝置識別資訊可能已替換為文件範例值。操作時請參閱現行指南。',
        }
        markdown = '> ' + messages.get(locale, messages['en']) + '\n\n' + markdown
    return markdown


def on_post_build(config):
    """Expose the deployment revision without credentials or build-environment data."""
    marker = {
        'documentation_commit': os.environ.get('GITHUB_SHA', 'local-preview'),
        'languages': ['en', 'zh', 'zh-TW'],
    }
    for locale in ('', 'zh', 'zh-TW'):
        destination = Path(config.site_dir) / locale
        destination.mkdir(parents=True, exist_ok=True)
        (destination / 'build-info.json').write_text(
            json.dumps(marker, indent=2) + '\n', encoding='utf-8'
        )
