// Local facility graphics: opaque SVG masters and PNG albedo, no baked lighting.
const fs = require('node:fs');
const path = require('node:path');
const sharp = require('sharp');
const root = path.join(__dirname, '..');
const output = path.join(root, 'content/graphics/sector_signage');
const ink = '#28332d', paper = '#dad8c6', green = '#536751', yellow = '#bf922b';
const escape = value => value.replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;');
const text = (x, y, size, value, color = ink) => `<text x="${x}" y="${y}" font-size="${size}" fill="${color}" font-family="Arial, sans-serif">${escape(value)}</text>`;
const line = (x, y, xx, yy, color = ink, width = 3) => `<path d="M${x} ${y} L${xx} ${yy}" stroke="${color}" stroke-width="${width}" fill="none"/>`;
const sectors = [
  { id: 'pressure', title: 'PRESSURE TRANSFER', room: 'EQUIPMENT BAY / SERVICE HALL / ISOLATION GALLERY', unit: 'PT-03 / DIFFERENTIAL BYPASS',
    steps: ['01 / OPEN SERVICE HALL', '02 / OPEN BYPASS HALL', '03 / START PRESSURE TRANSFER', 'EXIT / RELEASE TECHNICAL GATE'],
    warning: 'PRESSURIZED LINE / KEEP GUARDS CLOSED', route: 'BAY > HALL > GALLERY > BYPASS', hazard: 'PRESSURE',
    symbols: ['pressure', 'eye', 'gear', 'lock'], labels: ['PRESSURIZED LINE', 'EYE PROTECTION', 'MOVING MACHINERY', 'SERVICE ACCESS'] },
  { id: 'cooling', title: 'COOLING LOOP', room: 'SERVICE AISLE / HEAT EXCHANGER BANK', unit: 'CL-02 / COOLANT SUPPLY + RETURN',
    steps: ['01 / ISOLATE THE LEAK', '02 / START COOLING LOOP', '03 / RELEASE SECTOR EXIT'],
    warning: 'HOT COOLANT / ISOLATE BEFORE RESTART', route: '01 ISOLATE > 02 START > 03 EXIT', hazard: 'HOT COOLANT',
    symbols: ['heat', 'eye', 'gear', 'lock'], labels: ['HOT COOLANT', 'EYE PROTECTION', 'MOVING MACHINERY', 'SERVICE ACCESS'] },
  { id: 'power', title: 'POWER RESTORATION', room: 'INTAKE / SWITCHGEAR / PUMP / FILTRATION', unit: 'F-01 FUSE / P-02 PUMP / F-04 FILTER',
    steps: ['01 / COLLECT SPARE FUSE F-01', '02 / ISOLATE DAMAGED LINE', '03 / CONNECT RESERVE BUS', '04 / RESTORE SECTOR POWER', '05 / PURGE FILTER F-04', '06 / START COOLING / RELEASE AIRLOCK'],
    warning: 'HIGH VOLTAGE / ISOLATE DAMAGED LINE FIRST', route: 'INTAKE > SWITCHGEAR > PUMP > FILTER', hazard: 'HIGH VOLTAGE',
    symbols: ['voltage', 'eye', 'gear', 'lock'], labels: ['HIGH VOLTAGE', 'EYE PROTECTION', 'MOVING MACHINERY', 'SERVICE ACCESS'] },
];
async function save(name, w, h, body) {
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">${body}</svg>\n`;
  fs.writeFileSync(path.join(output, name + '.svg'), svg);
  await sharp(Buffer.from(svg)).flatten({background: paper}).removeAlpha().png().toFile(path.join(output, name + '.png'));
}
function symbol(kind) {
  if (kind === 'eye') return '<path d="M95 225 Q160 180 240 225 L270 225 Q350 180 417 225 L400 295 Q335 335 275 280 L237 280 Q168 335 112 295 Z" fill="none" stroke="#29382f" stroke-width="18"/><path d="M120 210 L100 155 M395 210 L415 155" stroke="#29382f" stroke-width="16"/>';
  if (kind === 'lock') return '<rect x="180" y="210" width="152" height="140" rx="8" fill="#29382f"/><path d="M205 210 V175 Q256 105 307 175 V210" fill="none" stroke="#29382f" stroke-width="18"/><circle cx="256" cy="265" r="16" fill="#dedbc7"/><path d="M256 275 V310" stroke="#dedbc7" stroke-width="12"/>';
  if (kind === 'gear') return '<circle cx="256" cy="250" r="75" fill="none" stroke="#29382f" stroke-width="24"/><circle cx="256" cy="250" r="23" fill="#29382f"/>' + Array.from({length:8}, (_, j) => line(256+Math.cos(j*Math.PI/4)*70, 250+Math.sin(j*Math.PI/4)*70,256+Math.cos(j*Math.PI/4)*110,250+Math.sin(j*Math.PI/4)*110,ink,25)).join('');
  const triangle = `<path d="M256 120 L430 350 L82 350 Z" fill="${yellow}" stroke="${ink}" stroke-width="12"/>`;
  if (kind === 'voltage') return triangle + '<path d="M270 175 L220 265 L270 265 L230 330 L305 235 L255 235 Z" fill="#29382f"/>';
  if (kind === 'heat') return triangle + '<path d="M205 310 Q185 287 205 265 T205 220 M256 310 Q236 287 256 265 T256 220 M307 310 Q287 287 307 265 T307 220" fill="none" stroke="#29382f" stroke-width="13"/>';
  return triangle + '<circle cx="256" cy="270" r="49" fill="none" stroke="#29382f" stroke-width="12"/><path d="M256 270 L280 245 M208 318 H304" stroke="#29382f" stroke-width="12"/>';
}
async function main() {
  fs.mkdirSync(output, {recursive:true});
  for (const sector of sectors) {
    let body = `<rect width="1600" height="900" fill="${paper}"/><rect x="24" y="24" width="1552" height="852" fill="none" stroke="${ink}" stroke-width="5"/><rect x="24" y="24" width="1552" height="118" fill="${green}"/>`;
    body += text(70,101,52,sector.title,'#eeebd6') + text(70,190,25,sector.room);
    body += line(70,218,1530,218) + text(70,270,28,'OPERATING SEQUENCE');
    sector.steps.forEach((step,i) => body += text(70,332+i*62,31,step));
    body += `<rect x="70" y="711" width="1460" height="58" fill="${yellow}"/>` + text(90,750,30,sector.warning);
    body += text(70,817,27,sector.unit) + text(70,854,23,sector.route);
    body += `<g transform="translate(1115 250) scale(.8)">${symbol(sector.symbols[0])}</g>`;
    body += text(1175,590,26,sector.hazard);
    await save(sector.id+'_procedure',1600,900,body);
    // Keep the B-17 atlas's 4-column UV layout; the existing plate samples its top row.
    let icons = `<rect width="2048" height="1024" fill="${paper}"/>`;
    for (let i=0;i<8;i++) {
      const index=i%4, warning=index===0||index===2;
      icons += `<g transform="translate(${i%4*512} ${Math.floor(i/4)*512})"><rect x="18" y="18" width="476" height="476" rx="8" fill="#dedbc7" stroke="${ink}" stroke-width="6"/><rect x="25" y="25" width="462" height="60" fill="${warning?yellow:green}"/>`;
      icons += text(48,67,25,`${sector.id.toUpperCase()} / ${String(index+1).padStart(2,'0')}`,warning?ink:'#eeebd6');
      icons += symbol(sector.symbols[index]) + text(40,445,27,sector.labels[index]) + '</g>';
    }
    await save(sector.id+'_pictograms',2048,1024,icons);
  }
}
main().catch(error => {console.error(error);process.exitCode=1;});
