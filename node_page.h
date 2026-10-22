/*
 * node_page.h — web pages of the LoRaWAN node (kept in flash).
 *
 * Same look as the ESP32 SX1302 gateway page. The page is static: it reads
 * /config.json to fill the form and /status every 5 s. The same page serves
 * the ABP and the OTAA firmware (elements with data-m="abp|otaa" are hidden
 * according to the firmware that answers).
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef NODE_PAGE_H
#define NODE_PAGE_H

#include <Arduino.h>

static const char FAVICON_SVG[] PROGMEM = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32"><rect width="32" height="32" rx="7" fill="#0b7a5f"/><g fill="none" stroke="#fff" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><rect x="8.5" y="14" width="15" height="12" rx="2.2"/><path d="M12.5 14V9.5"/><path d="M16.2 7.6a5 5 0 0 1 0 6M19.4 5a9 9 0 0 1 0 11.2" opacity=".85"/></g><circle cx="12.5" cy="8.4" r="1.9" fill="#fff"/><circle cx="12.6" cy="20" r="1.5" fill="#fff"/><path d="M16.5 18.5h4M16.5 21.5h4" stroke="#fff" stroke-width="1.8" stroke-linecap="round"/></svg>)SVG";

/* small pages (saved / rebooting / logged out) */
static const char SIMPLE_HEAD[] PROGMEM =
    "<!DOCTYPE html><html lang='pt-BR'><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<link rel='icon' type='image/svg+xml' href='/favicon.svg'>"
    "<style>body{margin:0;min-height:100vh;display:grid;place-items:center;padding:16px;box-sizing:border-box;"
    "background:#eef2f0;color:#14211d;font:16px/1.5 system-ui,-apple-system,'Segoe UI',Roboto,sans-serif}"
    "@media(prefers-color-scheme:dark){body{background:#0d1412;color:#e2ebe7}}"
    "div{max-width:460px;text-align:center}h2{margin:0 0 8px}p{opacity:.85}code{font-size:.92em}"
    "button{font:inherit;padding:10px 16px;margin:6px;border-radius:8px;cursor:pointer;"
    "border:1px solid #0b7a5f;background:transparent;color:inherit}.p{background:#0b7a5f;color:#fff;font-weight:600}"
    "form{display:inline}.dev{margin-top:18px;font-size:12.5px;opacity:.65}.dev a{color:inherit}</style>";

static const char SIMPLE_END[] PROGMEM =
    "<p class='dev'>LoRaWAN Node · Desenvolvido por <a href='https://k8s.adailsilva.com.br/' target='_blank' rel='noopener'><b>AdailSilva</b></a></p></div></body></html>";

/* ======================================================================= */
/*                              main page                                  */
/* ======================================================================= */
static const char PAGE_MAIN[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LoRaWAN Node · Configuração</title>
<link rel="icon" type="image/svg+xml" href="/favicon.svg">
<style>
:root{--bg:#f2f4f6;--sf:#fff;--sf2:#f7f9fa;--fg:#17202a;--mut:#5d6b78;--line:#dde3e8;--acc:#0b7a5f;--acc2:#e1f1eb;--on:#fff;--ok:#1f9d55;--warn:#c27803;--err:#c0362c;--errbg:#fbecea;--hd:#0e2a24;--mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;color-scheme:light}
@media(prefers-color-scheme:dark){:root{--bg:#0f151b;--sf:#151d25;--sf2:#1a232c;--fg:#e3e9ee;--mut:#8b98a5;--line:#26313c;--acc:#35b98f;--acc2:#16302a;--on:#07211a;--ok:#3fbf7f;--warn:#e3a33b;--err:#f07a6e;--errbg:#3a1f1d;--hd:#0b1116;color-scheme:dark}}
*{box-sizing:border-box}
html{scroll-behavior:smooth;scroll-padding-top:76px}
@media(prefers-reduced-motion:reduce){html{scroll-behavior:auto}*{transition:none!important}}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,Ubuntu,sans-serif;-webkit-font-smoothing:antialiased}
a{color:var(--acc)}
.m,code{font-family:var(--mono);font-size:.92em}
.top{position:sticky;top:0;z-index:5;background:var(--hd);color:#e8f1ee;border-bottom:1px solid rgba(255,255,255,.06)}
.top>div{display:flex;align-items:center;gap:12px 20px;flex-wrap:wrap;padding:10px 24px;max-width:1440px;margin:0 auto}
.brand{display:flex;align-items:center;gap:10px;margin-right:auto;min-width:0}
.brand img{width:28px;height:28px}
.brand h1{font-size:16px;font-weight:650;margin:0;white-space:nowrap;letter-spacing:.1px}
.brand h1 small{font-weight:500;opacity:.7;margin-left:6px;font-size:12.5px}
.chips{display:flex;gap:8px;flex-wrap:wrap}
.chip{display:inline-flex;align-items:center;gap:7px;padding:4px 10px;border-radius:999px;background:rgba(255,255,255,.07);font-size:12.5px;white-space:nowrap}
.chip b{font-weight:600;color:#fff}
.chip i{width:8px;height:8px;border-radius:50%;background:#7b8794}
.chip i.ok{background:#3fbf7f;box-shadow:0 0 0 3px rgba(63,191,127,.22)}.chip i.warn{background:#e3a33b}
.top nav{display:flex;gap:4px}
.top nav a{color:#e8f1ee;text-decoration:none;font-size:13.5px;padding:6px 10px;border-radius:7px}
.top nav a:hover{background:rgba(255,255,255,.08)}
.wrap{max-width:1440px;margin:0 auto;padding:0 24px;display:grid;grid-template-columns:210px minmax(0,1fr);gap:40px}
.side{position:sticky;top:76px;align-self:start;padding-top:28px}
.side a{display:flex;align-items:center;gap:10px;padding:8px 12px;border-radius:8px;color:var(--mut);text-decoration:none;font-size:14px;border-left:2px solid transparent}
.side a:hover{color:var(--fg);background:var(--sf)}
.side a.on{color:var(--fg);background:var(--sf);border-left-color:var(--acc);font-weight:600}
.side svg{width:16px;height:16px;flex:none;stroke:currentColor;fill:none;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round}
main{padding:28px 0 0;min-width:0}
.intro{margin-bottom:18px}
.intro h2{margin:0;font-size:24px;font-weight:650;letter-spacing:-.2px}
.intro p{margin:4px 0 0;color:var(--mut);font-size:14px}
form{display:flex;flex-direction:column;gap:16px}
#err{display:none;background:var(--errbg);color:var(--err);border:1px solid color-mix(in srgb,var(--err) 35%,transparent);border-radius:10px;padding:12px 14px;white-space:pre-line;font-size:14px}
.sec{background:var(--sf);border:1px solid var(--line);border-radius:12px;display:grid;grid-template-columns:260px minmax(0,1fr);min-width:0}
.sec>header{padding:22px 24px;border-right:1px solid var(--line)}
.sec>header h3{margin:0 0 6px;font-size:16px;font-weight:650}
.sec>header p{margin:0;color:var(--mut);font-size:13.5px;line-height:1.45}
.tag{display:inline-block;margin-top:10px;font-size:11.5px;font-weight:600;padding:2px 9px;border-radius:999px;border:1px solid var(--line);color:var(--mut)}
.tag.now{color:var(--ok);border-color:color-mix(in srgb,var(--ok) 45%,transparent)}
.tag.rb{color:var(--warn);border-color:color-mix(in srgb,var(--warn) 45%,transparent)}
.fl{padding:20px 24px 24px;min-width:0;max-width:820px}
.f{margin-top:16px}.f:first-child{margin-top:0}
label.l,.lb{display:block;font-size:13px;font-weight:550;color:var(--fg);margin:0 0 6px}
input[type=text],input[type=password],select{width:100%;height:40px;padding:0 12px;border:1px solid var(--line);border-radius:8px;background:var(--sf2);color:var(--fg);font:inherit;transition:border-color .15s,box-shadow .15s}
select{padding:0 8px;cursor:pointer}
input.m,select.m{font-family:var(--mono);font-size:14px}
input[type=text]:hover,input[type=password]:hover,select:hover{border-color:color-mix(in srgb,var(--mut) 45%,var(--line))}
input:focus,select:focus{outline:none;border-color:var(--acc);box-shadow:0 0 0 3px color-mix(in srgb,var(--acc) 22%,transparent)}
input::placeholder{color:color-mix(in srgb,var(--mut) 75%,transparent)}
input.bad{border-color:var(--err)}
button:focus-visible,summary:focus-visible,a:focus-visible,.seg input:focus-visible+span,.opt input:focus-visible{outline:2px solid var(--acc);outline-offset:2px}
.g2{display:grid;grid-template-columns:1fr 1fr;gap:16px}
.g3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:16px}
.g4{display:grid;grid-template-columns:repeat(4,1fr);gap:16px}
.g21{display:grid;grid-template-columns:2fr 1fr;gap:16px}
.g2>.f,.g3>.f,.g4>.f,.g21>.f{margin-top:0}.g2,.g3,.g4,.g21{margin-top:16px}.g2:first-child,.g3:first-child,.g4:first-child,.g21:first-child{margin-top:0}
#ipb{margin-top:16px}
.hint{font-size:12.5px;color:var(--mut);margin:6px 0 0;line-height:1.45}
.hint b{color:var(--fg);font-weight:600}
hr{border:0;border-top:1px solid var(--line);margin:22px 0 18px}
.seg{display:inline-flex;padding:3px;gap:3px;border-radius:9px;background:var(--sf2);border:1px solid var(--line);max-width:100%;flex-wrap:wrap}
.seg label{position:relative;cursor:pointer}
.seg input{position:absolute;opacity:0;inset:0;margin:0;cursor:pointer}
.seg span{display:block;padding:6px 14px;border-radius:7px;font-size:13.5px;font-weight:550;color:var(--mut);white-space:nowrap;transition:background .15s,color .15s}
.seg input:checked+span{background:var(--sf);color:var(--fg);box-shadow:0 1px 2px rgba(0,0,0,.12),0 0 0 1px var(--line)}
.seg label:hover span{color:var(--fg)}
.opts{display:grid;gap:8px}
.opts.c2{grid-template-columns:1fr 1fr}.opts.c3{grid-template-columns:1fr 1fr 1fr}
.opt{display:flex;gap:11px;align-items:flex-start;border:1px solid var(--line);border-radius:10px;padding:11px 13px;cursor:pointer;background:var(--sf);transition:border-color .15s,background .15s}
.opt:hover{border-color:color-mix(in srgb,var(--acc) 45%,var(--line))}
.opt:has(input:checked){border-color:var(--acc);background:var(--acc2)}
.opt input{margin:3px 0 0;accent-color:var(--acc);flex:none}
.opt div{flex:1;min-width:0}
.opt b{font-weight:600;font-size:14.5px}
.opt small{display:block;color:var(--mut);font-size:12.5px;margin-top:1px}
details{margin-top:14px}
summary{cursor:pointer;color:var(--mut);font-size:13.5px;width:max-content}
summary:hover{color:var(--fg)}
details>.f:first-of-type,details>.g2:first-of-type{margin-top:12px}
.pw{display:flex;gap:8px}
.pw button{flex:none}
button{font:inherit;font-size:14px;font-weight:550;border-radius:8px;height:40px;padding:0 16px;cursor:pointer;border:1px solid var(--line);background:var(--sf);color:var(--fg);white-space:nowrap}
button:hover{border-color:color-mix(in srgb,var(--mut) 50%,var(--line))}
button.s{height:34px;padding:0 12px;font-size:13px}
button:disabled{opacity:.5;cursor:default}
.pri{background:var(--acc);border-color:var(--acc);color:var(--on);min-width:150px}
.pri:hover{filter:brightness(1.07);border-color:var(--acc)}
.dng{color:var(--err);border-color:color-mix(in srgb,var(--err) 45%,var(--line))}
.st{padding:10px 12px;border-radius:9px;background:var(--sf2);font-size:13.5px;border:1px solid var(--line);border-left:3px solid var(--mut);margin:0 0 18px;overflow-wrap:anywhere}
.st.ok{border-left-color:var(--ok)}.st.warn{border-left-color:var(--warn)}.st.err{border-left-color:var(--err)}
#sts{margin-bottom:22px}
.kv{display:grid;grid-template-columns:repeat(auto-fill,minmax(170px,1fr));gap:10px}
.kv>div{border:1px solid var(--line);border-radius:10px;padding:10px 12px;background:var(--sf2);min-width:0}
.kv span{display:block;font-size:12px;color:var(--mut)}
.kv b{display:block;font:600 19px/1.35 var(--mono);font-variant-numeric:tabular-nums;margin:2px 0 1px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.kv small{display:block;font-size:12px;color:var(--mut);line-height:1.35;overflow-wrap:anywhere}
.acts{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px;align-items:center}
.acts .msg{font-size:13px;color:var(--mut)}
.band{margin-top:14px;padding:12px 14px 8px;border:1px solid var(--line);border-radius:10px;background:var(--sf2);color:var(--mut)}
.band svg{display:block;width:100%;height:46px}
.band .ax{display:flex;justify-content:space-between;font:11.5px var(--mono);margin-top:2px}
.band p{margin:0 0 8px;font-size:12.5px}.band p b{color:var(--fg);font-weight:600}
.calc{margin-top:14px;padding:10px 12px;border:1px dashed var(--line);border-radius:10px;font-size:13px;color:var(--mut)}
.calc b{color:var(--fg);font-family:var(--mono);font-weight:600}
.bar{position:sticky;bottom:0;z-index:4;margin:8px -24px 0;padding:14px 24px;display:flex;gap:12px;align-items:center;justify-content:flex-end;background:color-mix(in srgb,var(--bg) 88%,transparent);backdrop-filter:blur(8px);border-top:1px solid var(--line)}
.dirty{margin-right:auto;font-size:13.5px;color:var(--mut);display:flex;align-items:center;gap:8px}
.dirty i{width:8px;height:8px;border-radius:50%;background:var(--line)}
.bar.chg .dirty{color:var(--fg)}.bar.chg .dirty i{background:var(--warn)}
footer{max-width:1440px;margin:0 auto;padding:18px 24px 28px;text-align:center;color:var(--mut);font-size:12.5px}footer b{color:var(--fg);font-weight:600}footer a{color:inherit;text-decoration:none}footer a:hover b{text-decoration:underline}
[hidden]{display:none!important}
@media(max-width:1180px){.sec{grid-template-columns:1fr}.sec>header{border-right:0;border-bottom:1px solid var(--line);padding:16px 20px}.fl{padding:18px 20px 20px;max-width:none}.g4{grid-template-columns:1fr 1fr}}
@media(max-width:900px){.wrap{grid-template-columns:1fr;gap:0;padding:0 14px}.side{position:static;padding:14px 0 0;display:flex;flex-wrap:wrap;gap:6px}.side a{white-space:nowrap;background:var(--sf);border:1px solid var(--line);padding:6px 11px;font-size:13px}.side a.on{border-color:var(--acc)}.side svg{display:none}
main{padding-top:16px}.top{position:static}.top>div{padding:10px 14px}.chips{order:3;width:100%}.bar{margin:8px -14px 0;padding:12px 14px}.intro h2{font-size:20px}html{scroll-padding-top:12px}}
@media(max-width:560px){.kv{grid-template-columns:1fr 1fr}.kv b{font-size:16px}.g2,.g3,.g21,.opts.c2,.opts.c3{grid-template-columns:1fr}.dirty span{display:none}.bar button{flex:1;padding:0 10px}.pri{min-width:0}.brand{flex:1;margin:0}.brand h1{font-size:15px;white-space:normal;line-height:1.25}.chip{font-size:12px;padding:3px 9px}.side a{padding:5px 10px;font-size:12.5px}input.m{font-size:13px}.top>div{gap:10px 12px}.top nav a{padding:6px 8px}}
</style>
</head>
<body>
<div class="top"><div>
<div class="brand"><img src="/favicon.svg" alt=""><h1><span id="hn">LoRaWAN Node</span><small id="hm"></small></h1></div>
<div class="chips"><span class="chip" title="Estado da sessão LoRaWAN"><i id="cd"></i><span id="cl">LoRaWAN…</span></span><span class="chip">IP <b class="m" id="cip">-</b></span><span class="chip"><span id="cidl">DevAddr</span> <b class="m" id="cid">-</b></span><span class="chip">Plano <b id="cpl">-</b></span></div>
<nav><a href="logs">Logs</a><a href="saiu" onclick="return sair()" title="Encerrar o acesso a esta página">Sair</a></nav>
</div></div>

<div class="wrap">
<aside class="side" id="side">
<a href="#sts"><svg viewBox="0 0 24 24"><path d="M3 12h4l3-8 4 16 3-8h4"/></svg>Status</a>
<a href="#act"><svg viewBox="0 0 24 24"><rect x="4" y="10" width="16" height="11" rx="2"/><path d="M8 10V7a4 4 0 0 1 8 0v3"/></svg>Ativação <span data-m="abp">ABP</span><span data-m="otaa">OTAA</span></a>
<a href="#radio"><svg viewBox="0 0 24 24"><path d="M12 13v8M8 21h8"/><circle cx="12" cy="10" r="2"/><path d="M7.5 5.5a6.5 6.5 0 0 0 0 9M16.5 5.5a6.5 6.5 0 0 1 0 9"/></svg>Rádio LoRa</a>
<a href="#upl"><svg viewBox="0 0 24 24"><path d="M12 20V5M6 11l6-6 6 6"/></svg>Uplink</a>
<a href="#rst"><svg viewBox="0 0 24 24"><path d="M3 12a9 9 0 1 0 3-6.7L3 8"/><path d="M3 3v5h5"/></svg>Reset remoto</a>
<a href="#wifi"><svg viewBox="0 0 24 24"><path d="M2 8.5a15 15 0 0 1 20 0M5 12a10 10 0 0 1 14 0M8.5 15.5a5 5 0 0 1 7 0"/><circle cx="12" cy="19" r="1"/></svg>Rede Wi-Fi</a>
<a href="#boot"><svg viewBox="0 0 24 24"><path d="M12 3v8"/><path d="M6.3 7.3a8 8 0 1 0 11.4 0"/></svg>Inicialização</a>
<a href="#hw"><svg viewBox="0 0 24 24"><rect x="6" y="6" width="12" height="12" rx="2"/><path d="M9 2v4M15 2v4M9 18v4M15 18v4M2 9h4M2 15h4M18 9h4M18 15h4"/></svg>Hardware</a>
<a href="#acc"><svg viewBox="0 0 24 24"><circle cx="12" cy="8" r="4"/><path d="M4 21a8 8 0 0 1 16 0"/></svg>Acesso</a>
<a href="#mnt"><svg viewBox="0 0 24 24"><path d="M14.7 6.3a4 4 0 0 0-5.4 5.4L3 18l3 3 6.3-6.3a4 4 0 0 0 5.4-5.4l-2.5 2.5-2.4-.6-.6-2.4z"/></svg>Manutenção</a>
</aside>

<main>
<section class="sec" id="sts"><header><h3>Status</h3><p>Funcionamento do nó desde que ligou. Atualiza a cada 5 s enquanto esta aba estiver visível.</p></header>
<div class="fl">
<div id="gst" class="st">Carregando status…</div>
<div id="rbw" class="st warn" hidden></div>
<div class="kv">
<div><span>Ligado há</span><b id="kv_up">-</b><small id="ks_up"></small></div>
<div><span>Memória livre</span><b id="kv_mem">-</b><small id="ks_mem"></small></div>
<div><span>Sinal Wi-Fi</span><b id="kv_rssi">-</b><small id="ks_rssi"></small></div>
<div><span>Sessão</span><b id="kv_ses">-</b><small id="ks_ses"></small></div>
<div><span>Frame counter (FCnt)</span><b id="kv_fc">-</b><small id="ks_fc"></small></div>
<div><span>Uplinks enviados</span><b id="kv_upl">-</b><small id="ks_upl"></small></div>
<div><span>Downlinks recebidos</span><b id="kv_dn">-</b><small id="ks_dn"></small></div>
<div><span>Próximo envio</span><b id="kv_nx">-</b><small id="ks_nx"></small></div>
</div>
<div class="acts">
<button type="button" class="s" onclick="act('send')" title="Agenda um uplink agora (respeitando o duty cycle do LMiC)">Enviar agora</button>
<button type="button" class="s" data-m="otaa" onclick="act('restart','Refazer o JOIN agora? O nó deixa de transmitir até o servidor responder.')">Refazer JOIN</button>
<button type="button" class="s" data-m="abp" onclick="act('restart')" title="Reaplica DevAddr e chaves sem reiniciar o ESP32 (o FCnt é mantido)">Reprovisionar sessão</button>
<button type="button" class="s" data-m="abp" onclick="fcnt()" title="Define o próximo FCnt de uplink (também gravado na NVS)">Alterar FCnt…</button>
<span class="msg" id="amsg"></span>
</div>
</div></section>

<div class="intro"><h2>Configuração</h2><p>LoRaWAN, rádio, uplink e reset remoto valem assim que você salva. Wi-Fi, inicialização e hardware valem depois de reiniciar.</p></div>
<form name="config_form" action="/save" method="post" onsubmit="return chk()">
<div id="err" role="alert"></div>

<section class="sec" id="act"><header><h3>Ativação <span data-m="abp">ABP</span><span data-m="otaa">OTAA</span></h3>
<p data-m="abp">Sessão pré-provisionada: sem JOIN, o nó transmite logo ao ligar. Copie os valores de ChirpStack → Devices → Activation.</p>
<p data-m="otaa">O nó faz o JOIN e recebe as chaves de sessão do servidor. Cadastre no ChirpStack os mesmos DevEUI, JoinEUI e AppKey.</p>
<span class="tag now">aplica na hora</span></header>
<div class="fl">
<div data-m="otaa">
<div class="f"><label class="l" for="deveui">DevEUI (8 bytes, MSB)</label>
<div class="pw"><input type="text" id="deveui" class="m k" autocomplete="off" spellcheck="false" maxlength="23" name="dev_eui"><button type="button" class="s" onclick="rev('deveui')" title="Inverte a ordem dos bytes (LSB ↔ MSB)">Inverter</button><button type="button" class="s" onclick="macEui()" title="Gera um DevEUI único a partir do MAC deste ESP32">Do MAC</button></div>
<p class="hint">Digite como o ChirpStack mostra (MSB). Abaixo aparece o valor que o servidor vai receber.</p></div>
<div class="f"><label class="l" for="joineui">JoinEUI / AppEUI (8 bytes, MSB)</label>
<div class="pw"><input type="text" id="joineui" class="m k" autocomplete="off" spellcheck="false" maxlength="23" name="join_eui"><button type="button" class="s" onclick="rev('joineui')">Inverter</button></div></div>
<div class="f"><label class="l" for="appkey">AppKey (16 bytes, MSB)</label><input type="text" id="appkey" class="m k" autocomplete="off" spellcheck="false" maxlength="47" name="app_key">
<p class="hint">Mudar DevEUI, JoinEUI, AppKey ou os canais refaz o JOIN ao salvar.</p></div>
<div class="f"><span class="lb">Ordem dos bytes do DevEUI/JoinEUI no ar</span>
<div class="seg" role="radiogroup" aria-label="Ordem dos bytes"><label><input type="radio" name="eui_rev" value="off" data-h="Como no firmware original: os bytes vão ao LMiC na ordem digitada. O servidor recebe o EUI invertido."><span>Como digitado</span></label><label><input type="radio" name="eui_rev" value="on" data-h="Padrão MCCI LMiC (LSB first): o servidor recebe exatamente o EUI digitado acima."><span>Invertida (LSB)</span></label></div>
<p class="hint" data-h="eui_rev"></p>
<p class="hint" id="euih"></p></div>
</div>
<div data-m="abp">
<div class="g2">
<div class="f"><label class="l" for="devaddr">DevAddr (4 bytes)</label><input type="text" id="devaddr" class="m k" autocomplete="off" spellcheck="false" maxlength="11" name="dev_addr"></div>
<div class="f"><label class="l" for="netid">NetID</label><input type="text" id="netid" class="m" autocomplete="off" spellcheck="false" maxlength="8" name="net_id" list="netids"><datalist id="netids"><option value="000000">ChirpStack</option><option value="000013">The Things Network</option></datalist>
<p class="hint"><code>000000</code> ChirpStack · <code>000013</code> TTN</p></div>
</div>
<div class="f"><label class="l" for="nwkskey">NwkSKey — Network Session Key (16 bytes, MSB)</label><input type="text" id="nwkskey" class="m k" autocomplete="off" spellcheck="false" maxlength="47" name="nwk_skey"></div>
<div class="f"><label class="l" for="appskey">AppSKey — Application Session Key (16 bytes, MSB)</label><input type="text" id="appskey" class="m k" autocomplete="off" spellcheck="false" maxlength="47" name="app_skey"></div>
<div class="g2">
<div class="f"><label class="l" for="rx1d">Atraso da janela RX1 (s)</label><input type="text" id="rx1d" class="m" inputmode="numeric" name="rx1_delay"><p class="hint">Igual ao <i>RX1 delay</i> do perfil no ChirpStack (padrão 1 s).</p></div>
<div class="f"><label class="l" for="deveui2">DevEUI (referência)</label><input type="text" id="deveui2" class="m k" autocomplete="off" spellcheck="false" maxlength="23" name="dev_eui"><p class="hint">Não é usado no ABP; fica guardado só para o cadastro.</p></div>
</div>
<p class="hint">Mudar o DevAddr zera o FCnt (dispositivo novo). Mudar só as chaves mantém o FCnt.</p>
</div>
</div></section>

<section class="sec" id="radio"><header><h3>Rádio LoRa</h3><p>Canais de uplink, data rate e potência. Use os mesmos canais que o gateway escuta.</p><span class="tag now">aplica na hora</span></header>
<div class="fl">
<div class="g21">
<div class="f"><label class="l" for="sb">Sub-banda (canais de 125 kHz) · no Brasil, SB1</label><select id="sb" name="sub_band" class="m"></select></div>
<div class="f"><span class="lb">Canal de 500 kHz</span>
<div class="seg" role="radiogroup" aria-label="Canal de 500 kHz"><label><input type="radio" name="ch500" value="on"><span>Ligado</span></label><label><input type="radio" name="ch500" value="off"><span>Desligado</span></label></div></div>
</div>
<details id="chd"><summary>Canais personalizados</summary>
<div class="f"><label class="l" for="chl">Lista de canais (0–71)</label><input type="text" id="chl" class="m" autocomplete="off" spellcheck="false" placeholder="ex.: 8-15,65" name="ch_list"><p class="hint">Preenchido, substitui a sub-banda. Vazio usa a sub-banda escolhida acima.</p></div>
</details>
<div class="band" id="band"></div>
<hr>
<div class="g2">
<div class="f"><label class="l" for="dr">Data rate de uplink</label><select id="dr" name="dr" class="m"></select></div>
<div class="f"><label class="l" for="txp">Potência de transmissão (dBm)</label><select id="txp" name="tx_pow" class="m"></select></div>
</div>
<div class="g2">
<div class="f"><span class="lb">ADR (Adaptive Data Rate)</span>
<div class="seg" role="radiogroup" aria-label="ADR"><label><input type="radio" name="adr" value="off" data-h="Data rate e potência fixos (os escolhidos acima)."><span>Desligado</span></label><label><input type="radio" name="adr" value="on" data-h="O servidor ajusta data rate e potência. Os valores acima são só o ponto de partida."><span>Ligado</span></label></div>
<p class="hint" data-h="adr"></p></div>
<div class="f"><span class="lb">Link check</span>
<div class="seg" role="radiogroup" aria-label="Link check"><label><input type="radio" name="link_chk" value="off" data-h="Recomendado: não reduz o data rate sozinho."><span>Desligado</span></label><label><input type="radio" name="link_chk" value="on" data-h="Pede confirmação de link ao servidor; pode baixar o data rate e o tamanho do payload."><span>Ligado</span></label></div>
<p class="hint" data-h="link_chk"></p></div>
</div>
<details><summary>Janela RX2 e relógio</summary>
<div class="g3">
<div class="f"><label class="l" for="rx2dr">Data rate RX2</label><select id="rx2dr" name="rx2_dr" class="m"></select></div>
<div class="f"><label class="l" for="rx2f">Frequência RX2 (Hz)</label><input type="text" id="rx2f" class="m" inputmode="numeric" placeholder="923300000" name="rx2_freq"></div>
<div class="f"><label class="l" for="clk">Erro de relógio</label><select id="clk" name="clk_err" class="m"></select></div>
</div>
<p class="hint">Padrão AU915/ChirpStack: RX2 em 923,3 MHz com DR8 (SF12/BW500). Frequência vazia usa o padrão da região. O erro de relógio alarga a janela de recepção; aumente se o nó perder downlinks.</p>
</details>
</div></section>

<section class="sec" id="upl"><header><h3>Uplink</h3><p>O que o nó envia e com que frequência.</p><span class="tag now">aplica na hora</span></header>
<div class="fl">
<div class="g3">
<div class="f"><label class="l" for="txi">Intervalo (s)</label><input type="text" id="txi" class="m" inputmode="numeric" name="tx_int"></div>
<div class="f"><label class="l" for="fp">FPort</label><input type="text" id="fp" class="m" inputmode="numeric" name="fport"></div>
<div class="f"><span class="lb">Confirmação (ACK)</span>
<div class="seg" role="radiogroup" aria-label="Confirmação"><label><input type="radio" name="confirmed" value="off"><span>Não</span></label><label><input type="radio" name="confirmed" value="on"><span>Sim</span></label></div></div>
</div>
<div class="f" style="margin-top:16px"><span class="lb">Payload</span>
<div class="seg" role="radiogroup" aria-label="Formato do payload"><label><input type="radio" name="pl_fmt" value="text"><span>Texto</span></label><label><input type="radio" name="pl_fmt" value="hex"><span>Hexadecimal</span></label></div>
<input type="text" id="pl" class="m" autocomplete="off" spellcheck="false" name="payload" style="margin-top:10px">
<p class="hint" id="plh"></p></div>
<div class="calc" id="calc"></div>
</div></section>

<section class="sec" id="rst"><header><h3>Reset remoto</h3><p>Reinicia o ESP32 quando chega um downlink com exatamente este conteúdo nesta FPort.</p><span class="tag now">aplica na hora</span></header>
<div class="fl">
<span class="lb">Comando de reset por downlink</span>
<div class="seg" role="radiogroup" aria-label="Reset remoto"><label><input type="radio" name="rst_en" value="on"><span>Ativado</span></label><label><input type="radio" name="rst_en" value="off"><span>Desativado</span></label></div>
<div class="g3">
<div class="f"><label class="l" for="rp">FPort</label><input type="text" id="rp" class="m" inputmode="numeric" name="rst_port"></div>
<div class="f"><label class="l" for="rd">Espera antes de reiniciar (ms)</label><input type="text" id="rd" class="m" inputmode="numeric" name="rst_delay"></div>
</div>
<div class="f" style="margin-top:16px"><label class="l" for="rc">Comando (1 a 16 bytes em hexadecimal)</label><input type="text" id="rc" class="m" autocomplete="off" spellcheck="false" name="rst_cmd">
<p class="hint" id="rch"></p></div>
</div></section>

<section class="sec" id="wifi"><header><h3>Rede Wi-Fi</h3><p>Rede em que o nó se conecta para esta página e como ele recebe o endereço IP.</p><span class="tag rb">vale após reiniciar</span></header>
<div class="fl">
<div class="f"><label class="l" for="nm">Nome do nó</label><input type="text" id="nm" autocomplete="off" maxlength="31" name="name"><p class="hint" id="nmh"></p></div>
<hr>
<div class="f"><label class="l" for="ssid">Nome da rede (SSID)</label><input type="text" id="ssid" autocomplete="off" maxlength="32" name="wifi_ssid"></div>
<div class="f"><label class="l" for="pw">Senha</label>
<div class="pw"><input type="password" id="pw" autocomplete="new-password" maxlength="64" name="wifi_pswd"><button type="button" onclick="tp(this,'pw')">Mostrar</button></div></div>
<hr>
<span class="lb">Endereço IP</span>
<div class="seg" role="radiogroup" aria-label="Endereço IP">
<label><input type="radio" name="ip_mode" value="dhcp" data-h="O roteador entrega IP, gateway e DNS."><span>Automático (DHCP)</span></label>
<label><input type="radio" name="ip_mode" value="static" data-h="Endereço, gateway e DNS definidos abaixo. Use um IP fora da faixa do DHCP do roteador."><span>IP fixo</span></label>
</div>
<p class="hint" data-h="ip_mode"></p>
<div id="ipb">
<div class="g2">
<div class="f"><label class="l" for="ia">Endereço IP</label><input type="text" id="ia" class="m" inputmode="decimal" autocomplete="off" name="ip_addr"></div>
<div class="f"><label class="l" for="im">Máscara de rede</label><input type="text" id="im" class="m" inputmode="decimal" autocomplete="off" placeholder="255.255.255.0" name="ip_mask"></div>
</div>
<div class="f"><label class="l" for="ig">Gateway (roteador)</label><input type="text" id="ig" class="m" inputmode="decimal" autocomplete="off" name="ip_gw"></div>
<div class="g2">
<div class="f"><label class="l" for="d1">DNS primário</label><input type="text" id="d1" class="m" inputmode="decimal" autocomplete="off" name="ip_dns1"></div>
<div class="f"><label class="l" for="d2">DNS secundário (opcional)</label><input type="text" id="d2" class="m" inputmode="decimal" autocomplete="off" placeholder="8.8.8.8" name="ip_dns2"></div>
</div>
<p class="hint">DNS primário vazio usa o gateway. Depois de reiniciar, abra a página no novo IP.</p>
</div>
</div></section>

<section class="sec" id="boot"><header><h3>Inicialização</h3><p>Como o Wi-Fi sobe quando o nó liga, e o Soft AP de configuração.</p><span class="tag rb">vale após reiniciar</span></header>
<div class="fl">
<div class="opts c3">
<label class="opt"><input type="radio" name="wifi_mode" value="sta"><div><b>Station</b><small>Conecta na rede acima. Sem conexão, abre o Soft AP de emergência.</small></div></label>
<label class="opt"><input type="radio" name="wifi_mode" value="ap"><div><b>Soft AP</b><small>Cria a própria rede; página em 192.168.101.101.</small></div></label>
<label class="opt"><input type="radio" name="wifi_mode" value="off"><div><b>Desligado</b><small>Economiza energia. Para configurar, segure o botão do Soft AP por 3 s.</small></div></label>
</div>
<hr>
<div class="g2">
<div class="f"><label class="l" for="aps">Nome do Soft AP</label><input type="text" id="aps" autocomplete="off" maxlength="32" name="ap_ssid"></div>
<div class="f"><label class="l" for="app">Senha do Soft AP</label>
<div class="pw"><input type="password" id="app" autocomplete="new-password" maxlength="64" name="ap_pswd"><button type="button" onclick="tp(this,'app')">Mostrar</button></div></div>
</div>
<div class="g3">
<div class="f"><label class="l" for="apc">Canal do Soft AP</label><select id="apc" name="ap_ch" class="m"></select></div>
<div class="f"><label class="l" for="sto">Espera pelo Wi-Fi (s)</label><input type="text" id="sto" class="m" inputmode="numeric" name="sta_to"></div>
<div class="f"><label class="l" for="bp">Botão do Soft AP (GPIO)</label><input type="text" id="bp" class="m" inputmode="numeric" name="boot_pin"></div>
</div>
<p class="hint">Com o nó ligado, segure o botão (nível baixo) por 3 s para abrir o Soft AP, mesmo com o Wi-Fi desligado. GPIO 0 é o botão BOOT das placas DevKit (não segure ao ligar: isso entra no modo de gravação). <code>-1</code> desativa. No console serial, <code>wifi_ap</code> faz o mesmo.</p>
</div></section>

<section class="sec" id="hw"><header><h3>Hardware</h3><p>Ligação ESP32 ↔ RFM95W (SX1276) e console serial.</p><span class="tag rb">vale após reiniciar</span></header>
<div class="fl">
<div class="g4">
<div class="f"><label class="l" for="p_sclk">SCLK</label><input type="text" id="p_sclk" class="m" inputmode="numeric" name="pin_sclk"></div>
<div class="f"><label class="l" for="p_miso">MISO</label><input type="text" id="p_miso" class="m" inputmode="numeric" name="pin_miso"></div>
<div class="f"><label class="l" for="p_mosi">MOSI</label><input type="text" id="p_mosi" class="m" inputmode="numeric" name="pin_mosi"></div>
<div class="f"><label class="l" for="p_nss">NSS / CS</label><input type="text" id="p_nss" class="m" inputmode="numeric" name="pin_nss"></div>
</div>
<div class="g4">
<div class="f"><label class="l" for="p_rst">RESET</label><input type="text" id="p_rst" class="m" inputmode="numeric" name="pin_rst"></div>
<div class="f"><label class="l" for="p_dio0">DIO0</label><input type="text" id="p_dio0" class="m" inputmode="numeric" name="pin_dio0"></div>
<div class="f"><label class="l" for="p_dio1">DIO1</label><input type="text" id="p_dio1" class="m" inputmode="numeric" name="pin_dio1"></div>
<div class="f"><label class="l" for="p_dio2">DIO2</label><input type="text" id="p_dio2" class="m" inputmode="numeric" name="pin_dio2"></div>
</div>
<p class="hint"><code>-1</code> = não conectado (só RESET, DIO1 e DIO2). GPIO 34–39 do ESP32 são só entrada: servem para DIO, nunca para SCLK, MOSI, NSS ou RESET.</p>
<hr>
<div class="g3">
<div class="f"><label class="l" for="spi">Clock SPI (Hz)</label><input type="text" id="spi" class="m" inputmode="numeric" name="spi_freq"><p class="hint"><code>0</code> = padrão do LMiC.</p></div>
<div class="f"><label class="l" for="bd">Velocidade da serial</label><select id="bd" name="baud" class="m"></select></div>
<div class="f"><span class="lb">Diagnóstico de sessão</span>
<div class="seg" role="radiogroup" aria-label="Diagnóstico"><label><input type="radio" name="verbose" value="on"><span>Ligado</span></label><label><input type="radio" name="verbose" value="off"><span>Desligado</span></label></div>
<p class="hint">Chaves, frame e MIC a cada envio (aplica na hora).</p></div>
</div>
</div></section>

<section class="sec" id="acc"><header><h3>Acesso</h3><p>Usuário e senha pedidos ao abrir esta página.</p><span class="tag now">aplica na hora</span></header>
<div class="fl">
<div class="f"><label class="l" for="wu">Usuário</label><input type="text" id="wu" autocomplete="username" maxlength="31" name="web_user"></div>
<div class="g2">
<div class="f"><label class="l" for="wp">Nova senha</label><input type="password" id="wp" autocomplete="new-password" maxlength="31" placeholder="vazio mantém a atual" name="web_pswd"></div>
<div class="f"><label class="l" for="wp2">Confirmar nova senha</label><input type="password" id="wp2" autocomplete="new-password" maxlength="31"></div>
</div>
<p class="hint">Mínimo de 6 caracteres. Depois de salvar, o navegador pede o novo login. Esqueceu? No console serial: <code>web_auth --reset</code> (volta para admin / lorawan).</p>
</div></section>

<section class="sec" id="mnt"><header><h3>Manutenção</h3><p>Cópia de segurança da configuração e volta ao padrão de fábrica.</p></header>
<div class="fl">
<div class="acts" style="margin-top:0">
<button type="button" onclick="exp()">Exportar configuração</button>
<label class="s" style="display:inline-flex"><input type="file" id="impf" accept=".json,application/json" hidden onchange="imp(this)"><button type="button" onclick="$('impf').click()">Importar…</button></label>
<button type="button" class="dng" onclick="fac()">Restaurar padrão de fábrica</button>
</div>
<p class="hint">A exportação gera um JSON com tudo, menos as senhas (Wi-Fi, Soft AP e página). Importar preenche o formulário; revise e clique em <b>Salvar</b>. O padrão de fábrica usa os valores do <code>node_build.h</code> e mantém o FCnt.</p>
<p class="hint" id="fwv"></p>
</div></section>

<div class="bar" id="bar">
<div class="dirty"><i></i><span id="dt">Nenhuma alteração</span></div>
<button type="submit" form="rb">Reiniciar nó</button>
<button type="submit" class="pri">Salvar</button>
</div>
</form>
<form id="rb" action="/reboot" method="post" onsubmit="return confirm('Reiniciar o nó agora?')"></form>
<form id="fb" action="/factory" method="post"></form>
</main>
</div>
<footer><span id="fn">LoRaWAN Node</span>. Desenvolvido por <a href="https://k8s.adailsilva.com.br/" target="_blank" rel="noopener"><b>AdailSilva</b></a></footer>
<script>
var f=document.forms.config_form,C={},MODE="",dirty=0;
function $(i){return document.getElementById(i)}
function ck(n){return f.querySelector("input[name="+n+"]:checked")}
function rv(n){var c=ck(n);return c?c.value:""}
function sair(){if(!confirm("Sair da configuração do nó?"))return false;var x=new XMLHttpRequest();x.open("GET","logout",true,"_logout_","_logout_");x.onloadend=function(){location.replace("saiu")};x.send();return false}
function opt(s,list){$(s).innerHTML=list.map(function(o){return'<option value="'+o[0]+'">'+o[1]+"</option>"}).join("")}
/* AU915: [SF, BW kHz, max payload] for DR0..DR6, downlink DR8..DR13 */
var DR=[[12,125,51],[11,125,51],[10,125,51],[9,125,115],[8,125,222],[7,125,222],[8,500,222]];
var mhz=function(v){return v.toFixed(1).replace(".",",")};
opt("sb",[0,1,2,3,4,5,6,7].map(function(i){return[i,"SB"+i+" · canais "+(i*8)+"–"+(i*8+7)+" · "+mhz(915.2+1.6*i)+"–"+mhz(916.6+1.6*i)+" MHz"]}));
opt("dr",DR.map(function(d,i){return[i,"DR"+i+" — SF"+d[0]+" / BW"+d[1]+" · até "+d[2]+" bytes"]}));
opt("txp",[2,4,6,8,10,12,14,16,17,18,20].map(function(v){return[v,v+" dBm"+(v==14?" (padrão)":v==20?" (máx. RFM95W)":"")]}));
opt("rx2dr",[8,9,10,11,12,13].map(function(v){return[v,"DR"+v+" · SF"+(20-v)+"/BW500"]}));
opt("clk",[0,1,2,3,4,5,7,10].map(function(v){return[v,v+" %"+(v==1?" (padrão)":"")]}));
opt("apc",[1,2,3,4,5,6,7,8,9,10,11,12,13].map(function(v){return[v,"Canal "+v]}));
opt("bd",[9600,19200,38400,57600,115200,230400,460800,921600].map(function(v){return[v,v+" bps"]}));
/* ------------------------------ helpers ------------------------------ */
function hexb(s){s=s.replace(/0x/gi,"").replace(/[\s:,-]/g,"");if(!/^([0-9a-fA-F]{2})*$/.test(s))return null;var b=[];for(var i=0;i<s.length;i+=2)b.push(parseInt(s.substr(i,2),16));return b}
function hx(b,sep){return b.map(function(x){return("0"+x.toString(16)).slice(-2).toUpperCase()}).join(sep||"")}
function utf8(s){return unescape(encodeURIComponent(s)).length}
function plBytes(){var v=f.payload.value;if(rv("pl_fmt")=="hex"){var b=hexb(v);return b?b.length:-1}return utf8(v)}
function rev(id){var e=$(id),b=hexb(e.value);if(!b){alert("Valor hexadecimal inválido.");return}e.value=hx(b.reverse());mark()}
function macEui(){var m=(C._mac||"").split(":");if(m.length!=6)return;var e=$("deveui");e.value=(m.slice(0,3).join("")+"FFFE"+m.slice(3).join("")).toUpperCase();mark();upd()}
function tp(b,id){var p=$(id);p.type=p.type=="password"?"text":"password";b.textContent=p.type=="password"?"Mostrar":"Ocultar"}
function chList(s){var m={},n=0,ok=true;s.split(/[,;]/).forEach(function(p){p=p.trim();if(!p)return;var r=/^(\d+)\s*(?:-\s*(\d+))?$/.exec(p);if(!r){ok=false;return}var a=+r[1],b=r[2]?+r[2]:a;if(a>b||b>71){ok=false;return}for(var c=a;c<=b;c++)if(!m[c]){m[c]=1;n++}});return ok&&n?m:null}
function chans(){var l=f.ch_list.value.trim(),m;if(l)return chList(l);m={};var s=+f.sub_band.value;for(var i=0;i<8;i++)m[s*8+i]=1;if(rv("ch500")=="on")m[64+s]=1;return m}
/* LoRa time on air (ms), explicit header, CRC on, CR 4/5, preamble 8 */
function toa(dr,pl){var d=DR[dr],sf=d[0],bw=d[1],de=(bw==125&&sf>=11)?1:0,ts=Math.pow(2,sf)/(bw*1000),n=8+Math.max(Math.ceil((8*pl-4*sf+28+16)/(4*(sf-2*de)))*5,0);return(12.25+n)*ts*1000}
/* ---------------------------- band drawing --------------------------- */
function band(){var b=$("band"),m=chans(),i,on,n125=0,n500=0,s,x=function(v){return(v-915)/13*1000};
if(!m){b.innerHTML='<p>Lista de canais inválida.</p>';return}
s='<svg viewBox="0 0 1000 46" preserveAspectRatio="none" aria-hidden="true"><line x1="0" y1="40" x2="1000" y2="40" stroke="currentColor" stroke-opacity=".3"/>';
for(i=0;i<64;i++){on=!!m[i];if(on)n125++;s+='<rect x="'+(x(915.2+.2*i)-2.2).toFixed(1)+'" y="'+(on?8:26)+'" width="4.4" height="'+(on?32:14)+'" rx="1" fill="'+(on?"var(--acc)":"currentColor")+'" fill-opacity="'+(on?1:.25)+'"/>'}
for(i=0;i<8;i++){on=!!m[64+i];if(on)n500++;s+='<rect x="'+(x(915.9+1.6*i)-9).toFixed(1)+'" y="'+(on?4:30)+'" width="18" height="'+(on?36:10)+'" rx="2" fill="'+(on?"var(--warn)":"currentColor")+'" fill-opacity="'+(on?.9:.18)+'"/>'}
b.innerHTML='<p>Transmitindo em <b>'+n125+' canal(is) de 125 kHz</b>'+(n500?' e <b>'+n500+' de 500 kHz</b>':'')+' na faixa AU915.</p>'+s+'</svg><div class="ax"><span>915</span><span>918</span><span>921</span><span>924</span><span>928 MHz</span></div>'}
function hints(){f.querySelectorAll("p[data-h]").forEach(function(p){var c=ck(p.getAttribute("data-h"));p.textContent=c?c.getAttribute("data-h"):""})}
function host(n){return(n||"").toLowerCase().replace(/[^a-z0-9]+/g,"-").replace(/^-+|-+$/g,"").slice(0,31)||"lorawan-node"}
function upd(){
var hex=rv("pl_fmt")=="hex",n=plBytes(),dr=+f.dr.value,mx=DR[dr][2],t;
f.payload.placeholder=hex?"41 64 61 69 6C":"texto do payload";
$("plh").innerHTML=n<0?"Hexadecimal inválido.":"<b>"+n+" byte(s)</b> · máximo em DR"+dr+": "+mx+" bytes"+(n>mx?' — <span style="color:var(--err)">grande demais para este data rate</span>':"");
t=n>0?toa(dr,n+13):0;var iv=+f.tx_int.value||0;
$("calc").innerHTML=n>0?"Tempo no ar por uplink ≈ <b>"+t.toFixed(1)+" ms</b> (SF"+DR[dr][0]+"/BW"+DR[dr][1]+", "+(n+13)+" bytes PHY)"+(iv?" · ocupação ≈ <b>"+(100*t/(iv*1000)).toFixed(2)+" %</b> do tempo":"")+(rv("adr")=="on"?". Com ADR, o servidor pode mudar o data rate.":"."):"";
var rc=hexb(f.rst_cmd.value);$("rch").innerHTML=rc&&rc.length?"Para enfileirar no ChirpStack: FPort <b>"+(f.rst_port.value||"-")+"</b>, hex <code>"+hx(rc)+"</code>, base64 <code>"+btoa(String.fromCharCode.apply(null,rc))+"</code>":"Ex.: AD A1 DE AD BE EF 0D 0A";
var im=rv("ip_mode");$("ipb").style.display=im=="static"?"":"none";
f.ip_dns1.placeholder=f.ip_gw.value.trim()||"igual ao gateway";
var h=host(f.name.value);$("nmh").innerHTML="Endereço na rede: <code>http://"+h+".local/</code>. Também é o nome padrão do Soft AP.";
f.ap_ssid.placeholder=f.name.value||"";
$("hn").textContent=f.name.value||"LoRaWAN Node";$("fn").textContent=(f.name.value||"LoRaWAN Node")+" · "+(C._fw||"");document.title=(f.name.value||"LoRaWAN Node")+" · Configuração";
var sb=+f.sub_band.value;$("cpl").textContent=(C._region||"AU915")+(f.ch_list.value.trim()?" custom":" SB"+sb);
var de=hexb($("deveui").value),je=hexb($("joineui").value),rvs=rv("eui_rev")=="on";
$("euih").innerHTML=de&&de.length==8?"O ChirpStack vai receber DevEUI <b class=m>"+hx(rvs?de:de.slice().reverse())+"</b>"+(je&&je.length==8?" e JoinEUI <b class=m>"+hx(rvs?je:je.slice().reverse())+"</b>":"")+" — cadastre o dispositivo com esses valores.":"";
if(MODE=="otaa"){$("cidl").textContent="DevEUI";if(!$("cid").dataset.live)$("cid").textContent=$("deveui").value||"-"}
band();hints()}
function mark(){if(dirty)return;dirty=1;$("bar").classList.add("chg");$("dt").textContent="Alterações não salvas"}
/* ------------------------------ fill form ---------------------------- */
function fill(j,imp){Object.keys(j).forEach(function(k){if(k[0]=="_")return;var v=j[k],els=f.querySelectorAll('[name="'+k+'"]');
els.forEach(function(e){if(e.type=="radio")e.checked=(e.value==String(v));else if(e.type!="password")e.value=v==null?"":v})});
if(!imp){[["wifi_pswd","wifi_pswd_set"],["ap_pswd","ap_pswd_set"]].forEach(function(p){f[p[0]].value="";f[p[0]].placeholder=j[p[1]]?"Salva (vazio mantém)":""})}
if(f.ch_list.value.trim())$("chd").open=true;upd()}
function mode(m){MODE=m;document.querySelectorAll("[data-m]").forEach(function(e){var on=e.getAttribute("data-m")==m;e.hidden=!on;e.querySelectorAll("input,select").forEach(function(i){i.disabled=!on})});$("hm").textContent=m.toUpperCase();$("cidl").textContent=m=="otaa"?"DevEUI":"DevAddr"}
function load(){fetch("config.json",{cache:"no-store"}).then(function(r){return r.json()}).then(function(j){C=j;mode(j._mode);fill(j);
$("fwv").innerHTML="Firmware <b>"+j._fw+"</b> · "+j._lmic+" · região "+j._region+" · "+j._chip+" · MAC <code>"+j._mac+"</code>";
dirty=0;$("bar").classList.remove("chg");$("dt").textContent="Nenhuma alteração"})
.catch(function(){var e=$("err");e.textContent="Não foi possível ler a configuração do nó. Recarregue a página.";e.style.display="block"})}
/* ------------------------------ validation --------------------------- */
function ip4(s){var m=/^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/.exec(s);if(!m)return null;for(var i=1;i<5;i++)if(+m[i]>255)return null;return((m[1]<<24)|(m[2]<<16)|(m[3]<<8)|+m[4])>>>0}
function num(n,lo,hi,label,e,opt){var v=f[n].value.trim();f[n].value=v;if(!v&&opt)return;if(!/^-?\d+$/.test(v)||+v<lo||+v>hi)e.push(label+": número entre "+lo+" e "+hi+".")}
function key(n,len,label,e){var el=$(n);if(el.disabled)return;var b=hexb(el.value);if(!b||b.length!=len){e.push(label+": exatamente "+len+" bytes ("+len*2+" caracteres hexadecimais).");el.classList.add("bad")}else{el.value=hx(b);el.classList.remove("bad")}}
function chk(){var e=[],v;f.querySelectorAll(".bad").forEach(function(x){x.classList.remove("bad")});
v=f.name.value.trim();if(!v)e.push("Nome do nó: obrigatório.");
if(MODE=="otaa"){key("deveui",8,"DevEUI",e);key("joineui",8,"JoinEUI",e);key("appkey",16,"AppKey",e)}
else{key("devaddr",4,"DevAddr",e);key("nwkskey",16,"NwkSKey",e);key("appskey",16,"AppSKey",e);key("netid",3,"NetID",e);num("rx1_delay",1,15,"Atraso RX1",e);
if($("deveui2").value.trim())key("deveui2",8,"DevEUI (referência)",e)}
if(!chans())e.push("Canais personalizados: use números de 0 a 71, ex.: 8-15,65.");
num("rx2_freq",100000000,999999999,"Frequência RX2",e,1);
num("tx_int",5,86400,"Intervalo",e);num("fport",1,223,"FPort do uplink",e);
v=plBytes();if(v<1)e.push("Payload: "+(v<0?"hexadecimal inválido.":"não pode ficar vazio."));else if(v>222)e.push("Payload: máximo de 222 bytes.");
num("rst_port",1,223,"FPort do reset",e);num("rst_delay",0,60000,"Espera antes do reset",e);
v=hexb(f.rst_cmd.value);if(!v||v.length<1||v.length>16)e.push("Comando de reset: 1 a 16 bytes em hexadecimal.");
v=f.wifi_pswd.value;if(v&&v.length<8)e.push("Senha do Wi-Fi: mínimo de 8 caracteres.");
v=f.ap_pswd.value;if(v&&v.length<8)e.push("Senha do Soft AP: mínimo de 8 caracteres.");
if(rv("ip_mode")=="static"){var A={};if(!f.ip_mask.value.trim())f.ip_mask.value="255.255.255.0";
[["ip_addr","Endereço IP",1],["ip_mask","Máscara",1],["ip_gw","Gateway",1],["ip_dns1","DNS primário",0],["ip_dns2","DNS secundário",0]].forEach(function(x){v=f[x[0]].value.trim();f[x[0]].value=v;if(!v){if(x[2])e.push(x[1]+": obrigatório com IP fixo.");return}A[x[0]]=ip4(v);if(A[x[0]]===null)e.push(x[1]+": use o formato 192.168.1.60.")});
if(A.ip_mask!=null&&(!A.ip_mask||!/^1*0*$/.test(A.ip_mask.toString(2).padStart(32,"0"))))e.push("Máscara: inválida (ex.: 255.255.255.0).");
else if(A.ip_addr!=null&&A.ip_gw!=null&&A.ip_mask!=null){if(((A.ip_addr&A.ip_mask)>>>0)!==((A.ip_gw&A.ip_mask)>>>0))e.push("Gateway fora da rede do endereço IP (confira a máscara).");if(A.ip_addr===A.ip_gw)e.push("Endereço IP igual ao do gateway.")}}
if(rv("wifi_mode")=="sta"&&!f.wifi_ssid.value.trim())e.push("Modo Station: informe o nome da rede Wi-Fi (ou escolha Soft AP).");
num("sta_to",5,3600,"Espera pelo Wi-Fi",e);num("boot_pin",-1,48,"Botão de configuração",e);
var used={};[["pin_sclk","SCLK",0,1],["pin_miso","MISO",0,0],["pin_mosi","MOSI",0,1],["pin_nss","NSS",0,1],["pin_rst","RESET",1,1],["pin_dio0","DIO0",0,0],["pin_dio1","DIO1",1,0],["pin_dio2","DIO2",1,0]].forEach(function(p){
v=f[p[0]].value.trim();f[p[0]].value=v;if(!/^-?\d+$/.test(v)||+v<(p[2]?-1:0)||+v>48){e.push("Pino "+p[1]+": GPIO de "+(p[2]?"-1":"0")+" a 48.");return}v=+v;if(v<0)return;
if(used[v])e.push("Pino "+p[1]+": GPIO "+v+" já usado em "+used[v]+".");used[v]=p[1];
if(/^ESP32(-D0WD|-D0WDQ6|-D2WD|-S0WD|-U4WDH|-PICO)?/.test(C._chip||"")&&!/S2|S3|C3|C6|H2/.test(C._chip||"")){if(p[3]&&v>=34)e.push("Pino "+p[1]+": GPIO "+v+" é só entrada no ESP32.");if(v>=6&&v<=11)e.push("Pino "+p[1]+": GPIO 6–11 são da memória flash.")}});
num("spi_freq",0,10000000,"Clock SPI",e);
v=f.web_user.value.trim();if(!v||v.length>31||v.indexOf(":")>=0||v=="_logout_")e.push("Usuário da página: 1 a 31 caracteres, sem dois-pontos.");
v=f.web_pswd.value;if(v&&(v.length<6||v.indexOf(":")>=0))e.push("Nova senha da página: mínimo de 6 caracteres, sem dois-pontos.");
if(v!==$("wp2").value)e.push("Nova senha da página: a confirmação não confere.");
var b=$("err");b.textContent=e.join("\n");b.style.display=e.length?"block":"none";if(e.length)scrollTo(0,0);else f._s=1;return!e.length}
/* --------------------------- maintenance ----------------------------- */
function exp(){fetch("config.json",{cache:"no-store"}).then(function(r){return r.json()}).then(function(j){delete j.wifi_pswd_set;delete j.ap_pswd_set;
var a=document.createElement("a"),d=new Date(),p=function(n){return("0"+n).slice(-2)};a.href=URL.createObjectURL(new Blob([JSON.stringify(j,null,2)],{type:"application/json"}));
a.download=host(j.name)+"_"+d.getFullYear()+p(d.getMonth()+1)+p(d.getDate())+".json";document.body.appendChild(a);a.click();setTimeout(function(){URL.revokeObjectURL(a.href);a.remove()},500)})}
function imp(i){var r=new FileReader();r.onload=function(){try{var j=JSON.parse(r.result);if(j._mode&&j._mode!=MODE&&!confirm("Este arquivo é de um nó "+j._mode.toUpperCase()+" e este firmware é "+MODE.toUpperCase()+". Importar mesmo assim?"))return;fill(j,1);mark();alert("Configuração carregada no formulário. Revise e clique em Salvar.")}catch(x){alert("Arquivo JSON inválido.")}i.value=""};if(i.files[0])r.readAsText(i.files[0])}
function fac(){if(confirm("Apagar toda a configuração salva e voltar aos valores de fábrica?\nO nó reinicia e o login volta para admin / lorawan."))$("fb").submit()}
/* ------------------------------ actions ------------------------------ */
function act(a,q,v){if(q&&!confirm(q))return;$("amsg").textContent="…";fetch("action",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"a="+a+(v!=null?"&v="+v:"")})
.then(function(r){return r.json()}).then(function(j){$("amsg").textContent=j.ok?j.msg:(j.error||"Falha");setTimeout(st,800)}).catch(function(){$("amsg").textContent="Sem resposta do nó"})}
function fcnt(){var v=prompt("Próximo FCnt de uplink (0 a 4294967295).\nNo ChirpStack, o contador precisa ser maior que o último recebido — ou zere os contadores do dispositivo lá também.",L.fcnt_up!=null?L.fcnt_up:0);if(v==null)return;v=v.trim();if(!/^\d+$/.test(v)||+v>4294967295){alert("Número inválido.");return}act("fcnt",null,v)}
/* ------------------------------- status ------------------------------ */
var L={};
function nf(x){return(+x||0).toLocaleString("pt-BR")}
function dur(s){var d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return d?d+" d "+h+" h":h?h+" h "+m+" min":m?m+" min "+s%60+" s":s+" s"}
function kb(b){return Math.round(b/1024)+" KB"}
function kv(i,v,t){$("kv_"+i).textContent=v;$("kv_"+i).title=v;$("ks_"+i).textContent=t}
function gs(j){var n=j.node,l=j.lw,e=$("gst"),k="ok",t;L=l;
if(l.mode=="otaa"&&!l.joined){k="warn";t=l.join_tries?"Aguardando o JOIN (tentativa "+l.join_tries+"). Confira DevEUI, JoinEUI, AppKey e se o gateway escuta estes canais.":"Iniciando o JOIN…"}
else if(l.pending)t="Transmitindo agora…";
else if(l.last_tx<0)t=l.mode=="abp"?"Sessão ABP provisionada, aguardando o primeiro envio.":"JOIN aceito, aguardando o primeiro envio.";
else{t="Nó transmitindo. Último uplink há "+dur(l.last_tx)+".";if(l.last_tx>Math.max(60,3*l.interval)){k="warn";t="Nenhum uplink há "+dur(l.last_tx)+" (intervalo de "+l.interval+" s). Veja os logs."}}
e.className="st "+k;e.textContent=t;
var rb=$("rbw");rb.hidden=!n.reboot;if(n.reboot)rb.textContent="Configuração salva que só vale após reiniciar: "+n.reboot+".";
$("cd").className=(l.mode=="abp"||l.joined)?"ok":"warn";$("cl").textContent=l.mode=="abp"?"Sessão ABP":l.joined?"JOIN aceito":"Aguardando JOIN";
$("cip").textContent=n.ip||n.ap_ip||location.hostname||"-";
if(l.mode=="abp"){$("cid").textContent=l.devaddr}else{$("cid").dataset.live=1;$("cid").textContent=l.joined?l.devaddr:($("deveui").value||"-");$("cidl").textContent=l.joined?"DevAddr":"DevEUI"}
kv("up",dur(n.up),"contadores zeram ao reiniciar");
kv("mem",kb(n.heap),"mínima desde que ligou: "+kb(n.heap_min));
kv("rssi",n.rssi==null?"-":n.rssi+" dBm",n.wifi=="ap"?"Soft AP "+n.ap_ssid+" · "+n.ap_clients+" conectado(s)":n.wifi=="off"?"Wi-Fi desligado":n.rssi==null?"conectando ao Wi-Fi…":(n.rssi>=-60?"sinal bom":n.rssi>=-75?"sinal regular":"sinal fraco")+" · "+n.ssid+(n.wifi=="apsta"?" · Soft AP ativo":""));
kv("ses",l.mode=="otaa"&&!l.joined?"sem sessão":l.devaddr,l.mode=="otaa"?(l.joined?"OTAA · NetID "+l.netid+" · JOIN em "+l.join_tries+" tentativa(s)":"OTAA · "+l.join_tries+" tentativa(s) de JOIN"):"ABP · NetID "+l.netid);
kv("fc",nf(l.fcnt_up),"próximo de uplink · downlink "+nf(l.fcnt_dn)+(l.mode=="abp"?" · lido da NVS ao ligar: "+nf(l.boot_fcnt):""));
kv("upl",nf(l.up),l.ack?"com ACK do servidor: "+nf(l.ack):rv("confirmed")=="on"?"nenhum ACK ainda":"sem confirmação (unconfirmed)");
var d=l.last_dn;kv("dn",nf(l.dn),d?"último: FPort "+d.port+" · "+d.len+" B · "+d.hex+" · RSSI "+d.rssi+" dBm · SNR "+d.snr+" dB · há "+dur(d.age):"nenhum desde que ligou");
kv("nx",l.pending?"agora":l.next_tx<0?"-":l.next_tx<=0?"em instantes":"em "+dur(l.next_tx),"DR"+l.dr+" · "+l.txpow+" dBm"+(l.freq?" · "+(l.freq/1e6).toFixed(1)+" MHz":"")+" · a cada "+l.interval+" s")}
var T;function st(){clearTimeout(T);if(document.hidden)return;
fetch("status",{cache:"no-store"}).then(function(r){return r.json()}).then(gs)
.catch(function(){$("cl").textContent="sem status";var e=$("gst");e.className="st warn";e.textContent="Sem resposta do nó. Tentando de novo…"})
.then(function(){clearTimeout(T);if(!document.hidden)T=setTimeout(st,5000)})}
document.addEventListener("visibilitychange",function(){if(!document.hidden)st()});
f.addEventListener("change",function(ev){upd();if(ev.target.type!="file")mark()});
f.addEventListener("input",function(){upd();mark()});
addEventListener("beforeunload",function(ev){if(dirty&&!f._s){ev.preventDefault();ev.returnValue=""}});
/* sidebar: same behavior as the gateway page */
var LK=[].slice.call(document.querySelectorAll("#side a"));
function sec(a){return $(a.getAttribute("href").slice(1))}
var lk=null;function mk(c){LK.forEach(function(a){a.classList.toggle("on",a==c)})}
LK.forEach(function(a){a.addEventListener("click",function(){lk=a;mk(a)})});
["wheel","touchstart","keydown","mousedown"].forEach(function(n){addEventListener(n,function(){lk=null},{passive:true})});
function spy(){if(lk)return;var cur=LK[0],best=1e9;LK.forEach(function(a){var t=sec(a).getBoundingClientRect().top;if(t<=220&&220-t<best){best=220-t;cur=a}});
if(innerHeight+scrollY>=document.documentElement.scrollHeight-4)cur=LK.reduce(function(p,a){return sec(a).getBoundingClientRect().top>sec(p).getBoundingClientRect().top?a:p});mk(cur)}
addEventListener("scroll",spy,{passive:true});addEventListener("resize",spy);
lk=LK.filter(function(a){return a.getAttribute("href")==location.hash})[0]||null;if(lk)mk(lk);else spy();
load();st();
</script>
</body>
</html>)HTML";

/* ======================================================================= */
/*                               logs page                                 */
/* ======================================================================= */
static const char PAGE_LOGS[] PROGMEM = R"HTML(<!DOCTYPE html><html lang="pt-BR"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Logs · LoRaWAN Node</title>
<link rel="icon" type="image/svg+xml" href="/favicon.svg"><style>
:root{--bg:#eef2f0;--card:#fff;--fg:#14211d;--mut:#5b6a64;--line:#d3ddd9;--hdr:#0b7a5f;--term:#0f1513;--tfg:#cfe3da}
@media(prefers-color-scheme:dark){:root{--bg:#0d1412;--card:#151f1c;--fg:#e2ebe7;--mut:#93a39d;--line:#2a3833;--hdr:#123d31;--term:#080c0b;color-scheme:dark}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;display:flex;flex-direction:column;height:100vh}
header{background:var(--hdr);color:#fff;padding:12px 16px}header div{max-width:1100px;margin:0 auto;display:flex;justify-content:space-between;align-items:center;gap:8px;flex-wrap:wrap}
h1{font-size:18px;margin:0}header nav{display:flex;gap:14px}header a{color:#fff;font-size:14px}
.bar{max-width:1100px;width:100%;margin:0 auto;padding:10px 16px;display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.bar input[type=text]{flex:1;min-width:140px;padding:7px 10px;border:1px solid var(--line);border-radius:7px;background:var(--card);color:var(--fg);font:inherit}
button,.btn{font:inherit;font-size:14px;padding:7px 12px;border-radius:7px;border:1px solid var(--line);background:var(--card);color:var(--fg);cursor:pointer;text-decoration:none;display:inline-block}
label{font-size:13.5px;color:var(--mut);display:flex;gap:5px;align-items:center}#st{font-size:12.5px;color:var(--mut);margin-left:auto}
main{flex:1;min-height:0;max-width:1100px;width:100%;margin:0 auto;padding:0 16px 16px}
pre{margin:0;height:100%;overflow:auto;background:var(--term);color:var(--tfg);border-radius:10px;padding:12px;font:12.5px/1.45 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;white-space:pre-wrap;word-break:break-all}
@media(max-width:700px){main{padding:0 6px 8px}pre{padding:8px;font-size:clamp(8.5px,2.35vw,11px);line-height:1.3;white-space:pre;word-break:normal}}
footer{text-align:center;color:var(--mut);font-size:12px;padding:0 16px 10px}footer b{color:var(--fg);font-weight:600}footer a{color:inherit;text-decoration:none}footer a:hover b{text-decoration:underline}
.e{color:#ff8a7a}.w{color:#ffd166}.j{color:#7fd4b0}.d{color:#9ab8ff}
</style></head><body>
<header><div><h1>Logs do nó</h1><nav><a href="/">&larr; Configuração</a> <a href="saiu" onclick="return sair()" title="Encerrar o acesso a esta página">Sair</a></nav></div></header>
<div class="bar"><button id="pb" type="button">Pausar</button><button id="cb" type="button">Limpar tela</button>
<a class="btn" href="logs_download" title="Arquivo gerado pelo nó com tudo o que está na memória dele (até 16 KB)">Baixar do nó</a>
<button id="db" type="button" title="Tudo o que esta página recebeu desde que foi aberta">Baixar sessão</button>
<input id="flt" type="text" placeholder="Filtrar (ex.: EV_TX, Downlink, JOIN)">
<label><input id="as" type="checkbox" checked>Rolar automático</label><span id="st">conectando…</span></div>
<main><pre id="lg"></pre></main>
<footer>LoRaWAN Node · Desenvolvido por <a href="https://k8s.adailsilva.com.br/" target="_blank" rel="noopener"><b>AdailSilva</b></a></footer>
<script>
function sair(){if(!confirm("Sair da configuração do nó?"))return false;var x=new XMLHttpRequest();x.open("GET","logout",true,"_logout_","_logout_");x.onloadend=function(){location.replace("saiu")};x.send();return false}
var from=0,paused=false,lines=[],lg=document.getElementById("lg"),st=document.getElementById("st"),flt=document.getElementById("flt"),part="";
function esc(t){return t.replace(/&/g,"&amp;").replace(/</g,"&lt;")}
function cls(l){return/FAIL|ERROR|Invalid|DEAD|inválid/i.test(l)?"e":/WARN|RESET|TXCANCELED|OP_TXRXPEND|Soft AP/.test(l)?"w":/Downlink|ACK received|EV_JOINED/.test(l)?"d":/^EV_|Packet queued/.test(l)?"j":""}
function render(){var f=flt.value.toLowerCase(),h=[];for(var i=0;i<lines.length;i++){var l=lines[i];if(f&&l.toLowerCase().indexOf(f)<0)continue;var c=cls(l);h.push(c?'<span class="'+c+'">'+esc(l)+"</span>":esc(l))}
lg.innerHTML=h.join("\n");if(document.getElementById("as").checked)lg.scrollTop=lg.scrollHeight}
function add(t){t=part+t.replace(/\x1b\[[0-9;]*[A-Za-z]/g,"").replace(/\r/g,"");var a=t.split("\n");part=a.pop();for(var i=0;i<a.length;i++)lines.push(a[i]);if(lines.length>3000)lines=lines.slice(-3000);render()}
function poll(){if(paused||document.hidden){setTimeout(poll,1000);return}
fetch("logs_data?from="+from,{cache:"no-store"}).then(function(r){var s=+r.headers.get("X-Log-Start"),n=+r.headers.get("X-Log-Next");
if(from&&s>from)add("\n[… "+(s-from)+" bytes antigos descartados …]\n");from=n;return r.text()}).then(function(t){if(t)add(t);
st.textContent="atualizado "+new Date().toLocaleTimeString();setTimeout(poll,t&&t.length>=4000?100:1500)})
.catch(function(){st.textContent="sem conexão, tentando…";setTimeout(poll,3000)})}
document.getElementById("pb").onclick=function(){paused=!paused;this.textContent=paused?"Continuar":"Pausar"};
document.getElementById("cb").onclick=function(){lines=[];part="";render()};flt.oninput=render;
document.getElementById("db").onclick=function(){var d=new Date(),p=function(n){return("0"+n).slice(-2)},
name="lorawan_node_sessao_"+d.getFullYear()+p(d.getMonth()+1)+p(d.getDate())+"-"+p(d.getHours())+p(d.getMinutes())+p(d.getSeconds())+".log",
b=new Blob([lines.join("\n")+(part?"\n"+part:"")+"\n"],{type:"text/plain"}),a=document.createElement("a");
a.href=URL.createObjectURL(b);a.download=name;document.body.appendChild(a);a.click();setTimeout(function(){URL.revokeObjectURL(a.href);a.remove()},500)};poll();
</script></body></html>)HTML";

#endif
