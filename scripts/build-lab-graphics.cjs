// Original vector masters; run with Node.js and sharp available through NODE_PATH.
const fs = require('node:fs');
const path = require('node:path');
const sharp = require('sharp');
const output = path.join(__dirname, '..', 'content', 'graphics');
fs.mkdirSync(output, { recursive: true });

const text = (x, y, size, value, color = '#28332d') =>
  `<text x="${x}" y="${y}" font-size="${size}" fill="${color}" font-family="Arial, sans-serif">${value}</text>`;
const line = (x1, y1, x2, y2, color = '#33463e', width = 3) =>
  `<path d="M${x1},${y1} L${x2},${y2}" fill="none" stroke="${color}" stroke-width="${width}"/>`;
const svg = (w, h, content) => `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">${content}</svg>`;

async function save(name, w, h, content) {
  const source = svg(w, h, content);
  fs.writeFileSync(path.join(output, `${name}.svg`), source + '\n');
  await sharp(Buffer.from(source)).png().toFile(path.join(output, `${name}.png`));
}

async function main() {
  let notes = '<rect width="1600" height="900" fill="#dad8c6"/>';
  notes += text(70, 90, 46, 'B-17 / RESONANCE RESEARCH');
  notes += text(70, 138, 24, 'FIELD CONTINUITY CHECK — SHIFT 02');
  notes += line(65, 170, 1530, 170);
  for (const [index, value] of ['ISOLATE POWER', 'VERIFY COPPER CONTINUITY', 'FIT SAMPLE / CLOSE GUARD',
      'COMPARE PICKUP TO REFERENCE', 'LOG DRIFT / THEN ENERGIZE'].entries()) {
    notes += text(75, 245 + index * 77, 29, `${index + 1}. ${value}`);
  }
  notes += text(75, 680, 29, 'DO NOT BRIDGE THE INNER CONDUCTOR', '#873d2d');
  notes += text(75, 735, 26, 'CHECK: CONTACTS C1–C4 / RETURN R0');
  notes += text(75, 790, 25, 'SIGN OFF BEFORE LEAVING THE OBSERVATION BAY');
  notes += text(925, 200, 26, 'CONTINUITY / FOUR CONTACTS');
  for (const radius of [95, 119]) notes += `<circle cx="1190" cy="385" r="${radius}" fill="none" stroke="#80603c" stroke-width="4"/>`;
  for (let i = 0; i < 4; i++) {
    const a = i * Math.PI / 2;
    const x = 1190 + Math.cos(a) * 145, y = 385 + Math.sin(a) * 145;
    notes += line(1190 + Math.cos(a) * 95, 385 + Math.sin(a) * 95, x, y, '#80603c', 5);
    notes += text(x - 15, y - 15, 22, `C${i + 1}`);
  }
  notes += line(1095, 385, 1285, 385) + line(1190, 290, 1190, 480);
  notes += text(930, 600, 25, 'PICKUP / REFERENCE 50 Hz');
  notes += line(930, 620, 930, 790) + line(930, 790, 1480, 790);
  for (let y = 640; y < 790; y += 30) notes += line(930, y, 1480, y, '#b1baab', 1);
  let trace = '';
  for (let x = 0; x <= 540; x += 3) trace += `${x ? 'L' : 'M'}${935 + x},${700 - Math.sin(x / 32) * 49} `;
  notes += `<path d="${trace}" fill="none" stroke="#315f48" stroke-width="4"/>`;
  notes += text(950, 835, 21, 'DRIFT &lt; 2% / RECHECK AFTER WARM-UP');
  await save('lab_notes', 1600, 900, notes);

  const names = ['EYE PROTECTION', 'RESPIRATOR REQUIRED', 'HIGH VOLTAGE', 'MOVING MACHINERY',
    'FIELD ACTIVE', 'DO NOT CROSS', 'VERIFY CONTINUITY', 'AUTHORIZED ACCESS'];
  let icons = '<rect width="2048" height="1024" fill="#d6d3bd"/>';
  for (let i = 0; i < 8; i++) {
    const x = (i % 4) * 512, y = Math.floor(i / 4) * 512;
    icons += `<g transform="translate(${x},${y})"><rect x="18" y="18" width="476" height="476" rx="8" fill="#dedbc7" stroke="#303a32" stroke-width="6"/>`;
    const warning = [2, 3, 4].includes(i);
    icons += `<rect x="25" y="25" width="462" height="60" fill="${warning ? '#bf922b' : '#536751'}"/>`;
    icons += text(48, 67, 25, warning ? 'CAUTION / B-17' : 'LABORATORY / B-17', warning ? '#292d24' : '#eeebd6');
    if (i === 0) icons += '<path d="M95 225 Q160 180 240 225 L270 225 Q350 180 417 225 L400 295 Q335 335 275 280 L237 280 Q168 335 112 295 Z" fill="none" stroke="#29382f" stroke-width="18"/><path d="M120 210 L100 155 M395 210 L415 155" stroke="#29382f" stroke-width="16"/>';
    if (i === 1) icons += '<path d="M185 185 Q256 150 327 185 L340 295 L256 345 L172 295 Z" fill="none" stroke="#29382f" stroke-width="16"/><circle cx="256" cy="275" r="44" fill="none" stroke="#29382f" stroke-width="16"/>';
    if (i === 2) icons += '<path d="M256 120 L430 350 L82 350 Z" fill="#bf922b" stroke="#29382f" stroke-width="12"/><path d="M270 175 L220 265 L270 265 L230 330 L305 235 L255 235 Z" fill="#29382f"/>';
    if (i === 3) {
      icons += '<circle cx="256" cy="250" r="75" fill="none" stroke="#29382f" stroke-width="24"/><circle cx="256" cy="250" r="23" fill="#29382f"/>';
      for (let j = 0; j < 8; j++) icons += `<path d="M${256 + Math.cos(j * Math.PI / 4) * 70},${250 + Math.sin(j * Math.PI / 4) * 70} L${256 + Math.cos(j * Math.PI / 4) * 110},${250 + Math.sin(j * Math.PI / 4) * 110}" stroke="#29382f" stroke-width="25"/>`;
    }
    if (i === 4 || i === 6) icons += '<circle cx="256" cy="250" r="105" fill="none" stroke="#80603c" stroke-width="11"/><circle cx="256" cy="250" r="65" fill="none" stroke="#80603c" stroke-width="8"/><path d="M110 250 H402 M256 105 V395" stroke="#29382f" stroke-width="9"/>';
    if (i === 5) icons += '<circle cx="256" cy="250" r="112" fill="none" stroke="#963f30" stroke-width="22"/><path d="M180 165 L332 335" stroke="#963f30" stroke-width="23"/><path d="M222 185 V275 L195 330 M222 275 L265 330 M222 225 L280 255" stroke="#29382f" stroke-width="16"/><circle cx="222" cy="157" r="20" fill="#29382f"/>';
    if (i === 7) icons += '<rect x="180" y="210" width="152" height="140" rx="8" fill="#29382f"/><path d="M205 210 V175 Q256 105 307 175 V210" fill="none" stroke="#29382f" stroke-width="18"/><circle cx="256" cy="265" r="16" fill="#dedbc7"/><path d="M256 275 V310" stroke="#dedbc7" stroke-width="12"/>';
    icons += text(40, 445, 27, names[i]);
    icons += '</g>';
  }
  await save('lab_pictograms', 2048, 1024, icons);

  let display = '<rect width="1024" height="640" fill="#10251d"/>';
  display += text(35, 55, 33, 'RELAY / SPECTRAL ANALYSIS', '#8fae7a');
  display += text(35, 98, 22, 'B-17    PICKUP C2    RANGE 0.5 V    50 Hz', '#719b69');
  for (let x = 40; x <= 980; x += 47) display += line(x, 140, x, 550, '#294235', 1);
  for (let y = 140; y <= 550; y += 41) display += line(40, y, 980, y, '#294235', 1);
  for (let channel = 0; channel < 2; channel++) {
    let trace = '';
    for (let x = 0; x < 940; x += 2) {
      const signal = Math.sin(x / 39) * 54 + (channel ? Math.sin(x / 13) * 9 : 0);
      trace += `${x ? 'L' : 'M'}${40 + x},${245 + channel * 200 - signal} `;
    }
    display += `<path d="${trace}" fill="none" stroke="${channel ? '#b59a54' : '#98ba7b'}" stroke-width="4"/>`;
    display += text(50, 167 + channel * 200, 20, channel ? 'REFERENCE / RETURN R0' : 'CHANNEL 1 / FIELD PICKUP', '#8fae7a');
  }
  display += text(35, 605, 24, 'CONTINUITY OK     DRIFT 0.8%     LOG / SHIFT 02', '#8fae7a');
  await save('instrument_display', 1024, 640, display);
}
main().catch(error => { console.error(error); process.exitCode = 1; });
