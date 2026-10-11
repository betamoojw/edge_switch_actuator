"""Offline checks for published documentation, translations, links and images."""
import json
import re
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit, urljoin

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / 'site'
POLICY = json.loads((ROOT / 'docs-languages.json').read_text(encoding='utf-8'))


class Page(HTMLParser):
    def __init__(self, source):
        super().__init__()
        self.ids = set()
        self.links = []
        self.images = []
        self.languages = {}
        self.feed(source)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if attrs.get('id'):
            self.ids.add(attrs['id'])
        if tag == 'a' and attrs.get('href'):
            self.links.append(attrs['href'])
            if attrs.get('hreflang'):
                self.languages[attrs['hreflang']] = attrs['href']
        if tag == 'img' and attrs.get('src'):
            self.images.append((attrs['src'], attrs.get('alt', '')))


def main():
    errors = []
    pages = {p.resolve(): Page(p.read_text(encoding='utf-8')) for p in SITE.rglob('*.html')}
    if not pages:
        raise SystemExit('Build the site first: python -m mkdocs build --strict')
    for name in POLICY['current']:
        for locale in ('en', 'zh', 'zh-TW'):
            suffix = '' if locale == 'en' else '.' + locale
            source = ROOT / 'docs' / (name + suffix + '.md')
            prefix = SITE if locale == 'en' else SITE / locale
            output = prefix / ('index.html' if name == 'index' else name + '/index.html')
            if not source.is_file() or not output.is_file():
                errors.append(f'Missing translation/output: {name}/{locale}')
                continue
            text = source.read_text(encoding='utf-8')
            if locale != 'en' and len(re.findall(r'[\u3400-\u9fff]', text)) < 100:
                errors.append(f'Untranslated current page: {source.name}')
            html = output.read_text(encoding='utf-8')
            if 'md-select' not in html:
                errors.append(f'No language selector: {output}')
            page_url = '' if name == 'index' else name + '/'
            current_url = 'https://docs.example/edge_switch_actuator/' + ('' if locale == 'en' else locale + '/') + page_url
            for destination_locale, destination in [('en', ''), ('zh', 'zh/'), ('zh-TW', 'zh-TW/')]:
                expected = '/edge_switch_actuator/' + destination + page_url
                href = pages[output.resolve()].languages.get(destination_locale, '')
                if not href or urlsplit(urljoin(current_url, href)).path != expected:
                    errors.append(f'{name}/{locale}: missing equivalent-page link {expected}')
    private_patterns = [
        r'192\.168\.(?!4\.1\b)\d+\.\d+',
        r'\bgh[pousr]_[A-Za-z0-9]{25,}',
        r'\bgithub_pat_[A-Za-z0-9_]{30,}',
        r'\beyJ[A-Za-z0-9_-]{20,}\.[A-Za-z0-9_-]{15,}\.[A-Za-z0-9_-]{15,}',
    ]
    for source in (ROOT / 'docs').rglob('*.md'):
        for line_number, line in enumerate(source.read_text(encoding='utf-8').splitlines(), 1):
            if any(re.search(pattern, line) for pattern in private_patterns):
                # Report locations only, never reproduce a potential secret in logs.
                errors.append(f'Potential private endpoint/token: {source.name}:{line_number}')
    for path, page in pages.items():
        for link in page.links + [src for src, _ in page.images]:
            parsed = urlsplit(link)
            if parsed.scheme or parsed.netloc:
                continue
            if parsed.path.startswith('/edge_switch_actuator/'):
                target = (SITE / unquote(parsed.path[len('/edge_switch_actuator/'):])).resolve()
            elif parsed.path.startswith('/'):
                errors.append(f'{path.relative_to(SITE)}: unexpected absolute link {link}')
                continue
            else:
                target = (path.parent / unquote(parsed.path)).resolve() if parsed.path else path
            if target.is_dir():
                target = target / 'index.html'
            if not target.exists():
                errors.append(f'{path.relative_to(SITE)}: missing {link}')
            elif parsed.fragment and target in pages and unquote(parsed.fragment) not in pages[target].ids:
                errors.append(f'{path.relative_to(SITE)}: missing anchor {link}')
        for src, alt in page.images:
            if not alt.strip():
                errors.append(f'{path.relative_to(SITE)}: missing image alt: {src}')
    for locale, query in [('en', 'relay'), ('zh', '继电器'), ('zh-TW', '繼電器')]:
        # mkdocs-static-i18n merges every language into Material's root index.
        index_path = SITE / 'search/search_index.json'
        if not index_path.exists():
            errors.append(f'Missing search index: {locale}')
        else:
            data = json.loads(index_path.read_text(encoding='utf-8'))
            entries = [entry for entry in data['docs'] if
                       (not entry['location'].startswith(('zh/', 'zh-TW/')) if locale == 'en'
                        else entry['location'].startswith(locale + '/'))]
            if query not in json.dumps(entries, ensure_ascii=False).replace('\u200b', ''):
                errors.append(f'No localized search content: {locale}')
    if errors:
        print('\n'.join(errors))
        raise SystemExit(f'Documentation validation failed: {len(errors)} issues')
    print(f'PASS: {len(POLICY["current"])} current pages in 3 languages; {len(pages)} HTML pages; local links/anchors, image alt text, selectors and localized search indexes.')


if __name__ == '__main__':
    main()
