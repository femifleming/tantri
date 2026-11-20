// Shared data + SVG helpers for the Tantri UI mockups (static renders only).
const SHRUTI = [
  ["Sa", "1/1", 0], ["", "256/243", 90], ["re", "16/15", 112], ["", "10/9", 182], ["Re", "9/8", 204],
  ["", "32/27", 294], ["ga", "6/5", 316], ["Ga", "5/4", 386], ["", "81/64", 408], ["Ma", "4/3", 498],
  ["", "27/20", 520], ["Ma'", "45/32", 590], ["", "729/512", 612], ["Pa", "3/2", 702], ["", "128/81", 792],
  ["dha", "8/5", 814], ["", "5/3", 884], ["Dha", "27/16", 906], ["", "16/9", 996], ["ni", "9/5", 1018],
  ["Ni", "15/8", 1088], ["", "243/128", 1110]
];
// Raga Yaman (Kalyan thaat) – chosen shrutis
const YAMAN = new Set([0, 4, 7, 11, 13, 17, 20]);
const BOLS = [
  ["Na", "dayan rim", 60], ["Tin", "dayan maidan", 61], ["Tun", "dayan open", 62], ["Te", "dayan syahi", 63],
  ["Re", "syahi flat", 64], ["Ge", "bayan open", 48], ["Ghe", "bayan glide", 49], ["Ke", "bayan closed", 50],
  ["Dha", "Na + Ge", 52], ["Dhin", "Tin + Ge", 53], ["Ti", "dayan centre", 65], ["Tit", "flam", 66]
];

function arcPath(cx, cy, r, a0, a1) {
  const p = a => [cx + r * Math.cos(a), cy + r * Math.sin(a)];
  const [x0, y0] = p(a0), [x1, y1] = p(a1);
  return `M${x0},${y0} A${r},${r} 0 ${a1 - a0 > Math.PI ? 1 : 0} 1 ${x1},${y1}`;
}
// generic knob; style = {track, fill, body, pointer, size}
function knob(v, s) {
  const z = s.size || 54, c = z / 2, r = z / 2 - 4;
  const a0 = Math.PI * 0.75, a1 = a0 + Math.PI * 1.5 * v;
  return `<svg width="${z}" height="${z}" viewBox="0 0 ${z} ${z}">
    ${s.track ? `<path d="${arcPath(c, c, r, a0, Math.PI * 2.25)}" stroke="${s.track}" stroke-width="${s.w || 3}" fill="none" stroke-linecap="round"/>` : ""}
    ${s.fill ? `<path d="${arcPath(c, c, r, a0, a1)}" stroke="${s.fill}" stroke-width="${s.w || 3}" fill="none" stroke-linecap="round"/>` : ""}
    ${s.body ? `<circle cx="${c}" cy="${c}" r="${r - (s.inset || 6)}" fill="${s.body}" ${s.ring ? `stroke="${s.ring}" stroke-width="1.5"` : ""}/>` : ""}
    <line x1="${c + (r - 16) * Math.cos(a1) * 0.2}" y1="${c + (r - 16) * Math.sin(a1) * 0.2}" x2="${c + (r - (s.inset || 6) - 3) * Math.cos(a1)}" y2="${c + (r - (s.inset || 6) - 3) * Math.sin(a1)}" stroke="${s.pointer}" stroke-width="2.4" stroke-linecap="round"/>
  </svg>`;
}
// string displacement snapshot (sitar main string with jawari buzz)
function stringWave(w, h, col, seed = 1) {
  let d = "";
  for (let i = 0; i <= 200; i++) {
    const x = i / 200;
    let y = 0;
    // plucked-string modal snapshot: pluck point 0.13, phase = seed
    for (let k = 1; k < 40; k++) y += Math.sin(Math.PI * k * x) * Math.sin(Math.PI * k * 0.13) / (k * k) * Math.cos(k * seed * 2.3) * 1.9 * Math.exp(-k * 0.03);
    if (x < 0.07) y += 0.08 * Math.sin(x * 420) * (0.07 - x) * 14; // jawari grazing zone
    d += (i ? "L" : "M") + (x * w).toFixed(1) + "," + (h / 2 - y * h * 0.32).toFixed(1);
  }
  return `<path d="${d}" stroke="${col}" stroke-width="1.6" fill="none"/>`;
}
// shruti wheel: 22 positions placed by real cents
function shrutiWheel(R, opts) {
  const c = R + 30; let s = `<svg width="${c * 2}" height="${c * 2}" viewBox="0 0 ${c * 2} ${c * 2}">`;
  s += `<circle cx="${c}" cy="${c}" r="${R}" fill="none" stroke="${opts.ring}" stroke-width="1"/>`;
  s += `<circle cx="${c}" cy="${c}" r="${R * 0.62}" fill="none" stroke="${opts.ring}" stroke-width="0.6" stroke-dasharray="2 4"/>`;
  SHRUTI.forEach(([name, ratio, cents], i) => {
    const a = cents / 1200 * Math.PI * 2 - Math.PI / 2, on = YAMAN.has(i);
    const x = c + R * Math.cos(a), y = c + R * Math.sin(a);
    s += `<line x1="${c + R * 0.62 * Math.cos(a)}" y1="${c + R * 0.62 * Math.sin(a)}" x2="${x}" y2="${y}" stroke="${on ? opts.on : opts.ring}" stroke-width="${on ? 1.6 : 0.6}"/>`;
    s += `<circle cx="${x}" cy="${y}" r="${on ? 6 : 3}" fill="${on ? opts.on : opts.off}"/>`;
    if (name) s += `<text x="${c + (R + 18) * Math.cos(a)}" y="${c + (R + 18) * Math.sin(a) + 4}" text-anchor="middle" font-size="11" fill="${on ? opts.on : opts.text}" font-family="${opts.font}">${name}</text>`;
  });
  s += `<text x="${c}" y="${c - 6}" text-anchor="middle" font-size="22" fill="${opts.text}" font-family="${opts.font}">यमन</text>`;
  s += `<text x="${c}" y="${c + 14}" text-anchor="middle" font-size="10" fill="${opts.text}" font-family="${opts.font}" opacity=".7">22 shruti · just</text>`;
  return s + "</svg>";
}
// tabla membrane: syahi-loaded drum, nearly harmonic modes
// extra rows: taraf strings, bol grid, tanpura cycle
function tarafRow(col, sub) {
  const names = ["Sa","Re","Ga","Ma'","Pa","Dha","Ni","Sa'","Re'","Ga'","Ma'","Pa'","Sa''"];
  return `<div style="display:flex;gap:4px;margin-top:10px">` + names.map((n,i)=>`<div style="flex:1;text-align:center;font-size:10px;color:${sub}"><div style="height:46px;width:${1+(13-i)*.12}px;margin:0 auto 3px;background:${col};opacity:${.45+.5*Math.abs(Math.sin(i*1.3))}"></div>${n}</div>`).join("") + `</div>`;
}
function bolGrid(cell, sub) {
  return `<div style="display:grid;grid-template-columns:repeat(4,1fr);gap:5px;margin-top:10px">` + BOLS.map(([b,d,n])=>`<div style="${cell};padding:5px 6px;font-size:14px">${b}<div style="font-size:9.5px;color:${sub}">${d}</div></div>`).join("") + `</div>`;
}
function tanpuraCycle(col, sub, font) {
  const n = [["Pa",.55],["Sa",.8],["Sa",.8],["sa",1]];
  let s = `<svg width="210" height="120" style="margin-top:6px">`;
  n.forEach(([t,a],i)=>{ const x=26+i*52;
    for(let r=0;r<4;r++) s+=`<circle cx="${x}" cy="54" r="${8+r*9}" fill="none" stroke="${col}" stroke-width="${1.4-r*.3}" opacity="${a*(1-r*.22)}"/>`;
    s+=`<circle cx="${x}" cy="54" r="5" fill="${col}"/><text x="${x}" y="108" text-anchor="middle" font-size="13" fill="${sub}" font-family="${font}">${t}</text>`;});
  return s+"</svg>";
}
function membrane(r, opts) {
  const c = r + 4;
  let s = `<svg width="${c * 2}" height="${c * 2}" viewBox="0 0 ${c * 2} ${c * 2}">
    <circle cx="${c}" cy="${c}" r="${r}" fill="${opts.skin}" stroke="${opts.rim}" stroke-width="5"/>
    <circle cx="${c}" cy="${c}" r="${r * 0.82}" fill="none" stroke="${opts.rim}" stroke-width="0.8" opacity=".6"/>`;
  if (opts.syahi) s += `<circle cx="${c + (opts.off || 0)}" cy="${c}" r="${r * 0.42}" fill="${opts.syahi}"/>`;
  for (let k = 1; k <= 5; k++) s += `<circle cx="${c}" cy="${c}" r="${r * (0.18 + k * 0.13)}" fill="none" stroke="${opts.mode}" stroke-width="0.7" opacity="${0.5 - k * 0.07}" stroke-dasharray="${k * 2} 3"/>`;
  return s + "</svg>";
}

// Teentaal 16-matra cycle: sam (X), khali (0)
function talaRing(R, on, off, text, font) {
  const c = R + 16; let s = `<svg width="${c*2}" height="${c*2}">`;
  const bols = ["Dha","Dhin","Dhin","Dha","Dha","Dhin","Dhin","Dha","Dha","Tin","Tin","Ta","Ta","Dhin","Dhin","Dha"];
  bols.forEach((b,i)=>{ const a=i/16*Math.PI*2-Math.PI/2, x=c+R*Math.cos(a), y=c+R*Math.sin(a);
    const mark = i==0?"X":i==8?"0":(i==4||i==12)?String(i/4+1):"";
    s+=`<circle cx="${x}" cy="${y}" r="${mark?11:8}" fill="${i<6?on:"none"}" stroke="${i==8?off:on}" stroke-width="1.3" ${i==8?'stroke-dasharray="2 2"':""}/>`;
    s+=`<text x="${c+(R-30)*Math.cos(a)}" y="${c+(R-30)*Math.sin(a)+3}" text-anchor="middle" font-size="8.5" fill="${text}" font-family="${font}">${b}</text>`;
    if(mark) s+=`<text x="${x}" y="${y+4}" text-anchor="middle" font-size="10" fill="${i<6?"#fff":text}" font-family="${font}">${mark}</text>`; });
  s+=`<text x="${c}" y="${c+4}" text-anchor="middle" font-size="13" fill="${text}" font-family="${font}">tīntāl · 16</text>`;
  return s+"</svg>";
}
// tanpura strings, top view
function tanpuraStrings(w, h, col, sub, font) {
  let s = `<svg width="${w}" height="${h}">`; const n=["Pa","Sa","Sa","sa"];
  n.forEach((t,i)=>{ const x=w*(.2+i*.2); s+=`<path d="M${x},6 Q${x+(i==1?4:-3)},${h/2} ${x},${h-24}" stroke="${col}" stroke-width="${i==3?2.6:1.4}" fill="none"/><circle cx="${x}" cy="${h-34}" r="3" fill="${sub}"/>`;
    s+=`<text x="${x}" y="${h-6}" text-anchor="middle" font-size="12" fill="${sub}" font-family="${font}">${t}</text>`; });
  return s + `<rect x="${w*.12}" y="${h-38}" width="${w*.76}" height="8" fill="${sub}" opacity=".35"/></svg>`;
}
