// Original instrument masters. NODE_PATH must provide the existing sharp dependency.
const fs = require('node:fs');
const path = require('node:path');
const sharp = require('sharp');
const root = path.join(__dirname, '..');
const out = path.join(root, 'content/graphics/instruments');
const green = '#b3c99a', dark = '#10251d', ink = '#28332d', paper = '#dad8c6';
const text = (x,y,size,value,color=green) => `<text x="${x}" y="${y}" font-family="Arial, sans-serif" font-size="${size}" fill="${color}">${value}</text>`;
const line = (x,y,xx,yy,color=green,width=4) => `<path d="M${x},${y} L${xx},${yy}" fill="none" stroke="${color}" stroke-width="${width}"/>`;
async function save(name,body) {
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="640" viewBox="0 0 1024 640">${body}</svg>\n`;
  fs.writeFileSync(path.join(out,name+'.svg'),svg);
  await sharp(Buffer.from(svg)).flatten().removeAlpha().png().toFile(path.join(out,name+'.png'));
}
async function main() {
  fs.mkdirSync(out,{recursive:true});
  let body = `<rect width="1024" height="640" fill="${dark}"/>`;
  body += text(40,70,52,'СИГНАЛ / C2') + text(40,117,27,'RC     0.5 V/div     10 ms/div');
  for(let x=40;x<=984;x+=47.2) body+=line(x,155,x,525,'#294235',1);
  for(let y=155;y<=525;y+=37) body+=line(40,y,984,y,'#294235',1);
  for(let ch=0;ch<2;ch++) {
    let d='';
    for(let x=0;x<=940;x+=2) d+=`${x?'L':'M'}${40+x},${250+ch*180-Math.sin(x/(94.4/(2*Math.PI)))*55-(ch?Math.sin(x/(94.4/(6*Math.PI)))*7:0)} `;
    body+=`<path d="${d}" fill="none" stroke="${ch?'#b59a54':green}" stroke-width="5"/>`;
    body+=text(50,190+ch*180,27,ch?'R0 / ЭТАЛОН':'C2 / ДАТЧИК');
  }
  body+=text(40,625,108,'50 Hz') + text(430,600,40,'Δ 0.8% / ОБРАЗЕЦ');
  await save('waveform',body);
  body=`<rect width="1024" height="640" fill="${dark}"/>`+text(40,70,50,'ЭЛЕКТРИЧЕСКАЯ СХЕМА');
  body+=text(40,117,26,'RC / РАСПРЕДЕЛЕНИЕ     СПРАВОЧНАЯ СХЕМА');
  body+=text(80,215,39,'ВВОД')+text(650,215,39,'РЕЗЕРВ');
  body+=line(90,290,360,290)+line(360,290,415,250)+line(445,290,550,290)+line(550,290,550,420);
  body+=line(650,290,780,290)+line(780,290,835,250)+line(865,290,935,290)+line(935,290,935,420)+line(180,420,935,420);
  body+='<rect x="210" y="270" width="72" height="40" fill="none" stroke="'+green+'" stroke-width="5"/>';
  body+=line(180,420,180,500)+line(550,420,550,500)+line(935,420,935,500);
  body+=text(55,550,32,'НАСОС')+text(430,550,32,'ФИЛЬТР')+text(770,550,32,'ОХЛАЖДЕНИЕ');
  body+=text(40,610,26,'ПОЛОЖЕНИЕ АППАРАТОВ — НА ПАНЕЛИ');
  await save('electrical',body);
  await save('electrical_paper',body.replaceAll(dark,paper).replaceAll(green,ink));
  // A retained observation frame, deliberately labelled as an archive, not a live camera.
  const capture = fs.readFileSync(path.join(root,'content/graphics/instruments/observation_frame.png'));
  body=`<rect width="1024" height="640" fill="${dark}"/><image x="0" y="85" width="1024" height="480" preserveAspectRatio="xMidYMid slice" href="data:image/png;base64,${capture.toString('base64')}"/>`;
  body+=text(32,60,44,'НАБЛЮДЕНИЕ / B-17')+text(32,615,37,'CAM 02    АРХИВ / КАДР 014');
  body+=line(465,325,559,325,'#9eaf9a',2)+line(512,280,512,370,'#9eaf9a',2);
  await save('surveillance',body);
  for(const cooling of [false,true]) {
    body=`<rect width="1024" height="640" fill="${paper}"/><rect x="16" y="16" width="992" height="608" fill="none" stroke="${ink}" stroke-width="5"/>`;
    if (!cooling) {
      body+=text(42,85,48,'ДАВЛЕНИЕ / ДИФФЕРЕНЦИАЛ',ink);
      body+=text(42,140,28,'P1 / P2    ИМПУЛЬСНЫЕ ЛИНИИ    Δp = P1 − P2',ink);
      for (const [x,id] of [[170,'P1'],[854,'P2']]) {
        body+=`<rect x="${x-65}" y="225" width="130" height="205" rx="58" fill="none" stroke="${ink}" stroke-width="6"/>`;
        body+=text(x-24,320,34,id,ink)+line(x,430,x,490,ink,6);
      }
      body+=line(100,490,924,490,ink,8);
      for (const [x,source,id] of [[410,170,'P1'],[614,854,'P2']]) {
        body+=line(source,225,source,192,ink,4)+line(source,192,x,192,ink,4)+line(x,192,x,250,ink,4);
        body+=`<circle cx="${x}" cy="295" r="45" fill="${paper}" stroke="${ink}" stroke-width="5"/>`;
        body+=line(x,295,x+18,268,ink,5)+line(x,340,x,395,ink,4);
        body+=text(x-28,365,26,id,ink);
      }
      body+=line(410,395,614,395,ink,5)+text(435,445,30,'Δp / bar',ink);
      body+=text(42,545,29,'РЕСИВЕР 1      СВЯЗАННАЯ ПАРА      РЕСИВЕР 2',ink);
      body+=text(42,590,27,'СХЕМА СОЕДИНЕНИЙ / ПОКАЗАНИЯ НА ПРИБОРАХ',ink);
      await save('pressure',body);
      continue;
    }
    body+=text(42,85,54,cooling?'ОХЛАЖДЕНИЕ / T':'ДАВЛЕНИЕ / p',ink);
    body+=text(42,140,28,'СХЕМА КОНТУРА / ШКАЛЫ ПРИБОРОВ',ink);
    body+=line(100,330,924,330,ink,9);
    for(const [i,x] of [170,512,854].entries()) {
      body+=`<circle cx="${x}" cy="280" r="62" fill="${paper}" stroke="${ink}" stroke-width="6"/>`;
      body+=line(x,280,x+28,244,ink,7)+line(x,342,x,390,ink,6);
      body+=text(x-70,450,36,(cooling?['ПОДАЧА','ВОЗВРАТ','ПЕРЕПАД']:['ВХОД','ВЫХОД','ПЕРЕПАД'])[i],ink);
      body+=text(x-66,505,34,(cooling?['°C','°C','Δp / bar']:['p / bar','p / bar','Δp / bar'])[i],ink);
    }
    body+=text(42,590,29,'ПОКАЗАНИЯ — НА СТРЕЛОЧНЫХ ПРИБОРАХ',ink);
    await save(cooling?'cooling':'pressure',body);
  }
}
main().catch(error=>{console.error(error);process.exitCode=1;});
