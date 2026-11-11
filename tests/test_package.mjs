import assert from 'node:assert/strict';
import { mkdtemp, mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { packageRes } from '../scripts/package-res.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const temp = await mkdtemp(path.join(os.tmpdir(), 'bip-res-package-test-'));
try {
  const { ResAsset, ResFile } = await import('../vendor/res-tools/src/res-file.ts');
  const baselinePath = path.join(temp, 'synthetic-baseline.res');
  const noRelocPath = path.join(temp, 'synthetic-no-reloc.res');
  const duplicateRelocPath = path.join(temp, 'synthetic-duplicate-reloc.res');
  const appPath = path.join(temp, 'waypoint.elf');
  const relocPath = path.join(temp, 'fake-libbip.bin');
  const outputPath = path.join(temp, 'new-resource.res');

  const header = new Uint8Array(0x20);
  header.set(Buffer.from('NERES'));
  for (let i = 5; i < header.length; i += 1) header[i] = (i * 13) & 0xff;
  const emptyHeader = Buffer.alloc(0x24);
  emptyHeader.write('NERES', 0, 'ascii');
  const fixture = new ResFile(new Uint8Array(emptyHeader));
  fixture.header = header;
  fixture.assets = Array.from({ length: 930 }, (_, i) => new ResAsset(Uint8Array.from([i & 0xff, (i >> 8) & 0xff])));
  const fakeReloc = Buffer.from('LIBBIP synthetic test relocation', 'ascii');
  fixture.assets[17] = new ResAsset(new Uint8Array(fakeReloc));
  const baselineBytes = Buffer.from(fixture.pack());
  await writeFile(baselinePath, baselineBytes, { flag: 'wx' });

  const noRelocFixture = new ResFile(new Uint8Array(baselineBytes));
  noRelocFixture.assets = noRelocFixture.assets.filter((_, i) => i !== 17);
  while (noRelocFixture.assets.length < 930) noRelocFixture.assets.push(new ResAsset(Uint8Array.from([0xaa])));
  await writeFile(noRelocPath, Buffer.from(noRelocFixture.pack()), { flag: 'wx' });
  const duplicateFixture = new ResFile(new Uint8Array(baselineBytes));
  duplicateFixture.assets.push(new ResAsset(new Uint8Array(fakeReloc)));
  await writeFile(duplicateRelocPath, Buffer.from(duplicateFixture.pack()), { flag: 'wx' });
  await writeFile(relocPath, fakeReloc, { flag: 'wx' });

  // Synthetic ARM ELF header for packer tests only. There is no executable
  // payload and the synthetic RES is not flashable firmware.
  const fakeElf = Buffer.alloc(52);
  fakeElf.set([0x7f, 0x45, 0x4c, 0x46, 1, 1, 1]);
  fakeElf.writeUInt16LE(2, 16); // ET_EXEC
  fakeElf.writeUInt16LE(40, 18); // EM_ARM
  fakeElf.writeUInt32LE(1, 20); // EV_CURRENT
  await writeFile(appPath, fakeElf, { flag: 'wx' });

  const result = await packageRes({ base: baselinePath, out: outputPath, apps: [appPath] });
  assert.equal(result.baselineResources, 930);
  assert.equal(result.addedReloc, false);
  assert.equal(result.addedApps, 1);
  assert.equal(result.outputBytes, (await readFile(outputPath)).length);

  const outputBytes = await readFile(outputPath);
  const output = new ResFile(new Uint8Array(outputBytes));
  const baseline = new ResFile(new Uint8Array(baselineBytes));
  assert.equal(output.assets.length, 931);
  assert.deepEqual(Buffer.from(output.header), Buffer.from(baseline.header));
  for (let i = 0; i < baseline.assets.length; i += 1) {
    assert.deepEqual(Buffer.from(output.assets[i].data), Buffer.from(baseline.assets[i].data), `baseline resource ${i}`);
  }
  assert.equal(output.assets[930].data[0], 0x7f);
  assert.equal(output.assets[930].data[4], 1);
  assert.equal(Buffer.from(output.assets[930].data).readUInt16LE(18), 40);
  assert.deepEqual(Buffer.from(output.assets[930].data), fakeElf);
  assert.deepEqual(Buffer.from(output.assets[17].data), fakeReloc);

  await assert.rejects(packageRes({ base: noRelocPath, out: path.join(temp, 'missing-reloc.res'), apps: [appPath] }), /must contain exactly one LIBBIP/);
  await assert.rejects(packageRes({ base: duplicateRelocPath, out: path.join(temp, 'duplicate-reloc.res'), apps: [appPath] }), /contains 2 LIBBIP/);
  await assert.rejects(packageRes({ base: baselinePath, out: path.join(temp, 'conflict-reloc.res'), apps: [appPath], reloc: relocPath }), /conflicts with a LIBBIP/);

  const explicitOutput = path.join(temp, 'explicit-reloc-output.res');
  const explicitResult = await packageRes({ base: noRelocPath, out: explicitOutput, apps: [appPath], reloc: relocPath });
  assert.equal(explicitResult.addedReloc, true);
  const explicitPacked = new ResFile(new Uint8Array(await readFile(explicitOutput)));
  assert.equal(explicitPacked.assets.length, 932);
  assert.deepEqual(Buffer.from(explicitPacked.header), Buffer.from(noRelocFixture.header));
  for (let i = 0; i < noRelocFixture.assets.length; i += 1) {
    assert.deepEqual(Buffer.from(explicitPacked.assets[i].data), Buffer.from(noRelocFixture.assets[i].data), `explicit baseline resource ${i}`);
  }
  assert.deepEqual(Buffer.from(explicitPacked.assets[930].data), fakeReloc);
  assert.deepEqual(Buffer.from(explicitPacked.assets[931].data), fakeElf);

  const badRelocPath = path.join(temp, 'bad-reloc.bin');
  await writeFile(badRelocPath, Buffer.from('not LIBBIP'));
  await assert.rejects(packageRes({ base: noRelocPath, out: path.join(temp, 'bad-reloc.res'), apps: [appPath], reloc: badRelocPath }), /must begin with the LIBBIP/);
  const hugeRelocPath = path.join(temp, 'huge-reloc.bin');
  await writeFile(hugeRelocPath, Buffer.concat([Buffer.from('LIBBIP'), Buffer.alloc(64 * 1024)]));
  await assert.rejects(packageRes({ base: noRelocPath, out: path.join(temp, 'huge-reloc.res'), apps: [appPath], reloc: hugeRelocPath }), /exceeds 65536 bytes/);

  await assert.rejects(packageRes({ base: baselinePath, out: baselinePath, apps: [appPath] }), /must not be the baseline/);
  const sentinelPath = path.join(temp, 'already-exists.res');
  await writeFile(sentinelPath, 'keep me');
  await assert.rejects(packageRes({ base: baselinePath, out: sentinelPath, apps: [appPath] }), /already exists/);
  assert.equal(await readFile(sentinelPath, 'utf8'), 'keep me');

  const smallPath = path.join(temp, 'too-small.res');
  const small = new ResFile(new Uint8Array(emptyHeader));
  small.header.set(Buffer.from('NERES'));
  small.assets = Array.from({ length: 929 }, (_, i) => new ResAsset(Uint8Array.from([i & 0xff])));
  await writeFile(smallPath, Buffer.from(small.pack()));
  await assert.rejects(packageRes({ base: smallPath, out: path.join(temp, 'small-output.res'), apps: [appPath] }), /requires at least 930/);

  const malformedPath = path.join(temp, 'malformed.res');
  const malformed = Buffer.from(baselineBytes);
  malformed.writeUInt32LE(0xfffffff0, 0x24 + 20 * 4);
  await writeFile(malformedPath, malformed);
  await assert.rejects(packageRes({ base: malformedPath, out: path.join(temp, 'malformed-output.res'), apps: [appPath] }), /outside file bounds/);

  const badElfPath = path.join(temp, 'not-elf.bin');
  await writeFile(badElfPath, Buffer.from('not an elf'));
  await assert.rejects(packageRes({ base: baselinePath, out: path.join(temp, 'bad-app.res'), apps: [badElfPath] }), /32-bit little-endian ARM ET_EXEC ELF/);

  const cliOutput = path.join(temp, 'cli-output.res');
  const cli = spawnSync(process.execPath, ['--experimental-strip-types', path.join(root, 'scripts/package-res.mjs'), '--base', noRelocPath, '--out', cliOutput, '--app', appPath, '--reloc', relocPath], { cwd: root, encoding: 'utf8' });
  assert.equal(cli.status, 0, `status=${cli.status} error=${cli.error}\nstdout=${cli.stdout}\nstderr=${cli.stderr}`);
  assert.equal((await readFile(cliOutput)).length, explicitResult.outputBytes);
  const cliPacked = new ResFile(new Uint8Array(await readFile(cliOutput)));
  assert.equal(cliPacked.assets.length, 932);
  assert.deepEqual(Buffer.from(cliPacked.assets[930].data), fakeReloc);

  console.log('PASS: synthetic RES parse/append/round-trip, LIBBIP selection, preservation, overwrite protection, and CLI');
  console.log('NOTE: fixtures are synthetic and explicitly non-flashable; no firmware compatibility was tested.');
} finally {
  await rm(temp, { recursive: true, force: true });
}
