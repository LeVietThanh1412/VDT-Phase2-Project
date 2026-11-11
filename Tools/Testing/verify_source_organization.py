"""Validate source relocation against the independently saved formatted baseline.

This is a structural/API preservation check, not a gameplay or packaging test.
The expected digests exclude include directives only. The four documented UHT
literal repairs are incorporated into the pre-migration reference digests.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re


def non_include_body(data):
    return re.sub(
        rb'^[ \t]*#[ \t]*include[^\r\n]*(?:\r?\n|$)', b'', data, flags=re.M
    )


def verify(root):
    module = root / 'Source/VDTPhase2Project'
    manifest = json.loads(
        (root / 'Docs/Architecture/SOURCE_MIGRATION_MANIFEST.json').read_text(encoding='utf-8')
    )
    rows = manifest['files']
    additions = manifest.get('build_compatibility_additions', [])
    approved_includes = manifest.get('build_compatibility_includes', {})
    expected = {row['new'] for row in rows} | {row['path'] for row in additions}
    actual = {
        path.relative_to(root).as_posix()
        for path in module.rglob('*') if path.suffix in ('.h', '.cpp')
    }
    assert actual == expected, {'missing': sorted(expected - actual), 'extra': sorted(actual - expected)}
    headers = {Path(row['new']).name: (root / row['new']).resolve() for row in rows if row['new'].endswith('.h')}
    for addition in additions:
        path = root / addition['path']
        assert path.is_relative_to(module / 'Private'), 'Compatibility header outside Private'
        digest = hashlib.sha256(path.read_bytes().replace(b'\r\n', b'\n')).hexdigest()
        assert digest == addition['lf_sha256'], 'Compatibility header changed: ' + addition['path']
        headers[path.name] = path.resolve()
    assert set(approved_includes) <= {row['new'] for row in rows}, 'Unknown compatibility include user'
    verified = []
    generated_count = 0
    for row in rows:
        path = root / row['new']
        assert not (root / row['old']).exists(), 'Old source remains: ' + row['old']
        data = path.read_bytes()
        body = non_include_body(data)
        body_digest = hashlib.sha256(body).hexdigest()
        canonical_digest = hashlib.sha256(body.replace(b'\r\n', b'\n')).hexdigest()
        assert canonical_digest == row['preserved_non_include_lf_sha256'], 'Non-include source changed: ' + row['new']
        if path.suffix == '.cpp':
            assert path.is_relative_to(module / 'Private'), 'Implementation outside Private: ' + row['new']
        text = data.decode('utf-8-sig')
        includes = re.findall(r'^\s*#\s*include\s*[<"]([^">]+)[">]', text, re.M)
        canonical = lambda value: Path(value).name if Path(value).name in headers else value
        before = [canonical(value) for value in row['preserved_includes'] if not value.endswith('.generated.h')]
        extra_includes = approved_includes.get(row['new'], [])
        for extra in extra_includes:
            assert includes.count(extra) == 1, 'Missing/duplicate compatibility include: ' + row['new']
            assert (module / 'Private' / extra).resolve() in [headers[Path(item['path']).name] for item in additions]
            cesium_indices = [i for i, value in enumerate(includes) if value.startswith('Cesium')]
            assert cesium_indices and includes.index(extra) < min(cesium_indices), 'Compatibility include follows Cesium'
        after = [canonical(value) for value in includes if not value.endswith('.generated.h') and value not in extra_includes]
        assert before == after, 'Include dependency/order changed: ' + row['new']
        generated = [value for value in includes if value.endswith('.generated.h')]
        if generated:
            assert generated == [path.stem + '.generated.h'], row['new']
            assert includes[-1] == generated[0], 'Generated include is not last: ' + row['new']
            generated_count += 1
        for include in includes:
            name = Path(include).name
            if name not in headers:
                continue
            candidates = [path.parent / include, module / 'Public' / include, module / 'Private' / include]
            assert headers[name] in [value.resolve() for value in candidates], (row['new'], include)
            assert '..' not in Path(include).parts, 'Legacy relative game include: ' + include
            if path.is_relative_to(module / 'Public'):
                assert headers[name].is_relative_to((module / 'Public').resolve()), 'Private header leaked into public API'
        verified.append({
            'path': row['new'], 'preserved_non_include_sha256': body_digest,
            'exact_worktree_body_bytes': body_digest == row['preserved_non_include_sha256'],
        })
    return {
        'status': 'PASSED', 'files': len(verified), 'generated_headers': generated_count,
        'approved_build_compatibility_headers': len(additions),
        'approved_build_compatibility_include_users': len(approved_includes),
        'scope': 'Exact preservation of non-include source against the repaired formatted baseline; paths and generated-header constraints. Not runtime evidence.',
        'verification': verified,
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = verify(Path(__file__).resolve().parents[2])
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(result['status'], result['files'], 'C++ files;', result['generated_headers'], 'generated headers')
