const fs = require('node:fs');
const path = require('node:path');
const sharp = require('sharp');
const root = path.join(__dirname, '..');
const source = path.join(root, 'content/graphics/branding');
const output = path.join(root, 'assets/branding');
const brass = '#C4B477', green = '#111814';
const sizes = [16, 24, 32, 48, 64, 128, 256];
const read = name => fs.readFileSync(path.join(source, name), 'utf8');
const body = svg => svg.replace(/^.*?<svg[^>]*>/s, '').replace(/<\/svg>\s*$/, '');

// SDL's BMP reader preserves the explicit alpha mask in a BITMAPV4HEADER.
function bmp(rgba, width, height) {
  const offset = 14 + 108, result = Buffer.alloc(offset + width * height * 4);
  result.write('BM'); result.writeUInt32LE(result.length, 2); result.writeUInt32LE(offset, 10);
  result.writeUInt32LE(108, 14); result.writeInt32LE(width, 18); result.writeInt32LE(height, 22);
  result.writeUInt16LE(1, 26); result.writeUInt16LE(32, 28); result.writeUInt32LE(3, 30);
  result.writeUInt32LE(width * height * 4, 34);
  for (const [i, mask] of [0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000].entries())
    result.writeUInt32LE(mask, 54 + i * 4);
  result.writeUInt32LE(0x73524742, 70);
  for (let y = 0; y < height; ++y) for (let x = 0; x < width; ++x) {
    const src = (y * width + x) * 4, dst = offset + ((height - 1 - y) * width + x) * 4;
    result[dst] = rgba[src + 2]; result[dst + 1] = rgba[src + 1];
    result[dst + 2] = rgba[src]; result[dst + 3] = rgba[src + 3];
  }
  return result;
}

(async () => {
  fs.mkdirSync(output, {recursive: true});
  const master = read('null-meridian.svg');
  await sharp(Buffer.from(master), {density: 288}).resize(512, 512).png().toFile(path.join(output, 'relaycore.png'));
  const menu = await sharp(Buffer.from(master.replaceAll(brass, '#B4A06A')), {density: 144})
    .ensureAlpha().raw().toBuffer({resolveWithObject: true});
  fs.writeFileSync(path.join(output, 'relaycore-menu.bmp'), bmp(menu.data, menu.info.width, menu.info.height));
  await sharp(Buffer.from(read('relaycore-wordmark.svg')), {density: 144}).png()
    .toFile(path.join(output, 'relaycore-wordmark.png'));
  const frames = [];
  for (const size of sizes) {
    let drawing;
    if (size <= 24) {
      const a = .5, b = size - .5, k = size === 16 ? 3 : 4;
      const plate = `<path d="M${k} ${a} H${size-k} L${b} ${k} V${size-k} L${size-k} ${b} H${k} L${a} ${size-k} V${k} Z" fill="${green}" stroke="${brass}" stroke-width=".5"/>`;
      drawing = `<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}" viewBox="0 0 ${size} ${size}">${plate}${body(read(`null-meridian-${size}.svg`))}</svg>`;
    } else {
      drawing = `<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128"><path d="M22 6 H106 L122 22 V106 L106 122 H22 L6 106 V22 Z" fill="${green}" stroke="${brass}" stroke-width="2"/><g transform="translate(8 8) scale(.875)">${body(master)}</g></svg>`;
    }
    frames.push(await sharp(Buffer.from(drawing), {density: 72}).resize(size, size).png().toBuffer());
  }
  const header = Buffer.alloc(6 + sizes.length * 16);
  header.writeUInt16LE(1, 2); header.writeUInt16LE(sizes.length, 4);
  let offset = header.length;
  sizes.forEach((size, i) => {
    const entry = 6 + i * 16;
    header[entry] = header[entry + 1] = size % 256;
    header.writeUInt16LE(1, entry + 4); header.writeUInt16LE(32, entry + 6);
    header.writeUInt32LE(frames[i].length, entry + 8); header.writeUInt32LE(offset, entry + 12);
    offset += frames[i].length;
  });
  fs.writeFileSync(path.join(output, 'relaycore.ico'), Buffer.concat([header, ...frames]));
  console.log('Built RelayCore branding: menu BMP, PNG previews and seven-frame ICO.');
})().catch(error => { console.error(error); process.exit(1); });
