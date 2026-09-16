"""Local CPU-only regression: python SceneAssetCookerTests.py <AssetPipelineCmd.exe>."""

import base64
import copy
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import uuid
import zlib


def fixture():
    data = bytearray()
    views, accessors = [], []

    def accessor(values, fmt, kind, count, component):
        while len(data) % 4:
            data.append(0)
        blob = struct.pack('<' + fmt * len(values), *values)
        views.append(dict(buffer=0, byteOffset=len(data), byteLength=len(blob)))
        data.extend(blob)
        accessors.append(dict(bufferView=len(views) - 1, componentType=component, count=count, type=kind))
        return len(accessors) - 1

    primitives = []
    for offset in (0, 2, 4):
        position = accessor([offset, 0, 0, offset + 1, 0, 0, offset, 1, 0], 'f', 'VEC3', 3, 5126)
        accessors[position].update(min=[offset, 0, 0], max=[offset + 1, 1, 0])
        normal = accessor([0, 0, 1] * 3, 'f', 'VEC3', 3, 5126)
        uv = accessor([0, 0, 1, 0, 0, 1], 'f', 'VEC2', 3, 5126)
        indices = accessor([0, 1, 2], 'H', 'SCALAR', 3, 5123)
        primitives.append(dict(attributes=dict(POSITION=position, NORMAL=normal, TEXCOORD_0=uv), indices=indices, material=len(primitives) % 2))
    return dict(asset={'version': '2.0'}, scene=0, scenes=[{'nodes': [0]}],
                nodes=[dict(name='Root', children=[1, 2, 3], translation=[10, 0, 0]),
                       dict(name='First', mesh=0, translation=[0, 2, 0]),
                       dict(name='Shared', mesh=0, matrix=[1, 0, 0, 0, .5, 1, 0, 0, 0, 0, 1, 0, 0, 0, 3, 1]),
                       dict(name='Second', mesh=1), dict(name='Excluded', mesh=1)],
                meshes=[dict(name='A', primitives=primitives[:2]), dict(name='B', primitives=primitives[2:])],
                images=[{'uri': 'albedo.dds'}], textures=[{'source': 0}],
                materials=[dict(name='Red', pbrMetallicRoughness={'baseColorFactor': [1, 0, 0, 1], 'baseColorTexture': {'index': 0}}),
                           dict(name='Green', pbrMetallicRoughness={'baseColorFactor': [0, 1, 0, 1]})],
                buffers=[dict(byteLength=len(data), uri='data:application/octet-stream;base64,' + base64.b64encode(data).decode())],
                bufferViews=views, accessors=accessors)


def run(executable):
    work = Path(tempfile.mkdtemp(prefix='sceneasset-', dir=executable.parent.parent))
    source = work / 'scene.gltf'
    output = work / 'cooked'
    output.mkdir()
    metadata = work / 'scene.gltf.asset.json'
    asset = output / 'scene.sceneasset.json'
    document = fixture()
    document['materials'][0].update(alphaMode='MASK', alphaCutoff=0.35, doubleSided=True, emissiveFactor=[0.1, 0.2, 0.3])
    header = [124, 0x100f, 1, 1, 4, 0, 0] + [0] * 11 + [32, 0x41, 0, 32, 0xff, 0xff00, 0xff0000, 0xff000000] + [0x1000, 0, 0, 0, 0]
    (work / 'albedo.dds').write_bytes(b'DDS ' + struct.pack('<31I', *header) + bytes([255, 255, 255, 255]))

    def cook(force=False, succeeds=True):
        source.write_text(json.dumps(document), encoding='utf-8')
        command = [str(executable), '-pgltf', '--input-file', source.as_posix(), '--output', output.as_posix()]
        if force:
            command.append('--force')
        result = subprocess.run(command, cwd=work, capture_output=True, text=True, errors='replace', timeout=30)
        if result.returncode != (0 if succeeds else 1):
            raise AssertionError(f'exit={result.returncode:#x}\n{result.stdout}{result.stderr}')
        return json.loads(asset.read_text()) if succeeds else None

    first = cook()
    assert first['format'] == 'Horizon.SceneAsset' and first['version'] == 1
    assert uuid.UUID(first['id']).version == 4
    assert first['materials'][0]['baseColorTexture'] == first['textures'][0]['id']
    assert first['materials'][1]['baseColorTexture'] is None
    assert first['materials'][0]['alphaMode'] == 1 and abs(first['materials'][0]['alphaCutoff'] - 0.35) < 1e-6
    assert first['materials'][0]['doubleSided']
    assert all(abs(a - b) < 1e-6 for a, b in zip(first['materials'][0]['emissiveFactor'], [0.1, 0.2, 0.3]))
    identity_hash = 14695981039346656037
    for byte in metadata.read_bytes():
        identity_hash = ((identity_hash ^ byte) * 1099511628211) & ((1 << 64) - 1)
    assert first['identityHash'] == f'{identity_hash:016x}'
    assert [n['parent'] for n in first['nodes']] == [-1, 0, 0, 0]
    assert first['nodes'][0]['translation'] == [10, 0, 0]
    assert first['nodes'][1]['translation'] == [0, 2, 0]
    assert first['nodes'][2]['matrix'][4] == .5 and 'translation' not in first['nodes'][2]
    assert first['nodes'][1]['mesh'] == first['nodes'][2]['mesh']
    assert len(first['meshes']) == 2 and len(first['meshes'][0]['submeshes']) == 2
    assert [s['draw'] for m in first['meshes'] for s in m['submeshes']] == [0, 1, 2]
    assert json.loads((output / 'scene.scene.json').read_text())['version'] == 1
    assert (output / 'scene.bin').read_bytes().startswith(b'GeometryTF')
    before = [p.stat().st_mtime_ns for p in (metadata, asset, output / 'scene.bin')]
    assert cook() == first
    assert before == [p.stat().st_mtime_ns for p in (metadata, asset, output / 'scene.bin')]
    assert cook(force=True) == first
    # Reject invalid source data before replacing any published scene files.
    valid_document = copy.deepcopy(document)
    published_files = (metadata, asset, output / 'scene.bin', output / 'scene.scene.json')
    published_bytes = [p.read_bytes() for p in published_files]
    invalid_sources = []
    document['nodes'][1]['rotation'] = [0, 0, 0, 2]
    invalid_sources.append(document)
    document = copy.deepcopy(valid_document)
    document['nodes'][2]['matrix'][15] = 0
    invalid_sources.append(document)
    document = copy.deepcopy(valid_document)
    document['materials'][0]['pbrMetallicRoughness']['roughnessFactor'] = -1
    invalid_sources.append(document)
    document = copy.deepcopy(valid_document)
    document['nodes'][4]['children'] = [4]  # Cycle outside the selected scene must also fail.
    invalid_sources.append(document)
    document = copy.deepcopy(valid_document)
    buffer = bytearray(base64.b64decode(document['buffers'][0]['uri'].split(',')[1]))
    indices = document['accessors'][document['meshes'][0]['primitives'][0]['indices']]
    offset = document['bufferViews'][indices['bufferView']]['byteOffset']
    struct.pack_into('<H', buffer, offset, 65535)
    document['buffers'][0]['uri'] = 'data:application/octet-stream;base64,' + base64.b64encode(buffer).decode()
    invalid_sources.append(document)
    document = copy.deepcopy(valid_document)
    (work / 'truncated.bin').write_bytes(buffer[:-1])
    document['buffers'][0]['uri'] = 'truncated.bin'
    invalid_sources.append(document)
    for document in invalid_sources:
        cook(succeeds=False)
        assert published_bytes == [p.read_bytes() for p in published_files]
    document = valid_document
    assert cook() == first
    document['meshes'][0]['primitives'].append(copy.deepcopy(document['meshes'][1]['primitives'][0]))
    grown = cook()
    assert grown['meshes'][0]['id'] == first['meshes'][0]['id']
    assert [s['id'] for s in grown['meshes'][0]['submeshes'][:2]] == [s['id'] for s in first['meshes'][0]['submeshes']]
    document['meshes'][0]['primitives'].pop()
    cook()
    records = json.loads(metadata.read_text())['assets']
    mesh = next(r for r in records if r['id'] == first['meshes'][0]['id'])
    assert any(s['id'] == grown['meshes'][0]['submeshes'][2]['id'] and s['missing'] for s in mesh['submeshes'])
    document['meshes'].reverse()
    document['materials'].reverse()
    for node in document['nodes']:
        if 'mesh' in node:
            node['mesh'] = 1 - node['mesh']
    for mesh in document['meshes']:
        mesh['primitives'].reverse()
        for primitive in mesh['primitives']:
            primitive['material'] = 1 - primitive['material']
    reordered = cook()
    assert {m['name']: m['id'] for m in reordered['meshes']} == {m['name']: m['id'] for m in first['meshes']}
    assert {m['name']: m['id'] for m in reordered['materials']} == {m['name']: m['id'] for m in first['materials']}
    assert [s['id'] for s in reordered['meshes'][1]['submeshes']] == [s['id'] for s in reversed(first['meshes'][0]['submeshes'])]
    document['meshes'][1]['name'] = 'Renamed'
    assert cook()['meshes'][1]['id'] == first['meshes'][0]['id']
    removed = document['meshes'].pop(0)
    for node in document['nodes']:
        if node.get('mesh') == 0:
            del node['mesh']
        elif 'mesh' in node:
            node['mesh'] = 0
    cook()
    records = json.loads(metadata.read_text())['assets']
    assert any(r['id'] == first['meshes'][1]['id'] and r['missing'] for r in records)
    document['meshes'].append(removed)
    restored = cook()
    assert restored['meshes'][1]['id'] != first['meshes'][1]['id']
    old_id = restored['meshes'][0]['id']
    del document['meshes'][0]['name']
    document['meshes'].append(copy.deepcopy(document['meshes'][0]))
    ambiguous = cook()
    assert ambiguous['meshes'][0]['id'] != old_id and ambiguous['meshes'][2]['id'] != old_id
    assert ambiguous['meshes'][0]['id'] != ambiguous['meshes'][2]['id']
    document['meshes'][0]['extras'] = {'horizonId': 'exporter-mesh-1'}
    keyed = cook()
    document['meshes'][0]['name'] = 'Persistent rename'
    document['meshes'][0]['primitives'].pop()
    assert cook()['meshes'][0]['id'] == keyed['meshes'][0]['id']
    saved = metadata.read_bytes()
    broken = json.loads(saved)
    broken['assets'][1]['id'] = broken['assets'][0]['id']
    metadata.write_text(json.dumps(broken), encoding='utf-8')
    invalid = metadata.read_bytes()
    published = asset.read_bytes()
    geometry = (output / 'scene.bin').read_bytes()
    cook(force=True, succeeds=False)
    assert metadata.read_bytes() == invalid and asset.read_bytes() == published
    assert (output / 'scene.bin').read_bytes() == geometry
    metadata.write_bytes(saved)
    metadata.rename(work / 'saved.asset.json')
    cook(force=True, succeeds=False)
    assert not metadata.exists() and asset.read_bytes() == published
    print('SceneAsset cooker regressions passed:', work)
    texture_regressions(executable, work / 'textures')


def png(red=255):
    def chunk(kind, value):
        return struct.pack('>I', len(value)) + kind + value + struct.pack('>I', zlib.crc32(kind + value))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', 4, 4, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress((b'\0' + bytes([red, 80, 40, 255]) * 4) * 4)) + chunk(b'IEND', b''))


def texture_regressions(executable, work):
    work.mkdir()
    output = work / 'cooked'
    output.mkdir()
    document = fixture()
    document['images'] = [dict(name='Embedded', uri='data:image/png;base64,' + base64.b64encode(png()).decode())]
    source = work / 'one.gltf'
    metadata = work / 'one.gltf.asset.json'
    asset = output / 'one.sceneasset.json'

    def cook(name='one', succeeds=True, force=False):
        path = work / (name + '.gltf')
        path.write_text(json.dumps(document), encoding='utf-8')
        command = [str(executable), '-pgltf', '--input-file', path.as_posix(), '--output', output.as_posix()]
        if force:
            command.append('--force')
        result = subprocess.run(command, cwd=work, capture_output=True, text=True, errors='replace', timeout=30)
        if result.returncode != (0 if succeeds else 1):
            raise AssertionError(f'exit={result.returncode:#x}\n{result.stdout}{result.stderr}')
        return json.loads((output / (name + '.sceneasset.json')).read_text()) if succeeds else None

    first = cook()
    texture = first['textures'][0]
    assert texture['cooked'] and texture['srgb']
    dds = output / texture['path']
    encoded = dds.read_bytes()
    assert encoded[:4] == b'DDS ' and struct.unpack_from('<II', encoded, 12) == (4, 4)
    assert encoded[84:88] == b'DX10' and struct.unpack_from('<I', encoded, 128)[0] == 99  # BC7 sRGB
    assert struct.unpack_from('<I', encoded, 28)[0] == 3 and len(encoded) == 148 + 3 * 16
    unchanged = [p.stat().st_mtime_ns for p in (metadata, asset, dds)]
    assert cook() == first
    assert unchanged == [p.stat().st_mtime_ns for p in (metadata, asset, dds)]
    dds.unlink()
    assert cook() == first and dds.read_bytes() == encoded

    # Texture aliases deduplicate the resource, while retaining material references.
    document['textures'].append({'source': 0})
    document['materials'][1]['pbrMetallicRoughness']['baseColorTexture'] = {'index': 1}
    aliased = cook()
    assert len(aliased['textures']) == 1
    assert {m['baseColorTexture'] for m in aliased['materials']} == {texture['id']}

    document['images'].insert(0, dict(name='Other', uri='data:image/png;base64,' + base64.b64encode(png(20)).decode()))
    for item in document['textures']:
        item['source'] = 1
    document['textures'].insert(0, {'source': 0})
    for material in document['materials']:
        material['pbrMetallicRoughness']['baseColorTexture']['index'] += 1
    reordered = cook()
    assert reordered['materials'][0]['baseColorTexture'] == texture['id']
    old_other = next(t['id'] for t in reordered['textures'] if not t['srgb'])
    document['textures'].pop(0)
    for material in document['materials']:
        material['pbrMetallicRoughness']['baseColorTexture']['index'] -= 1
    cook()
    document['textures'].append({'source': 0})
    restored = cook()
    assert next(t['id'] for t in restored['textures'] if not t['srgb']) != old_other

    # Buffer-view JPEG input (the geometry buffer remains a data URI).
    jpeg = base64.b64decode(
        '/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/'
        '2wBDAQkJCQwLDBgNDRgyIRwhMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjL/wAARCAABAAEDASIAAhEBAxEB/'
        '8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0'
        'NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx'
        '8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl'
        '8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk'
        '5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDj6KKK8Y9k/9k=')
    buffer = base64.b64decode(document['buffers'][0]['uri'].split(',')[1])
    document['bufferViews'].append(dict(buffer=0, byteOffset=len(buffer), byteLength=len(jpeg)))
    buffer += jpeg
    document['buffers'][0] = dict(byteLength=len(buffer), uri='data:application/octet-stream;base64,' + base64.b64encode(buffer).decode())
    document['images'][1] = dict(name='Embedded', mimeType='image/jpeg', bufferView=len(document['bufferViews']) - 1)
    view = cook()
    assert view['materials'][0]['baseColorTexture'] == texture['id']
    assert (output / next(t['path'] for t in view['textures'] if t['id'] == texture['id'])).read_bytes()[:4] == b'DDS '

    # Invalid input fails before replacing geometry or identity metadata.
    saved = [p.read_bytes() for p in (metadata, asset, output / 'one.bin')]
    document['bufferViews'][-1]['byteLength'] = len(buffer) + 1
    cook(succeeds=False)
    assert saved == [p.read_bytes() for p in (metadata, asset, output / 'one.bin')]
    document['bufferViews'][-1]['byteLength'] = len(jpeg)

    # External image identities are shared across glTF sources, with distinct color-space variants.
    (work / 'shared.png').write_bytes(png())
    document['images'] = [{'uri': 'shared.png'}]
    document['textures'] = [{'source': 0}, {'source': 0}]
    document['materials'][0]['pbrMetallicRoughness']['baseColorTexture'] = {'index': 0}
    document['materials'][1]['pbrMetallicRoughness'].pop('baseColorTexture')
    document['materials'][1]['normalTexture'] = {'index': 1}
    shared = cook()
    second = cook('two')
    assert shared['textures'] == second['textures'] and len(shared['textures']) == 2
    assert shared['textures'][0]['id'] != shared['textures'][1]['id']
    external = work / 'shared.png.asset.json'
    identities = json.loads(external.read_text())
    assert identities['srgb'] == shared['materials'][0]['baseColorTexture']
    assert identities['linear'] == shared['materials'][1]['normalTexture']
    linear_dds = (output / shared['textures'][1]['path']).read_bytes()
    assert struct.unpack_from('<I', linear_dds, 128)[0] == 98  # BC7 linear
    document['materials'][1]['normalTexture'] = {'index': 0}
    cook(succeeds=False)
    assert json.loads(asset.read_text()) == shared
    document['materials'][1]['normalTexture'] = {'index': 1}
    (work / 'shared.png').write_bytes(png(100))
    changed = cook()
    assert [t['id'] for t in changed['textures']] == [t['id'] for t in shared['textures']]
    assert [t['path'] for t in changed['textures']] != [t['path'] for t in shared['textures']]

    original = external.read_bytes()
    external.write_text('{"format":"Horizon.TextureIdentity","version":2,"srgb":"' + identities['srgb'] + '"}')
    saved = [p.read_bytes() for p in (metadata, asset, output / 'one.bin')]
    cook(succeeds=False)
    assert saved == [p.read_bytes() for p in (metadata, asset, output / 'one.bin')]
    external.unlink()
    cook(succeeds=False)
    assert not external.exists()
    external.write_bytes(original)
    invalid = json.loads(original)
    invalid['linear'] = 123
    external.write_text(json.dumps(invalid))
    cook(succeeds=False)
    external.write_bytes(original)

    # An interrupted rename restores persistent identities from the exact backup.
    metadata.rename(Path(str(metadata) + '.bak'))
    external.rename(Path(str(external) + '.bak'))
    assert cook() == changed
    assert external.read_bytes() == original

    # Failed publication can be retried without generating another set of IDs.
    temporary = Path(str(asset) + '.tmp')
    temporary.mkdir()
    cook(succeeds=False, force=True)
    ids_after_failure = json.loads(metadata.read_text())
    temporary.rmdir()
    assert cook(force=True) == changed
    assert json.loads(metadata.read_text()) == ids_after_failure
    print('Scene texture and recovery regressions passed:', work)


if __name__ == '__main__':
    run(Path(sys.argv[1]).resolve())
