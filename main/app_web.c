#include "app_web.h"
#include "app_data.h"
#include "app_ir.h"
#include "app_led.h"
#include "app_log.h"
#include "app_mem.h"
#include "app_ota.h"
#include "app_wifi.h"
#include "cJSON.h"
#include "driver/temperature_sensor.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <string.h>

#define TAG "app_web"

static httpd_handle_t server = NULL;

/* HTML Content */
static const char *index_html =
    "<!DOCTYPE html>"
    "<html lang='en'>"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, "
    "initial-scale=1.0, "
    "maximum-scale=1.0, user-scalable=no'>"
    "<title>Goku IR Control</title>"
    "<link "
    "href='https://fonts.googleapis.com/"
    "css2?family=Inter:wght@400;600&display=swap' "
    "rel='stylesheet'>"
    "<script src='https://cdn.jsdelivr.net/npm/chart.js' "
    "defer></script>"
    "<style>"
    ":root { --primary: #3b82f6; --bg: #0f172a; --card: #1e293b; "
    "--input: "
    "#334155; --text: "
    "#f1f5f9; --success: #22c55e; --danger: #ef4444; "
    "--sidebar-bg: #020617; "
    "--radius: 20px; }"
    "body { font-family: 'Inter', -apple-system, sans-serif; "
    "background: var(--bg); color: var(--text); margin: 0; "
    "padding: 0; "
    "display: flex; height: 100vh; overflow: hidden; }"

    /* Sidebar Styles */
    ".sidebar { width: 250px; background: var(--sidebar-bg); "
    "color: white; "
    "display: flex; flex-direction: column; padding: 20px; "
    "box-sizing: "
    "border-box; "
    "transition: transform 0.3s ease; border-right: 1px solid "
    "#374151; "
    "z-index: 1000; }"
    ".sidebar h2 { margin-top: 0; font-size: 1.5rem; text-align: "
    "center; "
    "color: var(--primary); margin-bottom: 30px; }"
    ".nav-item { padding: 12px 15px; cursor: pointer; "
    "border-radius: 8px; "
    "margin-bottom: 5px; color: #9ca3af; text-decoration: none; "
    "transition: "
    "all 0.2s; }"
    ".nav-item:hover, .nav-item.active { background: #374151; "
    "color: white; }"

    /* Main Content Styles */
    ".main-content { flex: 1; overflow-y: auto; padding: 20px; "
    "position: "
    "relative; }"
    ".container { width: 100%; max-width: 600px; margin: 0 auto; "
    "}"

    /* Toggle Button (Mobile) */
    "#menu-toggle { position: absolute; top: 15px; left: 15px; "
    "z-index: 2000; "
    "background: var(--card); border: 1px solid #374151; color: "
    "white; "
    "padding: 8px 12px; border-radius: 6px; cursor: pointer; "
    "display: none; }"

    ".status { text-align: center; color: #94a3b8; font-size: "
    "0.9em; "
    "margin-top: 10px; }"
    ".hidden { display: none; }"
    "#logViewer { background: #0f172a; color: #22c55e; padding: "
    "15px; "
    "border-radius: 8px; font-family: monospace; height: 300px; "
    "overflow-y: "
    "scroll; white-space: pre-wrap; font-size: 0.85rem; border: "
    "1px solid "
    "#334155; }"

    /* Touch Feedback & Base Styles */
    ".btn { user-select: none; -webkit-tap-highlight-color: "
    "transparent; "
    "background: var(--primary); border: none; padding: 12px "
    "15px; "
    "border-radius: 10px; color: white; cursor: pointer; "
    "font-weight: 600; "
    "width: 100%; transition: all 0.2s; font-size: 1rem; "
    "touch-action: "
    "manipulation; }"
    ".btn:active { transform: scale(0.98); opacity: 0.9; }"
    ".btn-secondary { background: #334155; }"
    ".btn-danger { background: var(--danger); }"
    ".btn-success { background: var(--success); }"
    ".row { display: flex; gap: 10px; margin-top: 15px; "
    "align-items: center; "
    "flex-wrap: wrap; }"
    "input, select { width: 100%; padding: 12px 16px; "
    "border-radius: 12px; "
    "border: "
    "none; background: var(--input); color: white; box-sizing: "
    "border-box; font-size: 1rem; appearance: none; transition: "
    "0.2s; }"
    /* Custom Select Arrow */
    "select { background-image: "
    "url(\"data:image/svg+xml;charset=UTF-8,%3csvg "
    "xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' "
    "fill='none' "
    "stroke='white' stroke-width='2' stroke-linecap='round' "
    "stroke-linejoin='round'%3e%3cpolyline points='6 9 12 15 18 "
    "9'%3e%3c/polyline%3e%3c/svg%3e\");"
    "background-repeat: no-repeat; background-position: right "
    "1rem center; "
    "background-size: 1.2em; }"
    /* Custom Slider */
    "input[type=range] { height: 8px; background: var(--input); "
    "border-radius: "
    "4px; padding: 0; }"
    "input[type=range]::-webkit-slider-thumb { "
    "-webkit-appearance: none; "
    "height: 24px; width: 24px; "
    "border-radius: 50%; background: #fff; box-shadow: 0 2px 6px "
    "rgba(0,0,0,0.3); margin-top: -8px; }"
    ".key-item { display: flex; align-items: center; "
    "justify-content: "
    "space-between; padding: "
    "12px 0; border-bottom: 1px solid #334155; }"
    ".key-item:last-child { border-bottom: none; }"
    ".key-actions { display: flex; gap: 8px; }"
    ".key-actions .btn { padding: 8px 12px; font-size: 0.85rem; "
    "width: auto; }"
    "h1 { text-align: center; margin-bottom: 25px; font-weight: "
    "800; "
    "background: "
    "linear-gradient(to right, #60a5fa, #a78bfa); "
    "-webkit-background-clip: "
    "text; -webkit-text-fill-color: transparent; font-size: "
    "1.8rem; }"
    ".card { background: var(--card); border-radius: "
    "var(--radius); padding: "
    "24px; "
    "margin-bottom: 20px; box-shadow: 0 10px 15px -3px rgba(0, "
    "0, 0, 0.3); "
    "border: none; }"
    "h3 { margin-top: 0; margin-bottom: 20px; font-weight: 600; "
    "color: "
    "#94a3b8; font-size: 0.85rem; text-transform: uppercase; "
    "letter-spacing: "
    "1.5px; }"

    /* Mobile Optimizations */
    "@media (max-width: 768px) {"
    "  .sidebar { position: fixed; top: 0; left: 0; bottom: 0; "
    "transform: "
    "translateX(-100%); }"
    "  .sidebar.active { transform: translateX(0); }"
    "  #menu-toggle { display: block; top: 10px; left: 10px; "
    "padding: 12px; "
    "font-size: 1.2rem; }"
    "  .main-content { padding-top: 60px; padding-left: 15px; "
    "padding-right: "
    "15px; }"
    "  .card { padding: 15px; margin-bottom: 12px; }" /* Reduce
                                                         card
                                                         padding
                                                       */
    "  .btn, input, select { min-height: 48px; font-size: 16px; "
    "} " /* Prevent
            iOS zoom
            & better
            touch */
    "  .dash-row { flex-direction: column; }"
    "  .card { margin-bottom: 20px; }"
    "  .control-group { flex-direction: column; align-items: "
    "flex-start; }"
    "  .control-group > div { width: 100%; justify-content: "
    "space-between; "
    "margin-top: 10px; }"
    "  .control-group select { flex: 1; }"
    "  .control-group button { margin-left: 10px; }"
    "  .row { gap: 12px; }"
    "  .card { margin-bottom: 20px; border-radius: 16px; "
    "background: #1e293b; "
    "border: 1px solid #334155; }"
    "  .btn { border-radius: 12px; padding: 12px 20px; "
    "font-weight: 600; "
    "transition: all 0.2s; }"
    "  .btn:active { transform: scale(0.95); }"
    "  select, input { border-radius: 10px; padding: 10px; "
    "border: 1px solid "
    "#475569; background: #0f172a; color: white; }"
    "  .control-group { background: #334155; padding: 15px; "
    "border-radius: "
    "12px; margin-bottom: 10px; display: flex; align-items: "
    "center; "
    "justify-content: space-between; }"
    "  .sci-fi-container { border: 1px solid #3b82f6; "
    "box-shadow: 0 0 10px "
    "#3b82f6; border-radius: 8px; padding: 2px; margin-bottom: "
    "20px; overflow: "
    "hidden; }"
    "  .neon-grid { display: grid; grid-template-columns: "
    "repeat(auto-fit, "
    "minmax(140px, 1fr)); gap: 15px; "
    "margin-bottom: 20px; }"
    "  .neon-card { background: rgba(15, 23, 42, 0.8); border: "
    "1px solid "
    "#4ade80; border-radius: 12px; padding: 15px; text-align: "
    "center; "
    "box-shadow: 0 0 5px rgba(74, 222, 128, 0.3); position: "
    "relative; }"
    "  .neon-card h4 { margin: 0 0 10px 0; color: #4ade80; "
    "text-shadow: 0 0 "
    "3px #4ade80; font-family: monospace; }"
    "  .neon-val-big { font-size: 1.8rem; font-weight: bold; "
    "color: #fff; "
    "text-shadow: 0 0 5px #fff; margin-bottom: 5px; font-family: "
    "monospace; }"
    "  .neon-bar-track { background: #1e293b; height: 10px; "
    "width: 100%; "
    "border-radius: 5px; overflow: hidden; }"
    "  .neon-bar-fill { height: 100%; background: #4ade80; "
    "box-shadow: 0 0 8px "
    "#4ade80; transition: width 0.5s; }"
    "  .chart-container { background: rgba(15, 23, 42, 0.6); "
    "border: 1px solid "
    "#a855f7; border-radius: 12px; padding: 10px; position: "
    "relative; "
    "box-shadow: 0 0 8px rgba(168, 85, 247, 0.4); }"
    "  .footer-info { font-family: monospace; color: #94a3b8; "
    "font-size: "
    "0.8rem; margin: 15px 0; padding: 0 10px; }"
    "  .bottom-pulse { height: 6px; background: "
    "linear-gradient(90deg, "
    "#3b82f6, #00f3ff, #3b82f6); border-radius: 3px; animation: "
    "pulse 2s "
    "infinite; box-shadow: 0 0 10px #00f3ff; }"
    "  @keyframes pulse { 0% { opacity: 0.6; box-shadow: 0 0 5px "
    "#00f3ff; } "
    "50% { opacity: 1; box-shadow: 0 0 15px #00f3ff; } 100% { "
    "opacity: 0.6; "
    "box-shadow: 0 0 5px #00f3ff; } }"
    "  @media (max-width: 480px) {"
    "    .neon-grid { grid-template-columns: 1fr; }"
    "    .row { flex-wrap: wrap; }"
    "    .row input { width: 100% !important; }"
    "    .row button { width: 100% !important; margin-left: 0 "
    "!important; "
    "margin-top: 5px; }"
    "  }"
    "  .overlay { position: fixed; top: 0; left: 0; right: 0; "
    "bottom: 0; "
    "  background: rgba(0,0,0,0.5); backdrop-filter: blur(2px); "
    "z-index: 900; "
    "display: none; }" /* Added blur */
    "  .overlay.active { display: block; }"
    "}"
    ".sci-fi-card { background: rgba(5, 10, 20, 0.95); border: "
    "1px solid "
    "#00f3ff; }"

    /* LED Control Specifics */
    ".effect-tabs { display: flex; overflow-x: auto; gap: 10px; "
    "margin-bottom: 20px; padding-bottom: 5px; "
    "-webkit-overflow-scrolling: touch; scrollbar-width: none; }"
    ".effect-tabs::-webkit-scrollbar { display: none; }"
    ".effect-tab { background: transparent; border: 1px solid "
    "#334155; color: #94a3b8; padding: 8px 16px; border-radius: "
    "20px; white-space: nowrap; cursor: pointer; transition: "
    "0.2s; font-size: 0.9rem; }"
    ".effect-tab.active { background: var(--primary); color: "
    "white; border-color: var(--primary); font-weight: 600; }"
    ".led-ring-container { position: relative; width: 220px; "
    "height: 220px; margin: 30px auto; }"
    ".led-ring { position: relative; width: 100%; height: 100%; "
    "border-radius: 50%; border: 1px solid #334155; }"
    ".led-pixel { position: absolute; width: 32px; height: 32px; "
    "border-radius: 50%; background: #334155; border: 2px solid "
    "#0f172a; cursor: pointer; transition: 0.2s; box-shadow: 0 0 "
    "10px rgba(0,0,0,0.3); left: 50%; top: 50%; margin: -16px; }"
    ".led-pixel.selected { border-color: #fff; transform: "
    "scale(1.2); z-index: 10; box-shadow: 0 0 15px white; }"
    ".palette { display: grid; grid-template-columns: repeat(8, "
    "1fr); gap: 8px; margin: 20px 0; }"
    ".color-swatch { width: 100%; aspect-ratio: 1; "
    "border-radius: 8px; cursor: pointer; border: 1px solid "
    "#334155; transition: transform 0.1s; }"
    ".color-swatch:active { transform: scale(0.9); }"
    ".control-row { display: flex; align-items: center; "
    "justify-content: space-between; margin-bottom: 15px; }"
    ".control-label { font-size: 0.9rem; color: #cbd5e1; "
    "font-weight: 600; }"
    "</style>"
    "</head>"
    "<body>"

    "<div id='overlay' class='overlay' "
    "onclick='toggleSidebar()'></div>"
    "<button id='menu-toggle' "
    "onclick='toggleSidebar()'>☰</button>"

    "<div id='sidebar' class='sidebar'>"
    "<h2>Goku IR</h2>"
    "<a class='nav-item' "
    "onclick='navTo(\"dashboard\")'>Dashboard</a>"
    "<a class='nav-item' onclick='navTo(\"controls\")'>AC "
    "Control</a>"
    "<a class='nav-item' onclick='navTo(\"led\")'>LED Control</a>"
    "<a class='nav-item' onclick='navTo(\"wifi\")'>Wi-Fi</a>"
    "<a class='nav-item' onclick='navTo(\"logs\")'>System "
    "Logs</a>"
    "<a class='nav-item' onclick='navTo(\"ota\")'>Firmware</a>"
    "</div>"

    "<div class='main-content'>"
    "<div class='container'>"
    "<h1>Control Panel</h1>"

    // Dashboard View
    "<div id='dashboard' class='view'>"

    // Stats Grid (CPU, Temp, RAM)
    "<div class='neon-grid'>"
    // CPU Card
    "<div class='neon-card' style='border-color:#a855f7;'>"
    "<h4 style='color:#a855f7;text-shadow:0 0 3px #a855f7'>CPU "
    "LOAD</h4>"
    "<div id='valCpu' class='neon-val-big'>--%</div>"
    "<div class='neon-bar-track'><div id='progCpu' "
    "class='neon-bar-fill' "
    "style='background:#a855f7;box-shadow:0 0 8px "
    "#a855f7;width:0%'></div></div>"
    "</div>"
    // Temp Card
    "<div class='neon-card' style='border-color:#f59e0b;'>"
    "<h4 style='color:#f59e0b;text-shadow:0 0 3px "
    "#f59e0b'>TEMP</h4>"
    "<div id='valTemp' class='neon-val-big'>--\u00B0C</div>"
    "<div class='neon-bar-track'><div id='progTemp' "
    "class='neon-bar-fill' "
    "style='background:#f59e0b;box-shadow:0 0 8px "
    "#f59e0b;width:0%'></div></div>"
    "</div>"
    // RAM Card
    "<div class='neon-card' style='border-color:#3b82f6;'>"
    "<h4 style='color:#3b82f6;text-shadow:0 0 3px #3b82f6'>RAM "
    "USAGE</h4>"
    "<div id='valRam' class='neon-val-big'>--%</div>"
    "<div class='neon-bar-track'><div id='progRam' "
    "class='neon-bar-fill' "
    "style='background:#3b82f6;box-shadow:0 0 8px "
    "#3b82f6;width:0%'></div></div>"
    "</div>"
    // PSRAM Card
    "<div class='neon-card' style='border-color:#10b981;'>"
    "<h4 style='color:#10b981;text-shadow:0 0 3px #10b981'>PSRAM "
    "USAGE</h4>"
    "<div id='valPsram' class='neon-val-big'>--%</div>"
    "<div class='neon-bar-track'><div id='progPsram' "
    "class='neon-bar-fill' "
    "style='background:#10b981;box-shadow:0 0 8px "
    "#10b981;width:0%'></div></div>"
    "</div>"
    "</div>"

    // Charts
    "<div class='chart-container' style='margin-bottom:15px'>"
    "<canvas id='heapChart'></canvas>"
    "</div>"
    "<div class='chart-container'>"
    "<canvas id='rssiChart'></canvas>"
    "</div>"

    // Footer Info
    "<div class='footer-info'>"
    "<div style='display:flex;justify-content:space-between'>"
    "<span>Firmware : <span id='txtFirmware'>" PROJECT_VERSION "</span></span>"
    "<span id='txtUptime'>Uptime: 0m</span>"
    "<span id='dashStatus' class='status' "
    "style='margin-left:10px'></span>"
    "</div>"
    "</div>"

    // Bottom Pulse
    "<div class='bottom-pulse'></div>"
    "</div>"

    // AC & Learning Controls View
    "<div id='controls' class='view hidden'>"

    "<h2>AC Control Center</h2>"

    // Learning Section
    "<div id='learning' class='card'>"
    "<div "
    "style='display:flex;justify-content:space-between;align-"
    "items:center;"
    "margin-bottom:15px'>"
    "<h3 style='margin:0'>Signal Learning</h3>"
    "<div id='learnStatus' class='status' style='padding:5px "
    "10px;border-radius:20px;font-size:0.8rem;background:#334155'"
    ">Ready</div>"
    "</div>"
    "<div style='display:flex;gap:10px'>"
    "<button class='btn' "
    "style='flex:1;background:var(--primary)' "
    "onclick='startLearn()'>Start Learning</button>"
    "<button class='btn btn-secondary' style='flex:1' "
    "onclick='stopLearn()'>Stop</button>"
    "</div>"
    "</div>"

    // Controls Section
    "<div class='card'>"
    "<h3 style='margin-bottom:20px'>Remote Control</h3>"

    "<div class='control-group' "
    "style='flex-direction:column;align-items:stretch'>"
    "<span class='control-label' "
    "style='margin-bottom:10px'>Power</span>"
    "<div style='display:grid;grid-template-columns:1fr "
    "1fr;gap:15px'>"
    "<button class='btn' "
    "style='background:#ef4444;height:50px;font-size:1.1rem;"
    "display:flex;align-"
    "items:center;justify-content:center' "
    "onclick=\"saveAC('ac_off')\">"
    "<span>\u23FB OFF</span></button>"
    "<button class='btn' "
    "style='background:#22c55e;height:50px;font-size:1.1rem;"
    "display:flex;align-"
    "items:center;justify-content:center' "
    "onclick=\"saveAC('ac_on')\">"
    "<span>\u23FB ON</span></button>"
    "</div>"
    "</div>"

    "<div class='control-group'>"
    "<span class='control-label'>Operation Mode</span>"
    "<div style='display:flex;gap:10px'>"
    "<select id='modeSelect' style='width:120px'>"
    "<option value='auto'>Auto</option><option "
    "value='cool'>Cool</option>"
    "<option value='heat'>Heat</option><option value='fan'>Fan "
    "Controller</option>"
    "</select>"
    "<button class='btn' onclick=\"saveMode()\">Set</button>"
    "</div>"
    "</div>"

    "<div class='control-group'>"
    "<span class='control-label'>Fan Speed</span>"
    "<div style='display:flex;gap:10px'>"
    "<select id='fanSelect' style='width:120px'>"
    "<option value='auto'>Auto</option><option "
    "value='low'>Low</option>"
    "<option value='medium'>Medium</option><option "
    "value='high'>High</option>"
    "</select>"
    "<button class='btn' onclick=\"saveFan()\">Set</button>"
    "</div>"
    "</div>"

    "<div class='control-group'>"
    "<span class='control-label'>Temperature</span>"
    "<div style='display:flex;gap:10px'>"
    "<select id='tempSelect' style='width:120px'>"
    "<option value='16'>16&deg;C</option><option "
    "value='17'>17&deg;C</option>"
    "<option value='18'>18&deg;C</option><option "
    "value='19'>19&deg;C</option>"
    "<option value='20'>20&deg;C</option><option "
    "value='21'>21&deg;C</option>"
    "<option value='22'>22&deg;C</option><option "
    "value='23'>23&deg;C</option>"
    "<option value='24'>24&deg;C</option><option value='25' "
    "selected>25&deg;C</option>"
    "<option value='26'>26&deg;C</option><option "
    "value='27'>27&deg;C</option>"
    "<option value='28'>28&deg;C</option><option "
    "value='29'>29&deg;C</option>"
    "<option value='30'>30&deg;C</option>"
    "</select>"
    "<button class='btn' onclick=\"saveTemp()\">Set</button>"
    "</div>"
    "</div>"
    "</div>" // End Controls card

    "<div id='keys' class='card'>"
    "<h3>Saved Keys</h3>"
    "<div id='keyList'>Loading...</div>"
    "</div>"
    "</div>" // Close controls view wrapper

    "<div id='led' class='view hidden'>"
    "<div class='card'>"
    "<div "
    "style='display:flex;align-items:center;margin-bottom:15px'>"
    "<h2 style='margin:0;flex:1;text-align:center'>LED "
    "CONTROL</h2>"
    "<div style='width:40px'></div>" // spacer
    "</div>"

    // Effect Tabs
    "<div class='effect-tabs'>"
    "<button class='effect-tab active' "
    "onclick=\"setTab('static', this)\">Static</button>"
    "<button class='effect-tab' onclick=\"setTab('running', "
    "this)\">Running</button>"
    "<button class='effect-tab' onclick=\"setTab('rainbow', "
    "this)\">Rainbow</button>"
    "<button class='effect-tab' onclick=\"setTab('fire', "
    "this)\">Fire</button>"
    "<button class='effect-tab' onclick=\"setTab('breathing', "
    "this)\">Breathing</button>"
    "<button class='effect-tab' onclick=\"setTab('blink', "
    "this)\">Blink</button>"
    "<button class='effect-tab' "
    "onclick=\"setTab('knight_rider', this)\">Knight Rider</button>"
    "<button class='effect-tab' "
    "onclick=\"setTab('theater_chase', this)\">Chase</button>"
    "<button class='effect-tab' onclick=\"setTab('color_wipe', "
    "this)\">Wipe</button>"
    "<button class='effect-tab' onclick=\"setTab('loading', "
    "this)\">Loading</button>"
    "<button class='effect-tab' onclick=\"setTab('sparkle', "
    "this)\">Sparkle</button>"
    "<button class='effect-tab' onclick=\"setTab('random', "
    "this)\">Random</button>"
    "<button class='effect-tab' onclick=\"setTab('auto_cycle', "
    "this)\">Auto Cycle</button>"
    "</div>"

    // Controls
    "<div class='control-row'>"
    "<span class='control-label'>SPEED</span>"
    "<span id='valSpeed'>50</span>"
    "</div>"
    "<input type='range' id='ledSpeed' min='1' max='100' "
    "value='50' onchange='saveLedConfig()'>"

    "<div class='control-row' style='margin-top:20px'>"
    "<span class='control-label'>BRIGHTNESS</span>"
    "<span id='valBright'>100</span>"
    "</div>"
    "<input type='range' id='ledBright' min='0' max='100' "
    "value='100' onchange='saveLedConfig()'>"

    // Ring
    "<div class='led-ring-container'>"
    "<div id='ledRing' class='led-ring'></div>"
    "</div>"

    // Palette & Custom Color
    "<div "
    "style='display:flex;justify-content:space-between;align-"
    "items:end'>"
    "<span class='control-label'>CUSTOM COLOR</span>"
    "<input type='color' id='customColor' value='#ffffff' "
    "style='width:50px;height:40px;padding:2px' "
    "onchange='applyColor(this.value)'>"
    "</div>"

    "<div class='palette'>"
    "<div class='color-swatch' style='background:#ef4444' "
    "onclick=\"applyColor('#ef4444')\"></div>"
    "<div class='color-swatch' style='background:#f97316' "
    "onclick=\"applyColor('#f97316')\"></div>"
    "<div class='color-swatch' style='background:#f59e0b' "
    "onclick=\"applyColor('#f59e0b')\"></div>"
    "<div class='color-swatch' style='background:#22c55e' "
    "onclick=\"applyColor('#22c55e')\"></div>"
    "<div class='color-swatch' style='background:#06b6d4' "
    "onclick=\"applyColor('#06b6d4')\"></div>"
    "<div class='color-swatch' style='background:#3b82f6' "
    "onclick=\"applyColor('#3b82f6')\"></div>"
    "<div class='color-swatch' style='background:#a855f7' "
    "onclick=\"applyColor('#a855f7')\"></div>"
    "<div class='color-swatch' style='background:#ffffff' "
    "onclick=\"applyColor('#ffffff')\"></div>"
    "<div class='color-swatch' "
    "style='background:#000000;border-color:#475569' "
    "onclick=\"applyColor('#000000')\"></div>" // OFF
    "</div>"

    // Action Buttons
    "<div style='display:flex;gap:10px;margin-top:10px'>"
    "<button class='btn' onclick='applyToAll()'>APPLY TO "
    "ALL</button>"
    "<button class='btn btn-secondary' "
    "onclick='savePreset()'>SAVE PRESET</button>"
    "</div>"

    "</div>" // End Card
    "</div>"

    // Removed separate colors card

    "<div id='logs' class='card view hidden'>"
    "<h3>System Logs</h3>"
    "<div id='logViewer'>Loading...</div>"
    "</div>"

    // Keys moved to Controls view

    "<div id='wifi' class='card view hidden'>"
    "<h3>Wi-Fi Settings</h3>"
    "<div class='row'>"
    "<input type='text' id='wifiSSID' placeholder='SSID'>"
    "<button id='scanBtn' class='btn' style='width: 80px' "
    "onclick='scanWifi()'>Scan</button>"
    "</div>"
    "<div id='scanList' class='hidden' style='margin-bottom: "
    "10px; max-height: "
    "150px; overflow-y: auto; border: 1px solid #334155; "
    "border-radius: "
    "8px;'></div>"
    "<div class='row'>"
    "<input type='password' id='wifiPass' placeholder='Password' "
    "autocomplete='current-password'>"
    "</div>"
    "<div class='row'>"
    "<button class='btn' onclick='saveWifi()'>Save & "
    "Connect</button>"
    "</div>"
    "</div>"

    "<div id='ota' class='card view hidden'>"
    "<h3>Firmware Update</h3>"
    "<div id='otaSection'>"
    "<p class='status' style='margin-bottom: 10px'>System "
    "Version: <span "
    "id='currentVer'>Loading...</span></p>"
    "<button class='btn' onclick='checkUpdate()'>Check for "
    "Updates</button>"
    "</div>"
    "<div id='updateAvailable' class='hidden'>"
    "<p class='status' style='color: var(--success); "
    "margin-bottom: 10px'>New "
    "Version Available: <b id='newVer'></b></p>"
    "<button class='btn btn-success' "
    "onclick='startUpdate()'>Update "
    "Now</button>"
    "</div>"
    "<div id='otaStatus' class='status'></div>"

    "</div>"

    "</div></div>" // Close container and main-content

    "<script>"
    "const statusEl = document.getElementById('learnStatus');"
    "const keyListEl = document.getElementById('keyList');"

    // Sidebar Functionality
    "function toggleSidebar() {"
    "  "
    "document.getElementById('sidebar').classList.toggle('active'"
    ");"
    "  "
    "document.getElementById('overlay').classList.toggle('active'"
    ");"
    "}"

    "window.onload = function() {\n"
    "  navTo('dashboard');\n"
    "  checkUpdate();\n"
    "  setInterval(() => { "
    "if(!document.getElementById('dashboard').classList.contains('hidden')) "
    "updateDashboard(); }, 5000);\n"
    "  setTimeout(initCharts, 500);\n"
    "  document.getElementById('dashStatus').innerText = 'JS Active';\n"
    "};\n"

    "function navTo(id) {"
    "  if(window.innerWidth <= 768) toggleSidebar();"
    "  document.querySelectorAll('.view').forEach(el => "
    "el.classList.add('hidden'));"
    "  const el = document.getElementById(id);"
    "  if(el) el.classList.remove('hidden');"
    "  if(id === 'dashboard') updateDashboard();"
    "  if(id === 'logs') fetchLogs();"
    "  if(id === 'controls' || id === 'keys') fetchKeys();" // Fetch
                                                            // keys
    "  if(id === 'led') { fetchLedConfig(); fetchSystemColors(); "
    "}"
    "}"

    "async function fetchLogs() {\n"
    "  try {\n"
    "    const res = await fetch('/api/system/logs');\n"
    "    if (!res.ok) throw new Error('Failed');\n"
    "    const text = await res.text();\n"
    "    document.getElementById('logViewer').innerText = text || 'No logs "
    "available.';\n"
    "  } catch(e) {\n"
    "    document.getElementById('logViewer').innerText = 'Failed to load "
    "logs.';\n"
    "  }\n"
    "}\n"

    "async function fetchKeys() {\n"
    "  try {\n"
    "    const res = await fetch('/api/ir/list');\n"
    "    if (!res.ok) throw new Error('Failed');\n"
    "    const keys = await res.json();\n"
    "    let html = '';\n"
    "    if(keys.length === 0) html = '<p>No saved keys.</p>';\n"
    "    keys.forEach(k => {\n"
    "      html += `<div "
    "style='display:flex;justify-content:space-between;align-items:center;"
    "background:#0f172a;padding:10px;margin-bottom:5px;border-radius:8px'>`\n"
    "           + `<span>${k}</span>`\n"
    "           + `<div><button class='btn' style='padding:5px "
    "10px;margin-right:5px;font-size:0.8rem' "
    "onclick=\"sendKey('${k}')\">Send</button>`\n"
    "           + `<button class='btn' style='padding:5px "
    "10px;background:#ef4444;font-size:0.8rem' "
    "onclick=\"deleteKey('${k}')\">Del</button></div></div>`;\n"
    "    });\n"
    "    document.getElementById('keyList').innerHTML = html;\n"
    "  } catch(e) {\n"
    "    document.getElementById('keyList').innerHTML = 'Failed to load "
    "keys.';\n"
    "  }\n"
    "}\n"

    // ... existing vars ...

    "async function fetchSystemColors() {"
    "  const div = document.getElementById('systemColorsList');"
    "  div.innerHTML = 'Loading...';"
    "  try {"
    "    const res = await fetch('/api/led/state-config');"
    "    const data = await res.json();"
    "    div.innerHTML = '';"
    "    data.forEach(item => {"
    "       const row = document.createElement('div');"
    "       row.className = 'row';"
    "       row.innerHTML = `<label style='width: "
    "40%'>${item.name}</label>` +"
    "           `<input type='color' value='${rgbToHex(item.r, "
    "item.g, "
    "item.b)}' ` +"
    "           `onchange='saveSystemColor(${item.id}, "
    "this.value)'>`;"
    "       div.appendChild(row);"
    "    });"
    "  } catch(e) { div.innerHTML = 'Error loading colors'; }"
    "}"

    "async function saveSystemColor(id, hex) {"
    "  const c = hexToRgb(hex);"
    "  try {"
    "    await "
    "fetch(`/api/led/"
    "state-config?id=${id}&r=${c.r}&g=${c.g}&b=${c.b}`, "
    "{method:'POST'});"
    "  } catch(e) { alert('Failed to save color'); }"
    "}"

    "async function fetchList() {"
    "  try { const res = await fetch('/api/ir/list');"
    "  const keys = await res.json(); renderList(keys); } "
    "catch(e){}"
    "}"

    "function renderList(keys) {"
    "  keyListEl.innerHTML = '';"
    "  if(keys.length === 0) { keyListEl.innerHTML = '<div "
    "class=\"status\">No "
    "keys saved</div>'; return; }"
    "  keys.forEach(key => {"
    "    const div = document.createElement('div');"
    "    div.className = 'key-item';"
    "    div.innerHTML = `<div><strong>${key}</strong></div><div "
    "class='key-actions'>"
    "      <button class='btn btn-secondary' "
    "onclick=\"sendKey('${key}')\">Test</button>"
    "      <button class='btn btn-danger' "
    "onclick=\"deleteKey('${key}')\">Del</button>"
    "    </div>`;"
    "    keyListEl.appendChild(div);"
    "  });"
    "}"

    "async function startLearn() {"
    "  await fetch('/api/learn/start', {method:'POST'});"
    "  statusEl.innerText = 'Listening... Press remote button';"
    "  statusEl.style.color = '#fbbf24';"
    "}"

    "async function stopLearn() {"
    "  await fetch('/api/learn/stop', {method:'POST'});"
    "  statusEl.innerText = 'Stopped';"
    "  statusEl.style.color = '#94a3b8';"
    "}"

    "async function saveAC(keyName) {"
    "  const res = await fetch('/api/save?key=' + "
    "encodeURIComponent(keyName), "
    "{method:'POST'});"
    "  if(res.ok) { statusEl.innerText = 'Saved: '+keyName; "
    "statusEl.style.color = '#22c55e'; fetchList(); }"
    "  else alert('Failed too save. Ensure Learning mode is "
    "Active.');"
    "}"

    "function saveMode() { saveAC('mode_' + "
    "document.getElementById('modeSelect').value); }"
    "function saveFan() { saveAC('fan_' + "
    "document.getElementById('fanSelect').value); }"
    "function saveTemp() { saveAC('temp_' + "
    "document.getElementById('tempSelect').value); }"

    "async function sendKey(key) {"
    "  await fetch('/api/send?key='+encodeURIComponent(key), "
    "{method:'POST'});"
    "}"

    "async function deleteKey(key) {"
    "  if(!confirm('Delete ' + key + '?')) return;"
    "  await "
    "fetch('/api/ir/delete?key='+encodeURIComponent(key), "
    "{method:'POST'});"
    "  fetchList();"
    "}"

    "async function checkUpdate() {"
    "  const status = document.getElementById('otaStatus');"
    "  status.innerText = 'Checking...';"
    "  try {"
    "    const res = await fetch('/api/ota/check');"
    "    const data = await res.json();"
    "    document.getElementById('currentVer').innerText = "
    "data.current;"
    "    if(data.available) {"
    "       "
    "document.getElementById('otaSection').classList.add('hidden'"
    ");"
    "       "
    "document.getElementById('updateAvailable').classList.remove("
    "'hidden');"
    "       document.getElementById('newVer').innerText = "
    "data.latest;"
    "       status.innerText = '';"
    "    } else {"
    "       status.innerText = 'System is up to date.';"
    "    }"
    "  } catch (e) { status.innerText = 'Error checking update'; "
    "}"
    "}"

    "async function startUpdate() {"
    "  if(!confirm('Start Firmware Update? Device will "
    "reboot.')) return;"
    "  const status = document.getElementById('otaStatus');"
    "  status.innerText = 'Starting update...';"
    "  try {"
    "    const res = await fetch('/api/ota/start', "
    "{method:'POST'});"
    "    if(res.ok) status.innerText = 'Update started! Wait for "
    "reboot...';"
    "    else status.innerText = 'Failed to start update';"
    "  } catch(e) { status.innerText = 'Error starting update'; }"
    "}"

    "async function saveWifi() {"
    "  const ssid = document.getElementById('wifiSSID').value;"
    "  const pass = document.getElementById('wifiPass').value;"
    "  if(!ssid) { alert('SSID is required'); return; }"
    "  if(!confirm('Save credentials and restart device?')) "
    "return;"
    "  "
    "  try {"
    "    const res = await fetch('/api/wifi/config?ssid=' + "
    "encodeURIComponent(ssid) + '&password=' + "
    "encodeURIComponent(pass), "
    "{method:'POST'});"
    "    if(res.ok) alert('Settings saved. Device rebooting...');"
    "    else alert('Failed to save settings');"
    "  } catch(e) { alert('Error: ' + e); }"
    "}"

    "async function scanWifi() {\n"
    "  const btn = document.getElementById('scanBtn');\n"
    "  const list = document.getElementById('scanList');\n"
    "  btn.disabled = true; btn.innerText = 'Scanning...';\n"
    "  list.innerHTML = ''; list.classList.remove('hidden');\n"
    "  try {\n"
    "    const res = await fetch('/api/wifi/scan');\n"
    "    const data = await res.json();\n"
    "    if(data.length === 0) list.innerHTML = '<div class=\"status\">No "
    "networks found</div>';\n"
    "    data.forEach(net => {\n"
    "       const div = document.createElement('div');\n"
    "       div.className = 'key-item';\n"
    "       div.style.cursor = 'pointer';\n"
    "       div.innerHTML = `<div><strong>${net.ssid}</strong> "
    "<small>(${net.rssi}dBm)</small></div>`;\n"
    "       div.onclick = () => { document.getElementById('wifiSSID').value = "
    "net.ssid; list.classList.add('hidden'); };\n"
    "       list.appendChild(div);\n"
    "    });\n"
    "  } catch(e) { list.innerHTML = '<div class=\"status\">Scan "
    "failed</div>'; }\n"
    "  btn.disabled = false; btn.innerText = 'Scan';\n"
    "}\n"

    "function hexToRgb(hex) {\n"
    "  var result = /^#?([a-f\\d]{2})([a-f\\d]{2})([a-f\\d]{2})$/i.exec(hex);\n"
    "  return result ? {\n"
    "    r: parseInt(result[1], 16),\n"
    "    g: parseInt(result[2], 16),\n"
    "    b: parseInt(result[3], 16)\n"
    "  } : null;\n"
    "}\n"
    "\n"
    "function componentToHex(c) { var hex = c.toString(16); return hex.length "
    "== 1 ? '0' + hex : hex; }\n"
    "function rgbToHex(r, g, b) { return '#' + componentToHex(r) + "
    "componentToHex(g) + componentToHex(b); }\n"
    "\n"
    "let ledColors = [];\n"
    "let selectedLed = -1;\n"
    "let currentEffect = 'static';\n"
    "\n"
    "function initRing() {\n"
    "  const ring = document.getElementById('ledRing');\n"
    "  ring.innerHTML = '';\n"
    "  const radius = 90;\n"
    "  for(let i=0; i<8; i++) {\n"
    "    const div = document.createElement('div');\n"
    "    div.className = 'led-pixel';\n"
    "    const angle = (i * 360 / 8) - 90;\n"
    "    const rad = angle * Math.PI / 180;\n"
    "    const x = Math.round(radius * Math.cos(rad));\n"
    "    const y = Math.round(radius * Math.sin(rad));\n"
    "    div.style.transform = `translate(${x}px, ${y}px)`;\n"
    "    div.onclick = (e) => selectLed(i, e);\n"
    "    div.id = 'led-'+i;\n"
    "    ring.appendChild(div);\n"
    "  }\n"
    "}\n"
    "\n"
    "function selectLed(i, e) {\n"
    "  if(e) e.stopPropagation();\n"
    "  selectedLed = i;\n"
    "  "
    "document.querySelectorAll('.led-pixel').forEach(el=>el.classList.remove('"
    "selected'));\n"
    "  document.getElementById('led-'+i).classList.add('selected');\n"
    "}\n"
    "\n"
    "function updateRingUI() {\n"
    "  ledColors.forEach((c, i) => {\n"
    "    const el = document.getElementById('led-'+i);\n"
    "    if(el) el.style.backgroundColor = `rgb(${c[0]}, ${c[1]}, ${c[2]})`;\n"
    "  });\n"
    "}\n"
    "\n"
    "function setTab(effect, btn) {\n"
    "  currentEffect = effect;\n"
    "  "
    "document.querySelectorAll('.effect-tab').forEach(b=>b.classList.remove('"
    "active'));\n"
    "  if(btn) btn.classList.add('active');\n"
    "  else {\n"
    "  }\n"
    "  fetchLedConfig();\n"
    "}\n"
    "\n"
    "async function applyColor(hex) {\n"
    "  if(!hex) hex = document.getElementById('customColor').value;\n"
    "  const c = hexToRgb(hex);\n"
    "  if(selectedLed !== -1) {\n"
    "    ledColors[selectedLed] = [c.r, c.g, c.b];\n"
    "    updateRingUI();\n"
    "    await "
    "fetch(`/api/led/"
    "config?effect=${currentEffect}&index=${selectedLed}&r=${c.r}&g=${c.g}&b=${"
    "c.b}`, {method:'POST'});\n"
    "  }\n"
    "}\n"
    "\n"
    "async function applyToAll() {\n"
    "  const hex = document.getElementById('customColor').value;\n"
    "  const c = hexToRgb(hex);\n"
    "  for(let i=0; i<8; i++) ledColors[i] = [c.r, c.g, c.b];\n"
    "  updateRingUI();\n"
    "  await "
    "fetch(`/api/led/"
    "config?effect=${currentEffect}&index=255&r=${c.r}&g=${c.g}&b=${c.b}`, "
    "{method:'POST'});\n"
    "}\n"
    "\n"
    "async function saveLedConfig() {\n"
    "  const speed = document.getElementById('ledSpeed').value;\n"
    "  const bright = document.getElementById('ledBright').value;\n"
    "  document.getElementById('valSpeed').innerText = speed;\n"
    "  document.getElementById('valBright').innerText = bright;\n"
    "  await "
    "fetch(`/api/led/"
    "config?effect=${currentEffect}&speed=${speed}&brightness=${bright}`, "
    "{method:'POST'});\n"
    "}\n"
    "\n"
    "async function fetchLedConfig() {\n"
    "  try {\n"
    "    const res = await fetch(`/api/led/config?effect=${currentEffect}`);\n"
    "    const data = await res.json();\n"
    "    ledColors = data.colors || [];\n"
    "    updateRingUI();\n"
    "    document.getElementById('ledSpeed').value = data.speed;\n"
    "    document.getElementById('valSpeed').innerText = data.speed;\n"
    "    document.getElementById('ledBright').value = data.brightness;\n"
    "    document.getElementById('valBright').innerText = data.brightness;\n"
    "  } catch(e) {}\n"
    "}\n"
    "\n"
    "function savePreset() {\n"
    "    fetch('/api/led/save-preset', {method: 'POST'})\n"
    "    .then(r => { if(r.ok) alert('Preset saved!'); else alert('Failed to "
    "save'); })\n"
    "    .catch(e => alert('Error saving preset'));\n"
    "}\n"

    "const oldNavTo = navTo;\n"
    "navTo = function(id) {\n"
    "  if(id === 'led') {\n"
    "     if(document.getElementById('ledRing').innerHTML === '') initRing();\n"
    "     fetchLedConfig();\n"
    "  }\n"
    "  if(window.innerWidth <= 768) toggleSidebar();\n"
    "  document.querySelectorAll('.view').forEach(el => "
    "el.classList.add('hidden'));\n"
    "  const el = document.getElementById(id);\n"
    "  if(el) el.classList.remove('hidden');\n"
    "  if(id === 'dashboard') updateDashboard();\n"
    "  if(id === 'logs') fetchLogs();\n"
    "  if(id === 'controls' || id === 'keys') fetchKeys();\n"
    "};\n"

    "let heapChart, rssiChart;\n"
    "function initCharts() {\n"
    "  const ctxHeap = "
    "document.getElementById('heapChart').getContext('2d');"
    "  heapChart = new Chart(ctxHeap, {"
    "    type: 'line',"
    "    data: { labels: [], datasets: [{ label: 'Free Heap "
    "(KB)', data: [], "
    "borderColor: '#3b82f6', tension: 0.4 }] },"
    "    options: { animation: false, scales: { y: { "
    "beginAtZero: true } } }"
    "  });"
    "  const ctxRssi = "
    "document.getElementById('rssiChart').getContext('2d');"
    "  rssiChart = new Chart(ctxRssi, {"
    "    type: 'line',"
    "    data: { labels: [], datasets: [{ label: 'WiFi RSSI "
    "(dBm)', data: [], "
    "borderColor: '#22c55e', tension: 0.4 }] },"
    "    options: { animation: false }"
    "  });"
    "}"

    "async function updateDashboard() {\n"
    "  try {"
    "    const res = await fetch('/api/system/stats');\n"
    "    const data = await res.json();\n"

    // Uptime formatting
    "    const d = Math.floor(data.uptime / 86400);"
    "    const h = Math.floor((data.uptime % 86400) / 3600);"
    "    const m = Math.floor((data.uptime % 3600) / 60);"
    "    document.getElementById('txtUptime').innerText = "
    "`Uptime: ${d}d ${h}h "
    "${m}m`;"

    // CPU (Simulated for visual as no API data)
    "    const cpu = Math.floor(Math.random() * 10) + 2;"
    "    document.getElementById('valCpu').innerText = cpu + '%';"
    "    document.getElementById('progCpu').style.width = cpu + "
    "'%';"

    // RAM usage (Total assumed 300KB for S3 approx available
    // heap)"
    "    const totalHeap = 300000;" // approx
    "    const used = totalHeap - data.free_heap;"
    "    const ramPct = Math.round((used / totalHeap) * 100);"
    "    document.getElementById('valRam').innerText = ramPct + "
    "'%';"
    "    document.getElementById('progRam').style.width = ramPct "
    "+ '%';"

    // PSRAM usage
    "    if (data.psram_total > 0) {"
    "      const psramUsed = data.psram_total - data.psram_free;"
    "      const psramPct = Math.round((psramUsed / "
    "data.psram_total) * 100);"
    "      document.getElementById('valPsram').innerText = "
    "psramPct + '%';"
    "      document.getElementById('progPsram').style.width = "
    "psramPct + '%';"
    "    } else {"
    "      document.getElementById('valPsram').innerText = 'N/A';"
    "      document.getElementById('progPsram').style.width = "
    "'0%';"
    "    }"

    // Temperature
    "    if(data.temp) {"
    "       document.getElementById('valTemp').innerText = "
    "data.temp.toFixed(1) + ' \u00B0C';"
    "       const tempPct = Math.min((data.temp / 80) * 100, "
    "100);"
    "       document.getElementById('progTemp').style.width = "
    "tempPct + '%';"
    "    }"

    "    const now = new Date().toLocaleTimeString();"
    "    if(heapChart) {"
    "       if(heapChart.data.labels.length > 20) { "
    "heapChart.data.labels.shift(); "
    "heapChart.data.datasets[0].data.shift(); }"
    "       heapChart.data.labels.push(now);"
    "       heapChart.data.datasets[0].data.push(data.free_heap "
    "/ 1024);"
    "       heapChart.update();"
    "    }"
    "    if(rssiChart) {"
    "       if(rssiChart.data.labels.length > 20) { "
    "rssiChart.data.labels.shift(); "
    "rssiChart.data.datasets[0].data.shift(); }"
    "       rssiChart.data.labels.push(now);"
    "       rssiChart.data.datasets[0].data.push(data.rssi);"
    "       rssiChart.update();"
    "    }"
    "  } catch(e) { "
    "     document.getElementById('dashStatus').innerText = 'Err: ' + "
    "e.message;"
    "     document.getElementById('dashStatus').style.color = 'red';"
    "  }"
    "}"

    // Interval moved to window.onload

    // Global logic moved to window.onload

    "async function clearLogs() {\n"
    "  try {"
    "    await fetch('/api/system/logs/clear', {method: 'POST'});"
    "    fetchLogs();"
    "  } catch(e) {}"
    "}"

    "fetchList();\n"
    "fetchLedConfig();\n"
    "fetchSystemColors();\n"
    "fetchLogs();\n"
    "fetch('/api/ota/check').then(r=>r.json()).then(d=>{\n"
    "   document.getElementById('currentVer').innerText=d.current;\n"
    "   document.getElementById('txtFirmware').innerText=d.current;\n"
    "}).catch(e=>{});\n"
    "</script>"
    "</body></html>";

static esp_err_t root_get_handler(httpd_req_t *req) {
  size_t len = strlen(index_html);
  ESP_LOGI(TAG, "Serving index.html, size: %zu", len);
  const char *ptr = index_html;
  size_t chunk_size = 512;

  while (len > 0) {
    size_t to_send = (len > chunk_size) ? chunk_size : len;
    esp_err_t err = ESP_FAIL;
    int retries = 0;

    // Retry loop for sending chunk
    while (retries < 5) {
      err = httpd_resp_send_chunk(req, ptr, to_send);
      if (err == ESP_OK)
        break;

      // Wait a bit before retry
      vTaskDelay(pdMS_TO_TICKS(20));
      retries++;
    }

    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error sending chunk after retries: %d. Rem: %zu", err,
               len);
      return err;
    }
    ptr += to_send;
    len -= to_send;
  }
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

static esp_err_t api_ir_list_handler(httpd_req_t *req) {
  cJSON *list = app_data_get_ir_keys();
  char *json_str = cJSON_PrintUnformatted(list);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
  cJSON_Delete(list);
  free(json_str);
  return ESP_OK;
}

static esp_err_t api_learn_start_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "API: Start Learn");
  esp_err_t err = app_ir_start_learn();
  if (err == ESP_OK) {
    httpd_resp_send(req, "Started", HTTPD_RESP_USE_STRLEN);
  } else {
    httpd_resp_send_500(req);
  }
  return ESP_OK;
}

static esp_err_t api_learn_stop_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "API: Stop Learn");
  app_ir_stop_learn();
  httpd_resp_send(req, "Stopped", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t api_save_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char key[32] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "key", key, sizeof(key)) == ESP_OK) {
        ESP_LOGI(TAG, "API: Save Key %s", key);
        if (app_ir_save_learned_result(key) == ESP_OK) {
          httpd_resp_send(req, "Saved", HTTPD_RESP_USE_STRLEN);
        } else {
          httpd_resp_send_500(req);
        }
      }
    }
    free(buf);
  }

  if (key[0] == 0) {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_send_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char key[32] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "key", key, sizeof(key)) == ESP_OK) {
        ESP_LOGI(TAG, "API: Send Key %s", key);
        app_ir_send_key(key);
        httpd_resp_send(req, "Sent", HTTPD_RESP_USE_STRLEN);
      }
    }
    free(buf);
  }

  if (key[0] == 0) {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_delete_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char key[32] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "key", key, sizeof(key)) == ESP_OK) {
        ESP_LOGI(TAG, "API: Delete Key %s", key);
        app_data_delete_ir(key);
        httpd_resp_send(req, "Deleted", HTTPD_RESP_USE_STRLEN);
      }
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_rename_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char old_key[32] = {0};
  char new_key[32] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      httpd_query_key_value(buf, "old", old_key, sizeof(old_key));
      httpd_query_key_value(buf, "new", new_key, sizeof(new_key));

      if (old_key[0] && new_key[0]) {
        ESP_LOGI(TAG, "API: Rename %s -> %s", old_key, new_key);
        if (app_data_rename_ir(old_key, new_key) == ESP_OK) {
          httpd_resp_send(req, "Renamed", HTTPD_RESP_USE_STRLEN);
        } else {
          httpd_resp_send_500(req);
        }
      } else {
        httpd_resp_send_404(req);
      }
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_ota_check_handler(httpd_req_t *req) {
  char remote_ver_str[32] = {0};
  char response[128];
  bool available = false;

  // Optimistic check. If logic fails or server is down, we handle it.
  // Return local cached status to avoid blocking the Web Server thread
  const char *cached = app_ota_get_cached_version();
  available = app_ota_is_update_available();
  strncpy(remote_ver_str, cached, sizeof(remote_ver_str) - 1);

  // Trigger a fresh background check
  app_ota_trigger_check();

  snprintf(response, sizeof(response),
           "{\"current\":\"%s\", \"latest\":\"%s\", \"available\":%s}",
           PROJECT_VERSION, remote_ver_str, available ? "true" : "false");

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t api_ota_start_handler(httpd_req_t *req) {
  char url[256];
  snprintf(url, sizeof(url), "%s/goku-ir-device.bin", CONFIG_OTA_SERVER_URL);

  ESP_LOGI(TAG, "Starting manual update from UI: %s", url);

  // When triggering manually, we might not know the exact version unless
  // checking first. For now, label it as "Manual Update" or similar.
  if (app_ota_start(url) == ESP_OK) {
    httpd_resp_send(req, "Started", HTTPD_RESP_USE_STRLEN);
  } else {
    httpd_resp_send_500(req);
  }
  return ESP_OK;
}

static esp_err_t api_wifi_config_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char ssid[33] = {0};
  char password[65] = {0};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid)) == ESP_OK) {
        httpd_query_key_value(buf, "password", password, sizeof(password));

        ESP_LOGI(TAG, "API: Update WiFi Config. SSID: %s", ssid);
        app_wifi_update_credentials(ssid, password);
        httpd_resp_send(req, "Saved", HTTPD_RESP_USE_STRLEN);
      } else {
        httpd_resp_send_500(req);
      }
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_wifi_scan_handler(httpd_req_t *req) {
  uint16_t ap_count = 0;
  wifi_ap_record_t *ap_list = NULL;

  if (app_wifi_get_scan_results(&ap_list, &ap_count) != ESP_OK) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  cJSON *root = cJSON_CreateArray();
  for (int i = 0; i < ap_count; i++) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "ssid", (char *)ap_list[i].ssid);
    cJSON_AddNumberToObject(item, "rssi", ap_list[i].rssi);
    cJSON_AddItemToArray(root, item);
  }
  free(ap_list);

  char *json_str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
  cJSON_Delete(root);
  free(json_str);
  return ESP_OK;
}

static esp_err_t api_led_config_get_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char param[32] = {0};
  app_led_effect_t effect = APP_LED_EFFECT_STATIC;

  // Check if specific effect requested
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "effect", param, sizeof(param)) ==
          ESP_OK) {
        if (strcmp(param, "rainbow") == 0)
          effect = APP_LED_EFFECT_RAINBOW;
        else if (strcmp(param, "running") == 0)
          effect = APP_LED_EFFECT_RUNNING;
        else if (strcmp(param, "breathing") == 0)
          effect = APP_LED_EFFECT_BREATHING;
        else if (strcmp(param, "blink") == 0)
          effect = APP_LED_EFFECT_BLINK;
        else if (strcmp(param, "knight_rider") == 0)
          effect = APP_LED_EFFECT_KNIGHT_RIDER;
        else if (strcmp(param, "loading") == 0)
          effect = APP_LED_EFFECT_LOADING;
        else if (strcmp(param, "color_wipe") == 0)
          effect = APP_LED_EFFECT_COLOR_WIPE;
        else if (strcmp(param, "theater_chase") == 0)
          effect = APP_LED_EFFECT_THEATER_CHASE;
        else if (strcmp(param, "fire") == 0)
          effect = APP_LED_EFFECT_FIRE;
        else if (strcmp(param, "sparkle") == 0)
          effect = APP_LED_EFFECT_SPARKLE;
        else
          effect = APP_LED_EFFECT_STATIC;
      } else {
        // If no effect specified, get current
        uint8_t r, g, b, br, sp;
        app_led_get_config(&r, &g, &b, &effect, &br, &sp);
      }
    }
    free(buf);
  } else {
    // If no query string, get current
    uint8_t r, g, b, br, sp;
    app_led_get_config(&r, &g, &b, &effect, &br, &sp);
  }

  uint8_t speed;
  uint8_t colors[8][3];
  app_led_get_effect_config(effect, &speed, colors);

  // Get global brightness
  uint8_t global_br;
  app_led_get_config(NULL, NULL, NULL, NULL, &global_br, NULL);

  // Construct JSON manually or via cJSON
  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "speed", speed);
  cJSON_AddNumberToObject(root, "brightness", global_br);

  // Map enum to string
  const char *effect_str = "static";
  switch (effect) {
  case APP_LED_EFFECT_RAINBOW:
    effect_str = "rainbow";
    break;
  case APP_LED_EFFECT_RUNNING:
    effect_str = "running";
    break;
  case APP_LED_EFFECT_BREATHING:
    effect_str = "breathing";
    break;
  case APP_LED_EFFECT_BLINK:
    effect_str = "blink";
    break;
  case APP_LED_EFFECT_KNIGHT_RIDER:
    effect_str = "knight_rider";
    break;
  case APP_LED_EFFECT_LOADING:
    effect_str = "loading";
    break;
  case APP_LED_EFFECT_COLOR_WIPE:
    effect_str = "color_wipe";
    break;
  case APP_LED_EFFECT_THEATER_CHASE:
    effect_str = "theater_chase";
    break;
  case APP_LED_EFFECT_FIRE:
    effect_str = "fire";
    break;
  case APP_LED_EFFECT_SPARKLE:
    effect_str = "sparkle";
    break;
  default:
    effect_str = "static";
    break;
  }
  cJSON_AddStringToObject(root, "effect", effect_str);

  cJSON *arr = cJSON_CreateArray();
  for (int i = 0; i < 8; i++) {
    cJSON *c = cJSON_CreateArray();
    cJSON_AddItemToArray(c, cJSON_CreateNumber(colors[i][0]));
    cJSON_AddItemToArray(c, cJSON_CreateNumber(colors[i][1]));
    cJSON_AddItemToArray(c, cJSON_CreateNumber(colors[i][2]));
    cJSON_AddItemToArray(arr, c);
  }
  cJSON_AddItemToObject(root, "colors", arr);

  char *json_str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
  cJSON_Delete(root);
  free(json_str);
  return ESP_OK;
}

static esp_err_t api_led_config_post_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char param[32] = {0};
  uint8_t r = 0, g = 0, b = 0;
  int index = -1;
  app_led_effect_t effect = APP_LED_EFFECT_STATIC;
  bool effect_found = false;

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {

      // Determine Effect
      if (httpd_query_key_value(buf, "effect", param, sizeof(param)) ==
          ESP_OK) {
        if (strcmp(param, "rainbow") == 0)
          effect = APP_LED_EFFECT_RAINBOW;
        else if (strcmp(param, "running") == 0)
          effect = APP_LED_EFFECT_RUNNING;
        else if (strcmp(param, "breathing") == 0)
          effect = APP_LED_EFFECT_BREATHING;
        else if (strcmp(param, "blink") == 0)
          effect = APP_LED_EFFECT_BLINK;
        else if (strcmp(param, "knight_rider") == 0)
          effect = APP_LED_EFFECT_KNIGHT_RIDER;
        else if (strcmp(param, "loading") == 0)
          effect = APP_LED_EFFECT_LOADING;
        else if (strcmp(param, "color_wipe") == 0)
          effect = APP_LED_EFFECT_COLOR_WIPE;
        else if (strcmp(param, "theater_chase") == 0)
          effect = APP_LED_EFFECT_THEATER_CHASE;
        else if (strcmp(param, "fire") == 0)
          effect = APP_LED_EFFECT_FIRE;
        else if (strcmp(param, "sparkle") == 0)
          effect = APP_LED_EFFECT_SPARKLE;
        else if (strcmp(param, "random") == 0)
          effect = APP_LED_EFFECT_RANDOM;
        else if (strcmp(param, "auto_cycle") == 0)
          effect = APP_LED_EFFECT_AUTO_CYCLE;
        else
          effect = APP_LED_EFFECT_STATIC;
        effect_found = true;
      } else {
        // If not specified, get current? OR assume static?
        // Better: if modifying config, we should know the effect.
        // If just switching effect, use set_effect.
        // Let's first check if we are just switching effect (no colors/speed)
      }

      // Brightness (Global)
      if (httpd_query_key_value(buf, "brightness", param, sizeof(param)) ==
          ESP_OK) {
        app_led_set_brightness(atoi(param));
      }

      // Speed (Per Effect)

      // If effect param present, switch to it?
      if (effect_found) {
        app_led_set_effect(effect);
      } else {
        // Get current effect to modify it
        uint8_t dummy1, dummy2, dummy3, dummy4, dummy5;
        app_led_get_config(&dummy1, &dummy2, &dummy3, &effect, &dummy4,
                           &dummy5);
      }

      // Now set Speed for this effect
      if (httpd_query_key_value(buf, "speed", param, sizeof(param)) == ESP_OK) {
        // Since we potentially switched effect above, set_speed works on
        // current. BUT app_led_set_speed sets current.
        app_led_set_speed(atoi(param));
      }

      // Colors
      if (httpd_query_key_value(buf, "index", param, sizeof(param)) == ESP_OK) {
        index = atoi(param); // 0-7, or 255 for all
        if (index < 0)
          index = 255;
      }

      bool c_set = false;
      if (httpd_query_key_value(buf, "r", param, sizeof(param)) == ESP_OK) {
        r = atoi(param);
        c_set = true;
      }
      if (httpd_query_key_value(buf, "g", param, sizeof(param)) == ESP_OK) {
        g = atoi(param);
        c_set = true;
      }
      if (httpd_query_key_value(buf, "b", param, sizeof(param)) == ESP_OK) {
        b = atoi(param);
        c_set = true;
      }

      if (c_set && index != -1) {
        app_led_set_config(effect, index, r, g, b);
      }

      httpd_resp_send(req, "Saved", HTTPD_RESP_USE_STRLEN);
    } else {
      httpd_resp_send_500(req);
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
  return ESP_OK;
}

static esp_err_t api_led_save_preset_handler(httpd_req_t *req) {
  if (app_led_save_settings() == ESP_OK) {
    httpd_resp_send(req, "Saved", 5);
  } else {
    httpd_resp_send_500(req);
  }
  return ESP_OK;
}

static esp_err_t api_led_state_config_get_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateArray();
  // Expose specific states user might want to configure
  app_led_state_t states[] = {
      APP_LED_WIFI_PROV, APP_LED_WIFI_CONN, APP_LED_OTA,       APP_LED_IR_TX,
      APP_LED_IR_LEARN,  APP_LED_IR_FAIL,   APP_LED_IR_SUCCESS};
  const char *names[] = {"WiFi Prov", "WiFi Conn", "OTA",       "IR TX",
                         "IR Learn",  "IR Fail",   "IR Success"};

  for (int i = 0; i < 7; i++) {
    uint8_t r, g, b;
    app_led_get_state_color(states[i], &r, &g, &b);
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "id", states[i]);
    cJSON_AddStringToObject(item, "name", names[i]);
    cJSON_AddNumberToObject(item, "r", r);
    cJSON_AddNumberToObject(item, "g", g);
    cJSON_AddNumberToObject(item, "b", b);
    cJSON_AddItemToArray(root, item);
  }

  char *json_str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
  cJSON_Delete(root);
  free(json_str);
  return ESP_OK;
}

static esp_err_t api_led_state_config_post_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char param[32] = {0};
  int id = -1;
  uint8_t r = 0, g = 0, b = 0;

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "id", param, sizeof(param)) == ESP_OK)
        id = atoi(param);
      if (httpd_query_key_value(buf, "r", param, sizeof(param)) == ESP_OK)
        r = atoi(param);
      if (httpd_query_key_value(buf, "g", param, sizeof(param)) == ESP_OK)
        g = atoi(param);
      if (httpd_query_key_value(buf, "b", param, sizeof(param)) == ESP_OK)
        b = atoi(param);

      if (id >= 0) {
        app_led_set_state_color((app_led_state_t)id, r, g, b);
        httpd_resp_send(req, "Saved", HTTPD_RESP_USE_STRLEN);
      } else {
        httpd_resp_send_500(req);
      }
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
  }
  return ESP_OK;
}

static esp_err_t api_system_logs_handler(httpd_req_t *req) {
  // Debug: Print to UART to confirm handler is called
  ESP_LOGI(TAG, "API: Fetch Logs");

  char *buf = malloc(4096);
  if (buf) {
    int len = app_log_get_buffer(buf, 4096);
    if (len > 0) {
      httpd_resp_set_type(req, "text/plain");
      httpd_resp_send(req, buf, len);
    } else {
      httpd_resp_set_type(req, "text/plain");
      httpd_resp_send(req, "No logs available...", HTTPD_RESP_USE_STRLEN);
    }
    free(buf);
  } else {
    httpd_resp_send_500(req);
  }
  return ESP_OK;
}

static esp_err_t api_system_logs_clear_handler(httpd_req_t *req) {
  app_log_clear();
  httpd_resp_send(req, "Cleared", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static temperature_sensor_handle_t temp_sensor = NULL;

static esp_err_t api_system_stats_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "API Stats Requested");
  cJSON *root = cJSON_CreateObject();

  // Uptime
  int64_t uptime_us = esp_timer_get_time();
  cJSON_AddNumberToObject(root, "uptime", uptime_us / 1000000);

  // Heap
  cJSON_AddNumberToObject(root, "free_heap", app_mem_get_free_internal());
  cJSON_AddNumberToObject(root, "min_free_heap",
                          esp_get_minimum_free_heap_size());

  // PSRAM
  multi_heap_info_t psram_info;
  heap_caps_get_info(&psram_info, MALLOC_CAP_SPIRAM);
  cJSON_AddNumberToObject(root, "psram_free",
                          (double)psram_info.total_free_bytes);
  cJSON_AddNumberToObject(
      root, "psram_total",
      (double)(psram_info.total_free_bytes + psram_info.total_allocated_bytes));

  // Temperature (S3 internal)
  float tsens_out = 0;
  if (!temp_sensor) {
    temperature_sensor_config_t temp_sensor_config =
        TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100);
    if (temperature_sensor_install(&temp_sensor_config, &temp_sensor) ==
        ESP_OK) {
      temperature_sensor_enable(temp_sensor);
    }
  }
  if (temp_sensor) {
    temperature_sensor_get_celsius(temp_sensor, &tsens_out);
  }
  cJSON_AddNumberToObject(root, "temp", tsens_out);

  // WiFi RSSI
  wifi_ap_record_t ap_info;
  if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
    cJSON_AddNumberToObject(root, "rssi", ap_info.rssi);
    cJSON_AddStringToObject(root, "ssid", (char *)ap_info.ssid);
  } else {
    cJSON_AddNumberToObject(root, "rssi", 0);
    cJSON_AddStringToObject(root, "ssid", "Disconnected");
  }

  // Version
#ifdef PROJECT_VERSION
  cJSON_AddStringToObject(root, "version", PROJECT_VERSION);
#else
  cJSON_AddStringToObject(root, "version", "1.0.0");
#endif

  const char *json_str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
  cJSON_Delete(root);
  free((void *)json_str);
  return ESP_OK;
}

static const httpd_uri_t root = {
    .uri = "/", .method = HTTP_GET, .handler = root_get_handler};
static const httpd_uri_t ir_list = {
    .uri = "/api/ir/list", .method = HTTP_GET, .handler = api_ir_list_handler};
static const httpd_uri_t learn_start = {.uri = "/api/learn/start",
                                        .method = HTTP_POST,
                                        .handler = api_learn_start_handler};
static const httpd_uri_t learn_stop = {.uri = "/api/learn/stop",
                                       .method = HTTP_POST,
                                       .handler = api_learn_stop_handler};
static const httpd_uri_t save_key = {
    .uri = "/api/save", .method = HTTP_POST, .handler = api_save_handler};
static const httpd_uri_t send_key = {
    .uri = "/api/send", .method = HTTP_POST, .handler = api_send_handler};
static const httpd_uri_t delete_key = {.uri = "/api/ir/delete",
                                       .method = HTTP_POST,
                                       .handler = api_delete_handler};
static const httpd_uri_t rename_key = {.uri = "/api/ir/rename",
                                       .method = HTTP_POST,
                                       .handler = api_rename_handler};
// The following OTA URI definitions and registrations are moved to app_web_init
// to allow for inline definition and registration as per the instruction.
// static const httpd_uri_t ota_check = {.uri = "/api/ota/check",
//                                       .method = HTTP_GET,
//                                       .handler = api_ota_check_handler};
// static const httpd_uri_t ota_start = {.uri = "/api/ota/start",
//                                       .method = HTTP_POST,
//                                       .handler = api_ota_start_handler};
static const httpd_uri_t wifi_config = {.uri = "/api/wifi/config",
                                        .method = HTTP_POST,
                                        .handler = api_wifi_config_handler};
static const httpd_uri_t wifi_scan = {.uri = "/api/wifi/scan",
                                      .method = HTTP_GET,
                                      .handler = api_wifi_scan_handler};
static const httpd_uri_t led_config_get = {.uri = "/api/led/config",
                                           .method = HTTP_GET,
                                           .handler =
                                               api_led_config_get_handler};
static const httpd_uri_t led_config_post = {.uri = "/api/led/config",
                                            .method = HTTP_POST,
                                            .handler =
                                                api_led_config_post_handler};
static const httpd_uri_t led_state_get = {.uri = "/api/led/state-config",
                                          .method = HTTP_GET,
                                          .handler =
                                              api_led_state_config_get_handler};
static const httpd_uri_t led_state_post = {
    .uri = "/api/led/state-config",
    .method = HTTP_POST,
    .handler = api_led_state_config_post_handler};

static const httpd_uri_t system_logs = {.uri = "/api/system/logs",
                                        .method = HTTP_GET,
                                        .handler = api_system_logs_handler};
static const httpd_uri_t system_logs_clear = {
    .uri = "/api/system/logs/clear",
    .method = HTTP_POST,
    .handler = api_system_logs_clear_handler};

static const httpd_uri_t system_stats = {.uri = "/api/system/stats",
                                         .method = HTTP_GET,
                                         .handler = api_system_stats_handler};

esp_err_t app_web_init(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 25; // Increased to ensure all 19+ handlers register
  config.stack_size = 8192;

  ESP_LOGI(TAG, "Starting HTTP Server...");
  if (httpd_start(&server, &config) == ESP_OK) {
    int ret;
#define REG_URI(h)                                                             \
  if ((ret = httpd_register_uri_handler(server, h)) != ESP_OK)                 \
  ESP_LOGE(TAG, "Fail reg %s: %d", (h)->uri, ret)

    REG_URI(&system_stats); // Register first to ensure priority
    REG_URI(&root);
    REG_URI(&ir_list);
    REG_URI(&learn_start);
    REG_URI(&learn_stop);
    REG_URI(&save_key);
    REG_URI(&send_key);
    REG_URI(&delete_key);
    REG_URI(&rename_key);

    // OTA Handlers
    httpd_uri_t ota_check_uri = {.uri = "/api/ota/check",
                                 .method = HTTP_GET,
                                 .handler = api_ota_check_handler};
    if (httpd_register_uri_handler(server, &ota_check_uri) != ESP_OK)
      ESP_LOGE(TAG, "Fail reg ota_check");

    httpd_uri_t ota_start_uri = {.uri = "/api/ota/start",
                                 .method = HTTP_POST,
                                 .handler = api_ota_start_handler};
    if (httpd_register_uri_handler(server, &ota_start_uri) != ESP_OK)
      ESP_LOGE(TAG, "Fail reg ota_start");

    REG_URI(&wifi_config);
    REG_URI(&wifi_scan);
    REG_URI(&led_config_get);
    REG_URI(&led_config_post);

    httpd_uri_t led_save_preset = {.uri = "/api/led/save-preset",
                                   .method = HTTP_POST,
                                   .handler = api_led_save_preset_handler,
                                   .user_ctx = NULL};
    REG_URI(&led_save_preset);

    REG_URI(&led_state_get);
    REG_URI(&led_state_post);
    REG_URI(&system_logs);
    REG_URI(&system_logs_clear);

    return ESP_OK;
  } else {
    ESP_LOGE(TAG, "Error starting server!");
    return ESP_FAIL;
  }
}

void app_web_toggle_mode(void) {
  ESP_LOGI(TAG, "Web mode toggle requested (No-op in station mode)");
}
