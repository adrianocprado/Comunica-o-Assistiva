/*
  PRIYA - PROJETO ROBÓTICO DE INCLUSÃO E ACOLHIMENTO
  Comunicador assistivo - ESP32 (versão sem Telegram e sem rotinas)
  Contato: adrianocprado@gmail.com

  Hardware: ESP32 DevKit 30 pinos, display TFT 1.8" ST7735 128x160 com slot SD,
            leitor RFID RC522, amplificador I2S MAX98357A, 4 botões de atalho.

  Bibliotecas (Gerenciador de Bibliotecas da Arduino IDE):
    - Adafruit GFX Library
    - Adafruit ST7735 and ST7789 Library
    - MFRC522 (autor GithubCommunity / miguelbalboa)
    - ESP8266Audio (Earle F. Philhower)
  Placa: "ESP32 Dev Module"

  Pastas criadas no cartão SD (automaticamente):
    /audio/*.wav   sons (WAV 16 bits, mono, 22050 Hz - o painel converte sozinho)
    /img/*.bin     imagens (128x120 + faixa da palavra embaixo, RGB565 - o painel converte sozinho)
    /map.txt       tabela botão/cartão -> texto, som, imagem

  Painel: conecte no Wi-Fi "Comunicador" (senha inicial "comunicador")
          e abra http://192.168.4.1
  Para restaurar nome/senha do Wi-Fi e volume: ligue o aparelho segurando o botão 1.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <MFRC522.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

// ===================== PINOS =====================
#define PIN_SCK       18   // SPI compartilhado: TFT + SD + RC522
#define PIN_MISO      19
#define PIN_MOSI      23
#define PIN_TFT_CS    5
#define PIN_TFT_DC    4    // pino "A0" do display
#define PIN_TFT_RST   22
#define PIN_SD_CS     21
#define PIN_RFID_SS   16   // pino "SDA" do RC522
#define PIN_RFID_RST  17
#define PIN_I2S_BCLK  26
#define PIN_I2S_LRC   25
#define PIN_I2S_DIN   27
const uint8_t PIN_BTN[4] = {13, 14, 32, 33};

#define LARG 128           // display em retrato (rotação 0)
#define ALT  160
#define ALT_IMG 120        // altura da imagem; abaixo dela fica a faixa da palavra (40 px)
#define MAXI 150           // máximo de itens (4 botões + cartões)
#define TEMPO_MIN_TELA 3000

// ===================== OBJETOS =====================
Adafruit_ST7735 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
MFRC522 rfid(PIN_RFID_SS, PIN_RFID_RST);
WebServer server(80);
Preferences prefs;

AudioGeneratorWAV*  wav   = nullptr;
AudioFileSourceSD*  fonte = nullptr;
AudioOutputI2S*     saida = nullptr;

struct Item { String id, label, audio, img; };
Item itens[MAXI];
int  nItens = 0;

String apSsid, apPass;
int    volume = 50;
String ultimoUid = "";
unsigned long ultimoUidMs = 0;
bool telaOcupada = false;
unsigned long ate = 0;
File upArq;

// ===================== PAINEL WEB =====================
const char PAGINA[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="pt-br"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PRIYA - Comunicador</title>
<style>
body{font-family:sans-serif;margin:0;background:#f4f6f8;color:#222}
header{background:#1565c0;color:#fff;padding:12px 16px;text-align:center}
header b{display:block;font-size:18px;letter-spacing:.5px}
header small{font-size:13px;opacity:.9}
footer{text-align:center;padding:14px;font-size:13px;color:#555}
footer a{color:#1565c0}
main{padding:12px;max-width:950px;margin:auto}
.c{background:#fff;border-radius:8px;padding:12px;margin-bottom:12px;box-shadow:0 1px 3px #0003}
h2{margin:0 0 8px;font-size:17px}
table{width:100%;border-collapse:collapse}
td,th{padding:4px;border-bottom:1px solid #ddd;text-align:left;font-size:14px}
input,select,button{font-size:15px;padding:6px;margin:2px;max-width:100%}
button{background:#1565c0;color:#fff;border:0;border-radius:6px;cursor:pointer}
button.g{background:#2e7d32}button.r{background:#c62828}
#msg{position:fixed;bottom:10px;left:10px;right:10px;background:#333;color:#fff;padding:10px;border-radius:6px;display:none}
</style></head><body>
<header><b>PRIYA - PROJETO ROBÓTICO DE INCLUSÃO E ACOLHIMENTO</b><small>Comunicador assistivo</small></header><main>
<div class="c"><h2>1. Enviar arquivos</h2>
<p>Sons (mp3, wav, m4a, ogg...): <input type="file" id="fa" accept="audio/*" multiple> <button onclick="upA()">Enviar sons</button></p>
<p>Imagens (jpg, png...): <input type="file" id="fi" accept="image/*" multiple> <button onclick="upI()">Enviar imagens</button></p>
<small>Você pode escolher vários arquivos de uma vez. O painel converte tudo para o formato do aparelho e o nome do arquivo vira o nome na lista.</small></div>
<div class="c"><h2>2. Botões e cartões</h2>
<div style="overflow-x:auto"><table id="t"></table></div>
<p><button class="g" onclick="addCard()">+ Cadastrar cartão novo</button> <span id="rs"></span></p>
<p><button onclick="salvar()">Salvar tudo</button></p></div>
<div class="c"><h2>3. Ajustes</h2>
Volume: <input type="range" id="vol" min="0" max="100"> <b id="vv"></b>
<p>Wi-Fi do aparelho: <input id="ws" maxlength="20" placeholder="nome"> <input id="wp" maxlength="20" placeholder="senha (8+)"> <button onclick="setWifi()">Salvar Wi-Fi</button></p></div>
</main>
<footer>Contato: <a href="mailto:adrianocprado@gmail.com">adrianocprado@gmail.com</a></footer>
<div id="msg"></div>
<script>
const $=i=>document.getElementById(i);
let itens=[],audios=[],imgs=[],reg=null;
function aviso(t){const m=$('msg');m.textContent=t;m.style.display='block';clearTimeout(window._t);window._t=setTimeout(()=>m.style.display='none',4000)}
function esc(s){return s.replace(/&/g,'&amp;').replace(/"/g,'&quot;').replace(/</g,'&lt;')}
function opts(l,sel){return '<option value="">(nenhum)</option>'+l.map(n=>'<option'+(n==sel?' selected':'')+'>'+n+'</option>').join('')}
function desenhar(){
 let h='<tr><th>Botão / Cartão</th><th>Texto na tela</th><th>Som</th><th>Imagem</th><th></th></tr>';
 itens.forEach((it,i)=>{
  const bt=it.id.length==2;
  h+='<tr><td>'+(bt?'Botão '+it.id[1]:'Cartão<br><small>'+it.id+'</small>')+'</td>'
   +'<td><input data-i="'+i+'" data-k="label" value="'+esc(it.label)+'"></td>'
   +'<td><select data-i="'+i+'" data-k="audio">'+opts(audios,it.audio)+'</select></td>'
   +'<td><select data-i="'+i+'" data-k="img">'+opts(imgs,it.img)+'</select></td>'
   +'<td><button class="g" data-a="play" data-i="'+i+'">▶</button>'
   +(bt?'':'<button class="r" data-a="del" data-i="'+i+'">✕</button>')+'</td></tr>';
 });
 $('t').innerHTML=h;
}
$('t').addEventListener('change',e=>{const d=e.target.dataset;if(d.k)itens[d.i][d.k]=e.target.value.replace(/[|\n]/g,' ')});
$('t').addEventListener('click',async e=>{const d=e.target.dataset;
 if(d.a=='del'){itens.splice(d.i,1);desenhar()}
 if(d.a=='play'){await salvar(true);fetch('/api/play?id='+itens[d.i].id)}});
const T=async u=>(await fetch(u)).text();
async function arquivos(){
 const f=(await T('/api/files')).split('\n').filter(l=>l);
 audios=f.filter(x=>x[0]=='a').map(x=>x.slice(2));
 imgs=f.filter(x=>x[0]=='i').map(x=>x.slice(2));
}
async function carregar(){
 itens=(await T('/api/list')).split('\n').filter(l=>l.trim()).map(l=>{const p=l.split('|');return{id:p[0],label:p[1]||'',audio:p[2]||'',img:p[3]||''}});
 await arquivos();
 const c=(await T('/api/cfg')).split('|');
 $('vol').value=c[0];$('vv').textContent=c[0];$('ws').value=c[1];$('wp').value=c[2];
 desenhar();
}
async function salvar(quieto){
 const t=itens.map(i=>[i.id,i.label,i.audio,i.img].join('|')).join('\n')+'\n';
 await fetch('/api/save',{method:'POST',body:t});
 if(!quieto)aviso('Salvo!');
}
$('vol').oninput=()=>$('vv').textContent=$('vol').value;
$('vol').onchange=()=>fetch('/api/vol?v='+$('vol').value);
function addCard(){
 $('rs').textContent='Aproxime o cartão do leitor...';
 if(reg)clearInterval(reg);
 const ini=Date.now();
 reg=setInterval(async()=>{
  const r=(await T('/api/lastuid')).split('|');
  if(r[0]&&Number(r[1])<Date.now()-ini){
   clearInterval(reg);reg=null;$('rs').textContent='';
   if(itens.find(i=>i.id==r[0])){aviso('Esse cartão já está cadastrado.');return}
   itens.push({id:r[0],label:'Novo cartão',audio:'',img:''});desenhar();
   aviso('Cartão '+r[0]+' adicionado. Escolha texto, som e imagem e clique em Salvar tudo.');
  }else if(Date.now()-ini>30000){clearInterval(reg);reg=null;$('rs').textContent='Tempo esgotado.'}
 },700);
}
function slug(n){return n.toLowerCase().normalize('NFD').replace(/[\u0300-\u036f]/g,'').replace(/\.[^.]+$/,'').replace(/[^a-z0-9]+/g,'_').replace(/^_|_$/g,'').slice(0,24)||'arq'}
async function enviar(blob,nome){
 const fd=new FormData();fd.append('f',blob,nome);
 const r=await fetch('/upload',{method:'POST',body:fd});
 if(!r.ok)throw new Error('falha no envio');
}
function nomeUnico(f,usados,ext){let b=slug(f.name),n=b,i=2;while(usados.has(n+ext)){n=b+'_'+i++}usados.add(n+ext);return n+ext}
const pausa=ms=>new Promise(r=>setTimeout(r,ms));
async function upA(){
 const fl=[...$('fa').files];if(!fl.length){aviso('Escolha um ou mais arquivos de som.');return}
 const ctx=new (window.AudioContext||window.webkitAudioContext)(),usados=new Set();let ok=0;
 for(let k=0;k<fl.length;k++){
  const f=fl[k];
  try{
   aviso('Convertendo e enviando '+(k+1)+' de '+fl.length+': '+f.name);
   const buf=await ctx.decodeAudioData(await f.arrayBuffer());
   const sr=22050,len=Math.ceil(buf.duration*sr);
   const off=new OfflineAudioContext(1,len,sr);
   const s=off.createBufferSource();s.buffer=buf;s.connect(off.destination);s.start();
   const d=(await off.startRendering()).getChannelData(0);
   const w=new DataView(new ArrayBuffer(44+len*2));
   const tx=(o,t)=>{for(let i=0;i<t.length;i++)w.setUint8(o+i,t.charCodeAt(i))};
   tx(0,'RIFF');w.setUint32(4,36+len*2,true);tx(8,'WAVEfmt ');w.setUint32(16,16,true);
   w.setUint16(20,1,true);w.setUint16(22,1,true);w.setUint32(24,sr,true);w.setUint32(28,sr*2,true);
   w.setUint16(32,2,true);w.setUint16(34,16,true);tx(36,'data');w.setUint32(40,len*2,true);
   for(let i=0;i<len;i++){const v=Math.max(-1,Math.min(1,d[i]));w.setInt16(44+i*2,v<0?v*32768:v*32767,true)}
   await enviar(new Blob([w]),nomeUnico(f,usados,'.wav'));ok++;
  }catch(e){aviso('Erro em '+f.name+': '+e.message);await pausa(1800)}
 }
 ctx.close();$('fa').value='';await arquivos();desenhar();
 aviso(ok+' de '+fl.length+' som(ns) enviado(s).');
}
async function upI(){
 const fl=[...$('fi').files];if(!fl.length){aviso('Escolha uma ou mais imagens.');return}
 const usados=new Set();let ok=0;
 for(let k=0;k<fl.length;k++){
  const f=fl[k];
  try{
   aviso('Convertendo e enviando '+(k+1)+' de '+fl.length+': '+f.name);
   const bm=await createImageBitmap(f),W=128,H=120;
   const c=document.createElement('canvas');c.width=W;c.height=H;
   const x=c.getContext('2d');x.fillStyle='#000';x.fillRect(0,0,W,H);
   const q=Math.min(W/bm.width,H/bm.height),w=bm.width*q,h=bm.height*q;
   x.drawImage(bm,(W-w)/2,(H-h)/2,w,h);
   const px=x.getImageData(0,0,W,H).data,o=new Uint8Array(W*H*2);
   for(let i=0,j=0;i<px.length;i+=4,j+=2){
    const v=((px[i]&0xF8)<<8)|((px[i+1]&0xFC)<<3)|(px[i+2]>>3);o[j]=v&255;o[j+1]=v>>8}
   await enviar(new Blob([o]),nomeUnico(f,usados,'.bin'));ok++;
  }catch(e){aviso('Erro em '+f.name+': '+e.message);await pausa(1800)}
 }
 $('fi').value='';await arquivos();desenhar();
 aviso(ok+' de '+fl.length+' imagem(ns) enviada(s).');
}
async function setWifi(){
 const s=$('ws').value.trim(),p=$('wp').value.trim();
 if(!s||p.length<8||/[|]/.test(s+p)){aviso('Informe o nome e uma senha com 8+ caracteres (sem o símbolo |).');return}
 if(!confirm('O aparelho vai reiniciar. Depois conecte na nova rede.'))return;
 fetch('/api/wifi',{method:'POST',body:s+'|'+p});aviso('Reiniciando...');
}
carregar();
</script></body></html>
)rawliteral";

// ===================== UTILIDADES =====================
// A fonte padrão do display não tem acentos: troca por letras simples só na tela
String semAcento(const String& s) {
  String o;
  for (unsigned i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (c == 0xC3 && i + 1 < s.length()) {
      uint8_t d = s[++i];
      char r = '?';
      if (d >= 0x80 && d <= 0x85) r = 'A';
      else if (d == 0x87) r = 'C';
      else if (d >= 0x88 && d <= 0x8B) r = 'E';
      else if (d >= 0x8C && d <= 0x8F) r = 'I';
      else if (d == 0x91) r = 'N';
      else if (d >= 0x92 && d <= 0x96) r = 'O';
      else if (d >= 0x99 && d <= 0x9C) r = 'U';
      else if (d >= 0xA0 && d <= 0xA5) r = 'a';
      else if (d == 0xA7) r = 'c';
      else if (d >= 0xA8 && d <= 0xAB) r = 'e';
      else if (d >= 0xAC && d <= 0xAF) r = 'i';
      else if (d == 0xB1) r = 'n';
      else if (d >= 0xB2 && d <= 0xB6) r = 'o';
      else if (d >= 0xB9 && d <= 0xBC) r = 'u';
      o += r;
    } else if (c < 0x80) {
      o += (char)c;
    }
  }
  return o;
}

String limparNome(String n) {
  int p = max(n.lastIndexOf('/'), n.lastIndexOf('\\'));
  if (p >= 0) n = n.substring(p + 1);
  n.toLowerCase();
  String o;
  for (unsigned i = 0; i < n.length() && o.length() < 32; i++) {
    char c = n[i];
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') o += c;
  }
  return o;
}

Item* buscar(const String& id) {
  for (int i = 0; i < nItens; i++) if (itens[i].id == id) return &itens[i];
  return nullptr;
}

void carregarMapa() {
  nItens = 0;
  File f = SD.open("/map.txt");
  if (f) {
    while (f.available() && nItens < MAXI) {
      String l = f.readStringUntil('\n');
      l.trim();
      if (l.length() < 2) continue;
      int p1 = l.indexOf('|');
      int p2 = l.indexOf('|', p1 + 1);
      int p3 = l.indexOf('|', p2 + 1);
      if (p1 < 0 || p2 < 0 || p3 < 0) continue;
      itens[nItens].id    = l.substring(0, p1);
      itens[nItens].label = l.substring(p1 + 1, p2);
      itens[nItens].audio = l.substring(p2 + 1, p3);
      itens[nItens].img   = l.substring(p3 + 1);
      nItens++;
    }
    f.close();
  }
  for (int b = 1; b <= 4; b++) {            // os 4 botões sempre existem
    String id = "B" + String(b);
    if (!buscar(id) && nItens < MAXI) {
      itens[nItens].id = id;
      itens[nItens].label = "Botao " + String(b);
      itens[nItens].audio = "";
      itens[nItens].img = "";
      nItens++;
    }
  }
}

void salvarMapa(const String& corpo) {
  SD.remove("/map.txt");
  File f = SD.open("/map.txt", FILE_WRITE);
  if (f) { f.print(corpo); f.close(); }
  carregarMapa();
}

void aplicarVolume() {
  if (saida) saida->SetGain(volume / 100.0 * 2.5);   // se distorcer, reduza o 2.5
}

// ===================== TELA =====================
void centro(const String& s, int y, uint8_t tam, uint16_t cor) {
  int16_t x1, y1; uint16_t w, h;
  tft.setTextSize(tam);
  tft.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((tft.width() - w) / 2, y);
  tft.setTextColor(cor);
  tft.print(s);
}

// Escreve texto centralizado, com quebra de linha, dentro de uma faixa da tela
void textoNaArea(String s, int y0, int h, int tamMax) {
  s = semAcento(s);
  if (s.length() == 0) return;
  String lin[10]; int n = 0, tam = tamMax;
  for (; tam >= 1; tam--) {
    int maxc = tft.width() / (6 * tam);
    n = 0; String atual = ""; bool largo = false;
    int i = 0;
    while (i <= (int)s.length()) {
      int j = s.indexOf(' ', i); if (j < 0) j = s.length();
      String w = s.substring(i, j);
      if (w.length() > (unsigned)maxc) largo = true;
      if (atual.length() == 0) atual = w;
      else if (atual.length() + 1 + w.length() <= (unsigned)maxc) atual += " " + w;
      else { if (n < 10) lin[n++] = atual; atual = w; }
      i = j + 1;
    }
    if (n < 10) lin[n++] = atual;
    if (!largo && n * (8 * tam + 2) <= h) break;
    if (tam == 1) break;
  }
  int hTot = n * (8 * tam + 2);
  int y = y0 + (h - hTot) / 2; if (y < y0) y = y0;
  for (int k = 0; k < n; k++) { centro(lin[k], y, tam, ST77XX_WHITE); y += 8 * tam + 2; }
}

void mostrarTexto(String s) {
  tft.fillScreen(ST77XX_BLACK);
  textoNaArea(s, 0, tft.height(), 3);
}

// Mostra a imagem e, abaixo dela, a palavra (texto) da frase.
// Aceita o formato novo (128x120) e o antigo de tela cheia (128x160).
bool mostrarImagem(const String& arq, const String& palavra) {
  File f = SD.open("/img/" + arq);
  if (!f) return false;
  size_t tam = f.size();
  int altImg;
  if (tam >= (size_t)LARG * ALT * 2) altImg = ALT;
  else if (tam >= (size_t)LARG * ALT_IMG * 2) altImg = ALT_IMG;
  else { f.close(); return false; }
  static uint16_t buf[LARG * 8];
  for (int y = 0; y < altImg; y += 8) {
    f.read((uint8_t*)buf, sizeof(buf));          // lê do SD primeiro...
    tft.startWrite();                            // ...depois escreve no display
    tft.setAddrWindow(0, y, LARG, 8);
    tft.writePixels(buf, LARG * 8);
    tft.endWrite();
  }
  f.close();
  if (altImg < ALT) {
    tft.fillRect(0, altImg, LARG, ALT - altImg, ST77XX_BLACK);
    textoNaArea(palavra, altImg, ALT - altImg, 3);
  }
  return true;
}

void mostrarAbertura() {
  tft.fillScreen(ST77XX_BLACK);
  centro("PRIYA", 24, 4, ST77XX_CYAN);
  centro("PROJETO ROBOTICO", 78, 1, ST77XX_WHITE);
  centro("DE INCLUSAO E", 90, 1, ST77XX_WHITE);
  centro("ACOLHIMENTO", 102, 1, ST77XX_WHITE);
  centro("Comunicador assistivo", 132, 1, 0x8410);
}

void mostrarHome() {
  tft.fillScreen(ST77XX_BLACK);
  centro("PRIYA", 8, 3, ST77XX_CYAN);
  centro("Comunicador assistivo", 38, 1, 0x8410);
  centro("Aproxime", 66, 2, ST77XX_WHITE);
  centro("um cartao", 86, 2, ST77XX_WHITE);
  centro("ou aperte um botao", 114, 1, ST77XX_YELLOW);
  String r = "Rede: " + semAcento(apSsid);
  if (r.length() > 21) r = r.substring(0, 21);
  centro(r, 134, 1, 0x8410);
  centro(WiFi.softAPIP().toString(), 146, 1, 0x8410);
  telaOcupada = false;
}

void mostrarDesconhecido(const String& uid) {
  tft.fillScreen(ST77XX_BLACK);
  centro("Cartao", 20, 2, ST77XX_YELLOW);
  centro("novo", 40, 2, ST77XX_YELLOW);
  centro("Cadastre no painel:", 80, 1, ST77XX_WHITE);
  centro(WiFi.softAPIP().toString(), 94, 1, ST77XX_CYAN);
  centro(uid, 124, 1, 0x8410);
}

// ===================== ÁUDIO =====================
void pararAudio() {
  if (wav) { if (wav->isRunning()) wav->stop(); delete wav; wav = nullptr; }
  if (fonte) { fonte->close(); delete fonte; fonte = nullptr; }
}

void tocarArquivo(const String& arq) {
  if (arq.length() == 0) return;
  String p = "/audio/" + arq;
  if (!SD.exists(p)) return;
  fonte = new AudioFileSourceSD(p.c_str());
  wav = new AudioGeneratorWAV();
  if (!wav->begin(fonte, saida)) pararAudio();
}

void tocarId(const String& id) {
  pararAudio();
  Item* it = buscar(id);
  if (!it) { mostrarDesconhecido(id); telaOcupada = true; ate = millis() + TEMPO_MIN_TELA; return; }
  bool ok = false;
  if (it->img.length()) ok = mostrarImagem(it->img, it->label);
  if (!ok) mostrarTexto(it->label.length() ? it->label : it->id);
  tocarArquivo(it->audio);                       // imagem e som quase ao mesmo tempo
  telaOcupada = true;
  ate = millis() + TEMPO_MIN_TELA;
}

// ===================== ENTRADAS =====================
void verificarBotoes() {
  static bool ant[4] = {false, false, false, false};
  static unsigned long t[4] = {0, 0, 0, 0};
  for (int i = 0; i < 4; i++) {
    bool p = digitalRead(PIN_BTN[i]) == LOW;
    if (p && !ant[i] && millis() - t[i] > 60) {
      t[i] = millis();
      tocarId("B" + String(i + 1));
    }
    ant[i] = p;
  }
}

void lerCartao() {
  static unsigned long t = 0;
  if (millis() - t < 120) return;
  t = millis();
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;
  String uid;
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  rfid.PICC_HaltA();          // cartão parado sobre o leitor não repete
  rfid.PCD_StopCrypto1();
  ultimoUid = uid;
  ultimoUidMs = millis();
  tocarId(uid);
}

// ===================== SERVIDOR WEB =====================
void listar(const char* dir, const char* tag, String& s) {
  File d = SD.open(dir);
  if (!d) return;
  File f = d.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      String n = f.name();
      int p = n.lastIndexOf('/');
      if (p >= 0) n = n.substring(p + 1);
      s += String(tag) + ":" + n + "\n";
    }
    f.close();
    f = d.openNextFile();
  }
  d.close();
}

void hLista() {
  String s;
  for (int i = 0; i < nItens; i++)
    s += itens[i].id + "|" + itens[i].label + "|" + itens[i].audio + "|" + itens[i].img + "\n";
  server.send(200, "text/plain; charset=utf-8", s);
}
void hArquivos() {
  String s;
  listar("/audio", "a", s);
  listar("/img", "i", s);
  server.send(200, "text/plain; charset=utf-8", s);
}
void hSalvar() { pararAudio(); salvarMapa(server.arg("plain")); server.send(200, "text/plain", "ok"); }
void hUid()    { server.send(200, "text/plain", ultimoUid + "|" + String(millis() - ultimoUidMs)); }
void hPlay()   { tocarId(server.arg("id")); server.send(200, "text/plain", "ok"); }
void hCfg()    { server.send(200, "text/plain; charset=utf-8", String(volume) + "|" + apSsid + "|" + apPass); }
void hVol() {
  volume = constrain(server.arg("v").toInt(), 0, 100);
  prefs.putInt("vol", volume);
  aplicarVolume();
  server.send(200, "text/plain", "ok");
}
void hWifi() {
  String b = server.arg("plain");
  int p = b.indexOf('|');
  if (p < 1 || (int)b.length() - p - 1 < 8) { server.send(400, "text/plain", "erro"); return; }
  prefs.putString("ssid", b.substring(0, p));
  prefs.putString("pass", b.substring(p + 1));
  server.send(200, "text/plain", "ok");
  delay(600);
  ESP.restart();
}
void hUploadFim() { server.send(200, "text/plain", "ok"); }
void hUpload() {
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    pararAudio();
    String n = limparNome(u.filename);
    String dir = n.endsWith(".wav") ? "/audio/" : (n.endsWith(".bin") ? "/img/" : "");
    upArq = File();
    if (dir.length() && n.length() > 4) {
      SD.remove(dir + n);
      upArq = SD.open(dir + n, FILE_WRITE);
    }
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (upArq) upArq.write(u.buf, u.currentSize);
  } else if (u.status == UPLOAD_FILE_END || u.status == UPLOAD_FILE_ABORTED) {
    if (upArq) upArq.close();
  }
}

// ===================== SETUP / LOOP =====================
void setup() {
  Serial.begin(115200);
  Serial.println("\nPRIYA - iniciando");
  pinMode(PIN_TFT_CS, OUTPUT);  digitalWrite(PIN_TFT_CS, HIGH);
  pinMode(PIN_SD_CS, OUTPUT);   digitalWrite(PIN_SD_CS, HIGH);
  pinMode(PIN_RFID_SS, OUTPUT); digitalWrite(PIN_RFID_SS, HIGH);
  for (int i = 0; i < 4; i++) pinMode(PIN_BTN[i], INPUT_PULLUP);

  prefs.begin("comm", false);
  if (digitalRead(PIN_BTN[0]) == LOW) prefs.clear();      // botão 1 ao ligar = restaura padrão
  apSsid = prefs.getString("ssid", "Comunicador");
  apPass = prefs.getString("pass", "comunicador");
  volume = prefs.getInt("vol", 50);

  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
  tft.initR(INITR_BLACKTAB);     // se as cores/bordas ficarem erradas, teste INITR_GREENTAB ou INITR_REDTAB
  tft.setRotation(0);            // retrato; se ficar de cabeça para baixo, use 2
  tft.fillScreen(ST77XX_BLACK);
  Serial.println("Display iniciado");
  mostrarAbertura();             // tela de abertura com o nome PRIYA
  delay(2500);
  rfid.PCD_Init();

  while (!SD.begin(PIN_SD_CS, SPI, 4000000)) {
    Serial.println("SD nao encontrado");
    mostrarTexto("Coloque o cartao SD");
    delay(1500);
  }
  Serial.println("SD ok");
  SD.mkdir("/audio");
  SD.mkdir("/img");
  carregarMapa();

  saida = new AudioOutputI2S();
  saida->SetPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DIN);
  saida->SetOutputModeMono(true);
  aplicarVolume();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid.c_str(), apPass.c_str(), 1, 0, 8);

  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html; charset=utf-8", PAGINA); });
  server.on("/api/list", HTTP_GET, hLista);
  server.on("/api/files", HTTP_GET, hArquivos);
  server.on("/api/save", HTTP_POST, hSalvar);
  server.on("/api/lastuid", HTTP_GET, hUid);
  server.on("/api/play", HTTP_GET, hPlay);
  server.on("/api/cfg", HTTP_GET, hCfg);
  server.on("/api/vol", HTTP_GET, hVol);
  server.on("/api/wifi", HTTP_POST, hWifi);
  server.on("/upload", HTTP_POST, hUploadFim, hUpload);
  server.begin();
  Serial.print("Painel em http://");
  Serial.println(WiFi.softAPIP());

  mostrarHome();
}

void loop() {
  server.handleClient();
  if (wav) { if (!wav->isRunning() || !wav->loop()) pararAudio(); }
  verificarBotoes();
  lerCartao();
  if (telaOcupada && !wav && millis() > ate) mostrarHome();
}
