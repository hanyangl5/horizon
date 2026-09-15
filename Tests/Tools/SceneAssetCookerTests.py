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
    header = [124, 0x100f, 1, 1, 4, 0, 0] + [0] * 11 + [32, 0x41, 0, 32, 0xff, 0xff00, 0xff0000, 0xff000000] + [0x1000, 0, 0, 0, 0]
    (work / 'albedo.dds').write_bytes(b'DDS ' + struct.pack('<31I', *header) + bytes([255, 255, 255, 255]))

    def cook(force=False, succeeds=True):
        source.write_text(json.dumps(document), encoding='utf-8')
        command = [str(executable), '-pgltf', '--input-file', source.as_posix(), '--output', output.as_posix()]
        if force:
            command.append('--force')
        result = subprocess.run(command, cwd=work, capture_output=True, text=True, errors='replace', timeout=30)
        if (result.returncode == 0) != succeeds:
            raise AssertionError(f'exit={result.returncode:#x}\n{result.stdout}{result.stderr}')
        return json.loads(asset.read_text()) if succeeds else None

    first = cook()
    assert first['format'] == 'Horizon.SceneAsset' and first['version'] == 1
    assert uuid.UUID(first['id']).version == 4
    assert first['materials'][0]['baseColorTexture'] == first['textures'][0]['id']
    assert first['materials'][1]['baseColorTexture'] is None
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


if __name__ == '__main__':
    run(Path(sys.argv[1]).resolve())
