import re
import json
import sys
import os

# Per-executable config: (segment -> IDA base address)
# E2WIN95P: PE-loaded, image base 0x400000; seg 0001 code +0x10000, seg 0002 data +0x70000
# E2DOS: LE, relocation base from Object Table (obj1=0x10000, obj2=0x70000)
EXE_CONFIGS = {
    'E2WIN95P.EXE': {
        'segment_base': {'0001': 0x410000, '0002': 0x470000},
        'module_index_limit': 18,
    },
    'E2WIN95.EXE': {
        'segment_base': {'0001': 0x410000, '0002': 0x470000},
        'module_index_limit': None,
    },
    'E2DOS.EXE': {
        'segment_base': {'0001': 0x10000, '0002': 0x70000},
        'module_index_limit': None,
    },
    'E1WIN95.EXE': {
        'segment_base': {'0001': 0x410000, '0002': 0x460000},
        'module_index_limit': None,
    },
}


def parse_wdump(input_path, config):
    with open(input_path, 'r') as f:
        data = f.read()

    lines = data.split('\n')
    entries = []
    modules = []
    entry = {}
    module_entry = {}

    global_info = False
    module_info = False
    for line in lines:
        if 'Module Info (section 0)' in line:
            module_info = True
            continue

        if module_info:
            if ')' in line:
                index = line.split(')')[0].strip()
                module_entry['index'] = index
            if 'Name:' in line:
                name = line.split('Name:')[1].strip()
                module_entry['name'] = name
                modules.append({**module_entry})

        if 'Global Info (section 0)' in line:
            global_info = True
            module_info = False
            continue

        if 'Addr Info (section 0)' in line:
            global_info = False
            break

        if global_info:
            if 'Name:' in line:
                name = line.split('Name:')[1].strip()
                entry['name'] = name
            if 'address' in line:
                address = line.split('address')[1].split('=')[1].strip()
                entry['full_address'] = address
                entry['segment'] = address.split(':')[0]
                entry['address'] = int(address.split(':')[1], 16)
            if 'module index' in line:
                module_index = line.split('module index')[1].split('=')[1].strip()
                entry['module_index'] = module_index
                entry['module'] = modules[int(module_index)]['name']
            if 'kind' in line:
                kind = line.split('kind:')[1].strip()
                entry['kind'] = kind
                entries.append({**entry})

    seg_base = config['segment_base']
    for e in entries:
        e['ida_prefix'] = e['module'].split('\\')[-1].split('.')[0]
        base = seg_base.get(e['segment'], 0)
        e['ida_address'] = e['address'] + base
        e['ida_address_hex'] = hex(e['ida_address']).replace('0x', '').upper()
        e['ida_name'] = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', e['name'].replace('_', ''))
        e['ida_name'] = re.sub('([a-z0-9])([A-Z])', r'\1_\2', e['ida_name']).lower()
        if e['segment'] == '0001':
            e['ida_name'] = e['ida_prefix'] + '_' + e['ida_name']
        e['ida_name'] = e['ida_name'] + '_' + e['ida_address_hex']
        e['ida_name'] = sanitize_name(e['ida_name'])

    return entries, modules


def sanitize_name(name):
    # replace any char not valid in IDA name with '_'
    n = re.sub(r'[^A-Za-z0-9_]', '_', name)
    # collapse runs of _
    n = re.sub(r'_+', '_', n)
    # leading digit -> prefix underscore
    if n and n[0].isdigit():
        n = '_' + n
    n = n.replace('sub_', 'sub')
    return n


def write_outputs(exe_name, entries, modules, config):
    base = exe_name
    with open(base + '.json', 'w') as f:
        json.dump(entries, f, indent=4)
    with open(base + '.modules.json', 'w') as f:
        json.dump(modules, f, indent=4)

    limit = config['module_index_limit']
    with open(base + '.idc', 'w') as f:
        for e in entries:
            if limit is not None and int(e['module_index']) > limit:
                continue
            # skip Watcom TRANSFER thunks (module named "TRANSFER CODE")
            if 'TRANSFER' in e['module']:
                continue
            # only undefine on data segment (0002) to fix tail-byte rename;
            # never on code (0001) — would destroy function definitions
            if e['segment'] == '0002':
                f.write('MakeUnknown(0x{ida_address_hex}, 1, 0);\n'.format(**e))
            f.write('MakeName(0x{ida_address_hex}, "{ida_name}");\n'.format(**e))


def main():
    targets = sys.argv[1:] if len(sys.argv) > 1 else list(EXE_CONFIGS.keys())
    for exe in targets:
        if exe not in EXE_CONFIGS:
            print(f'skip {exe}: no config')
            continue
        txt = exe + '.txt'
        if not os.path.exists(txt):
            print(f'skip {exe}: {txt} missing')
            continue
        cfg = EXE_CONFIGS[exe]
        entries, modules = parse_wdump(txt, cfg)
        write_outputs(exe, entries, modules, cfg)
        print(f'{exe}: {len(entries)} symbols, {len(modules)} modules -> {exe}.idc')


if __name__ == '__main__':
    main()
