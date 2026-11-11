#!/usr/bin/env node
import { readFile, writeFile, realpath } from 'node:fs/promises';
import { registerHooks } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

// libbip searches for ELF applications starting at RES resource index 930.
export const FIRST_APP_RESOURCE = 930;
export const MAX_RELOC_BYTES = 64 * 1024;
const LIBBIP_MAGIC = Buffer.from('LIBBIP', 'ascii');

// The pinned upstream TypeScript sources use extensionless relative imports.
// Resolve those imports for Node's native TS type stripping without modifying
// the vendored implementation.
registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier.startsWith('.') && !/\.[a-zA-Z0-9]+$/.test(specifier)) {
      try { return nextResolve(`${specifier}.ts`, context); } catch {}
    }
    return nextResolve(specifier, context);
  },
});
const { ResAsset, ResFile } = await import('../vendor/res-tools/src/res-file.ts');

function u32le(buffer, offset) {
  return buffer.readUInt32LE(offset);
}

function validateResBuffer(buffer, label) {
  if (buffer.length < 0x24) throw new Error(`${label}: RES is shorter than its 36-byte header`);
  if (buffer.toString('ascii', 0, 5) !== 'NERES') throw new Error(`${label}: missing NERES signature`);
  const count = u32le(buffer, 0x20);
  if (count === 0) throw new Error(`${label}: resource count is zero`);
  const tableEnd = 0x24 + count * 4;
  if (!Number.isSafeInteger(tableEnd) || tableEnd > buffer.length) {
    throw new Error(`${label}: resource address table exceeds file length`);
  }
  const dataOffset = tableEnd;
  let previous = 0;
  for (let i = 0; i < count; i += 1) {
    const relative = u32le(buffer, 0x24 + i * 4);
    if (i === 0 && relative !== 0) throw new Error(`${label}: first resource offset must be zero`);
    if (relative < previous) throw new Error(`${label}: resource offsets are not monotonic at index ${i}`);
    const start = dataOffset + relative;
    const end = i + 1 === count ? buffer.length : dataOffset + u32le(buffer, 0x24 + (i + 1) * 4);
    if (start < dataOffset || end < start || end > buffer.length) {
      throw new Error(`${label}: resource ${i} range is outside file bounds`);
    }
    previous = relative;
  }
  return count;
}

function parseArgs(argv) {
  const parsed = { apps: [] };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === '--base' || arg === '--out' || arg === '--app' || arg === '--reloc') {
      const value = argv[++i];
      if (!value || value.startsWith('--')) throw new Error(`${arg} requires a path`);
      if (arg === '--app') parsed.apps.push(value);
      else {
        const key = arg === '--base' ? 'base' : arg === '--out' ? 'out' : 'reloc';
        if (parsed[key]) throw new Error(`${arg} may be specified only once`);
        parsed[key] = value;
      }
    } else if (arg === '--help' || arg === '-h') {
      parsed.help = true;
    } else {
      throw new Error(`unknown argument: ${arg}`);
    }
  }
  if (!parsed.help && (!parsed.base || !parsed.out || parsed.apps.length === 0)) {
    throw new Error('usage: package-res.mjs --base matching.res --out NEW.res --app app.elf [--app other.elf] [--reloc libbip.bin]');
  }
  return parsed;
}

function isArmElf(buffer) {
  return buffer.length >= 52 &&
    buffer[0] === 0x7f && buffer.toString('ascii', 1, 4) === 'ELF' &&
    buffer[4] === 1 && // ELFCLASS32
    buffer[5] === 1 && // ELFDATA2LSB
    buffer[6] === 1 && // EV_CURRENT in e_ident
    buffer.readUInt16LE(16) === 2 && // ET_EXEC
    buffer.readUInt16LE(18) === 40 && // EM_ARM
    buffer.readUInt32LE(20) === 1; // EV_CURRENT
}

async function ensureDistinctNewOutput(basePath, outputPath) {
  const baseReal = await realpath(basePath);
  const outResolved = path.resolve(outputPath);
  if (outResolved === path.resolve(basePath) || outResolved === baseReal) {
    throw new Error('output path must not be the baseline RES path');
  }
  try {
    const outReal = await realpath(outResolved);
    if (outReal === baseReal) throw new Error('output path resolves to the baseline RES file');
    throw new Error('output path already exists; choose a new path to preserve every baseline');
  } catch (error) {
    if (error.code !== 'ENOENT') throw error;
  }
  if (path.dirname(outResolved) === outResolved) throw new Error('invalid output path');
}

export async function packageRes({ base, out, apps, reloc }) {
  const basePath = path.resolve(base);
  const outputPath = path.resolve(out);
  if (basePath === outputPath) throw new Error('output path must not be the baseline RES path');
  const baseBytes = await readFile(basePath);
  const baseCount = validateResBuffer(baseBytes, 'baseline');
  if (baseCount < FIRST_APP_RESOURCE) {
    throw new Error(`baseline has ${baseCount} resources; BipOS app scanning requires at least ${FIRST_APP_RESOURCE}`);
  }

  await ensureDistinctNewOutput(basePath, outputPath);
  const parsedBaseline = new ResFile(new Uint8Array(baseBytes));
  if (parsedBaseline.assets.length !== baseCount) throw new Error('upstream parser resource count mismatch');
  const matchingRelocs = parsedBaseline.assets.filter((asset) =>
    Buffer.from(asset.data).subarray(0, LIBBIP_MAGIC.length).equals(LIBBIP_MAGIC));
  if (matchingRelocs.length > 1) throw new Error(`baseline contains ${matchingRelocs.length} LIBBIP relocation assets; exactly one is allowed`);

  let relocBytes = null;
  if (reloc) {
    if (matchingRelocs.length !== 0) throw new Error('explicit --reloc conflicts with a LIBBIP asset already in the baseline');
    relocBytes = await readFile(path.resolve(reloc));
    if (relocBytes.length > MAX_RELOC_BYTES) throw new Error(`--reloc exceeds ${MAX_RELOC_BYTES} bytes`);
    if (relocBytes.length < LIBBIP_MAGIC.length || !relocBytes.subarray(0, LIBBIP_MAGIC.length).equals(LIBBIP_MAGIC)) {
      throw new Error('--reloc must begin with the LIBBIP signature');
    }
  } else if (matchingRelocs.length !== 1) {
    throw new Error('baseline must contain exactly one LIBBIP relocation asset or pass --reloc');
  }

  const appBytes = [];
  for (const appPath of apps) {
    const bytes = await readFile(path.resolve(appPath));
    if (!isArmElf(bytes)) {
      throw new Error(`${appPath}: expected a 32-bit little-endian ARM ET_EXEC ELF`);
    }
    appBytes.push(bytes);
  }
  if (relocBytes) parsedBaseline.assets.push(new ResAsset(new Uint8Array(relocBytes)));
  for (const bytes of appBytes) parsedBaseline.assets.push(new ResAsset(new Uint8Array(bytes)));

  const packed = Buffer.from(parsedBaseline.pack());
  const outputCount = validateResBuffer(packed, 'packed output');
  if (outputCount !== baseCount + appBytes.length + (relocBytes ? 1 : 0)) throw new Error('packed resource count mismatch');

  // Verify baseline header and every existing resource are byte-identical.
  if (!packed.subarray(0, 0x20).equals(baseBytes.subarray(0, 0x20))) {
    throw new Error('packed output changed the baseline RES header');
  }
  const reparsed = new ResFile(new Uint8Array(packed));
  const original = new ResFile(new Uint8Array(baseBytes));
  for (let i = 0; i < baseCount; i += 1) {
    if (!Buffer.from(reparsed.assets[i].data).equals(Buffer.from(original.assets[i].data))) {
      throw new Error(`packed output changed baseline resource ${i}`);
    }
  }
  if (relocBytes && !Buffer.from(reparsed.assets[baseCount].data).equals(relocBytes)) {
    throw new Error(`packed output relocation resource ${baseCount} failed byte verification`);
  }
  const appStart = baseCount + (relocBytes ? 1 : 0);
  for (let i = 0; i < appBytes.length; i += 1) {
    const asset = reparsed.assets[appStart + i];
    if (!isArmElf(Buffer.from(asset.data)) || !Buffer.from(asset.data).equals(appBytes[i])) {
      throw new Error(`packed output app resource ${appStart + i} failed byte verification`);
    }
  }

  // wx is exclusive: a race or existing path can never replace the baseline
  // or another output artifact.
  await writeFile(outputPath, packed, { flag: 'wx', mode: 0o600 });
  return { outputPath, baselineResources: baseCount, addedReloc: Boolean(relocBytes), addedApps: appBytes.length, outputBytes: packed.length };
}

if (fileURLToPath(import.meta.url) === path.resolve(process.argv[1] ?? '')) {
  try {
    const args = parseArgs(process.argv.slice(2));
    if (args.help) {
      console.log('Usage: node scripts/package-res.mjs --base matching.res --out NEW.res --app app.elf [--app other.elf] [--reloc libbip.bin]');
    } else {
      const result = await packageRes(args);
      console.log(`Wrote ${result.outputPath}: preserved ${result.baselineResources} resources; added relocation=${result.addedReloc}; added ${result.addedApps} ELF app(s); ${result.outputBytes} bytes.`);
      console.log('Offline packaging only. This output is not firmware-validated and must not be flashed without matching-bundle review.');
    }
  } catch (error) {
    console.error(`package-res: ${error.message}`);
    process.exitCode = 1;
  }
}
