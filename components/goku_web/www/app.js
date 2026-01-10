const statusEl = document.getElementById('learnStatus');
const keyListEl = document.getElementById('keyList');

// Custom Brands State
let customBrands = [];

// Debug Mode State
let debugMode = localStorage.getItem('debugMode') === 'true';
let logoClickCount = 0;
let logoClickTimer = null;

function toggleSidebar() {
    document.getElementById('sidebar').classList.toggle('active');
    document.getElementById('overlay').classList.toggle('active');
}

window.onload = function () {
    // Apply debug mode if enabled
    if (debugMode) {
        document.body.classList.add('debug-mode');
        document.getElementById('debugBadge').classList.remove('hidden');
    }

    navTo('dashboard');
    checkUpdate();
    setInterval(() => { if (!document.getElementById('dashboard').classList.contains('hidden')) updateDashboard(); }, 5000);
    setTimeout(initCharts, 500);
    document.getElementById('dashStatus').innerText = 'JS Active';
};

function navTo(id) {
    if (window.innerWidth <= 768) toggleSidebar();
    document.querySelectorAll('.view').forEach(el => el.classList.add('hidden'));
    const el = document.getElementById(id);
    if (el) el.classList.remove('hidden');
    if (id === 'dashboard') updateDashboard();
    if (id === 'controls' || id === 'keys') {
        fetchKeys();
        fetchACState();
        loadACBrands();  // Load custom brands for AC Control
    }
    if (id === 'logs') fetchLogs();
    if (id === 'led') {
        initRing(); // Initialize LED ring pixels
        fetchLedConfig();
        fetchSystemColors();
    }
    if (id === 'learning') {
        fetchCustomBrands(); // Load custom brands
        loadACBrands();      // Load custom brands for IR Learning dropdown
    }
}

async function fetchLogs() {
    try {
        const res = await fetch('/api/system/logs');
        if (!res.ok) throw new Error('Failed');
        const text = await res.text();
        document.getElementById('logViewer').innerText = text || 'No logs available.';
    } catch (e) {
        document.getElementById('logViewer').innerText = 'Failed to load logs.';
    }
}

let acPower = false;
async function fetchACState() {
    try {
        const res = await fetch('/api/ac/state');
        const data = await res.json();
        acPower = data.power;
        document.getElementById('modeSelect').value = data.mode;
        document.getElementById('tempSelect').value = data.temp;
        document.getElementById('fanSelect').value = data.fan;
        document.getElementById('brandSelect').value = data.brand;
        updatePowerUI();
    } catch (e) { }
}

function updatePowerUI() {
    if (acPower) {
        document.getElementById('btnOn').style.background = '#22c55e';
        document.getElementById('btnOn').style.color = '#fff';
        document.getElementById('btnOff').style.background = '#334155';
        document.getElementById('btnOff').style.color = '#94a3b8';
    } else {
        document.getElementById('btnOff').style.background = '#ef4444';
        document.getElementById('btnOff').style.color = '#fff';
        document.getElementById('btnOn').style.background = '#334155';
        document.getElementById('btnOn').style.color = '#94a3b8';
    }
}

function setPower(state) {
    acPower = state;
    updatePowerUI();
    saveAC();
}

async function saveAC() {
    const brandValue = document.getElementById('brandSelect').value;
    let brand;

    // Check if custom brand (starts with 'custom_')
    if (brandValue.startsWith('custom_')) {
        // Custom brand - send as string without prefix
        brand = brandValue.substring(7);  // Remove 'custom_' prefix
    } else {
        // Preset brand - send as integer (backward compatibility)
        brand = parseInt(brandValue);
    }

    const body = {
        power: acPower ? 1 : 0,
        mode: parseInt(document.getElementById('modeSelect').value),
        temp: parseInt(document.getElementById('tempSelect').value),
        fan: parseInt(document.getElementById('fanSelect').value),
        brand: brand  // Can be int (preset) or string (custom)
    };
    try {
        await fetch('/api/ac/control', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body)
        });
    } catch (e) { alert('Error sending command'); }
}

async function fetchKeys() {
    try {
        const res = await fetch('/api/ir/list');
        if (!res.ok) throw new Error('Failed');
        const keys = await res.json();
        let html = '';
        if (keys.length === 0) html = '<p>No saved keys.</p>';
        keys.forEach(k => {
            html += `<div style='display:flex;justify-content:space-between;align-items:center;background:#0f172a;padding:10px;margin-bottom:5px;border-radius:8px'>`
                + `<span>${k}</span>`
                + `<div><button class='btn' style='padding:5px 10px;margin-right:5px;font-size:0.8rem' onclick="sendKey('${k}')">Send</button>`
                + `<button class='btn' style='padding:5px 10px;background:#ef4444;font-size:0.8rem' onclick="deleteKey('${k}')">Del</button></div></div>`;
        });
        document.getElementById('keyList').innerHTML = html;
    } catch (e) {
        document.getElementById('keyList').innerHTML = 'Failed to load keys.';
    }
}

async function fetchSystemColors() {
    const div = document.getElementById('systemColorsList');
    if (!div) return;
    div.innerHTML = 'Loading...';
    try {
        const res = await fetch('/api/led/state-config');
        const data = await res.json();
        div.innerHTML = '';
        data.forEach(item => {
            const row = document.createElement('div');
            row.className = 'row';
            row.innerHTML = `<label style='width: 40%'>${item.name}</label>` +
                `<input type='color' value='${rgbToHex(item.r, item.g, item.b)}' ` +
                `onchange='saveSystemColor(${item.id}, this.value)'>`;
            div.appendChild(row);
        });
    } catch (e) { div.innerHTML = 'Error loading colors'; }
}

async function saveSystemColor(id, hex) {
    const c = hexToRgb(hex);
    try {
        await fetch(`/api/led/state-config?id=${id}&r=${c.r}&g=${c.g}&b=${c.b}`, { method: 'POST' });
    } catch (e) { alert('Failed to save color'); }
}

async function fetchList() {
    try {
        const res = await fetch('/api/ir/list');
        const keys = await res.json(); renderList(keys);
    }
    catch (e) { }
}

function renderList(keys) {
    keyListEl.innerHTML = '';
    if (keys.length === 0) { keyListEl.innerHTML = '<div class="status">No keys saved</div>'; return; }
    keys.forEach(key => {
        const div = document.createElement('div');
        div.className = 'key-item';
        div.innerHTML = `<div><strong>${key}</strong></div><div class='key-actions'>`
            + `<button class='btn btn-secondary' onclick="sendKey('${key}')">Test</button>`
            + `<button class='btn btn-danger' onclick="deleteKey('${key}')">Del</button>`
            + `</div>`;
        keyListEl.appendChild(div);
    });
}

let learnInterval = null;

async function startLearn() {
    await fetch('/api/learn/start', { method: 'POST' });
    statusEl.innerText = 'Listening... Press remote button';
    statusEl.style.color = '#fbbf24';
    document.getElementById('saveForm').classList.add('hidden');
    if (learnInterval) clearInterval(learnInterval);
    learnInterval = setInterval(checkLearnStatus, 1000);
}

async function stopLearn() {
    await fetch('/api/learn/stop', { method: 'POST' });
    statusEl.innerText = 'Stopped';
    statusEl.style.color = '#94a3b8';
    if (learnInterval) clearInterval(learnInterval);
}

async function checkLearnStatus() {
    try {
        const res = await fetch('/api/learn/status');
        const data = await res.json();
        if (data.captured > 0) {
            statusEl.innerText = 'Signal Captured! (' + data.captured + ' symbols)';
            statusEl.style.color = '#22c55e';
            document.getElementById('saveForm').classList.remove('hidden');
            if (!data.learning) clearInterval(learnInterval);
        } else if (!data.learning) {
            statusEl.innerText = 'Stopped (No signal)';
            clearInterval(learnInterval);
        }
    } catch (e) { }
}

async function saveLearnedKey() {
    const name = document.getElementById('keyNameInput').value;
    if (!name) { alert('Enter a key name'); return; }
    const res = await fetch('/api/save?key=' + encodeURIComponent(name), { method: 'POST' });
    if (res.ok) {
        alert('Saved!');
        fetchList();
        document.getElementById('keyNameInput').value = '';
        document.getElementById('saveForm').classList.add('hidden');
    } else { alert('Failed to save'); }
}

function genKeyName() {
    const bCode = { 'daikin': 'dk', 'samsung': 'ss', 'mitsubishi': 'mt' };
    const mCode = { 'auto': 'a', 'cool': 'c', 'heat': 'h', 'fan': 'f', 'dry': 'd' };
    const fCode = { 'auto': 'a', 'low': 'l', 'medium': 'm', 'high': 'h' };

    const brandValue = document.getElementById('learnBrand').value;
    // Use preset code if available, otherwise use custom brand name directly
    const b = bCode[brandValue] || brandValue.replace(/\s+/g, '_');  // Replace spaces with underscores

    const m = mCode[document.getElementById('learnMode').value] || '0';
    const t = document.getElementById('learnTemp').value;
    const f = fCode[document.getElementById('learnFan').value] || '0';
    const name = `${b}_${m}${t}_${f}`;
    document.getElementById('keyNameInput').value = name;
}

async function updateAC() {
    const brandValue = document.getElementById('brandSelect').value;
    let brand;

    // Check if custom brand
    if (brandValue.startsWith('custom_')) {
        brand = brandValue.substring(7);  // Remove prefix → string
    } else {
        brand = parseInt(brandValue);     // Keep as int
    }

    const data = {
        power: acPower ? 1 : 0,
        mode: parseInt(document.getElementById('modeSelect').value),
        temp: parseInt(document.getElementById('tempSelect').value),
        fan: parseInt(document.getElementById('fanSelect').value),
        brand: brand
    };
    try {
        const res = await fetch('/api/ac/control', {
            method: 'POST',
            body: JSON.stringify(data)
        });
        if (!res.ok) alert('Failed to set AC');
    } catch (e) { alert('Error setting AC'); }
}

function saveMode() { updateAC(); }
function saveFan() { updateAC(); }
function saveTemp() { updateAC(); }
function saveBrand() { updateAC(); }
function setPower(p) { acPower = p; updatePowerUI(); updateAC(); }

async function sendKey(key) {
    await fetch('/api/send?key=' + encodeURIComponent(key), { method: 'POST' });
}

async function deleteKey(key) {
    if (!confirm('Delete ' + key + '?')) return;
    await fetch('/api/ir/delete?key=' + encodeURIComponent(key), { method: 'POST' });
    fetchList();
}

async function checkUpdate() {
    const status = document.getElementById('otaStatus');
    status.innerText = 'Checking...';
    try {
        const res = await fetch('/api/ota/check');
        const data = await res.json();
        document.getElementById('currentVer').innerText = data.current;
        if (data.available) {
            document.getElementById('otaSection').classList.add('hidden');
            document.getElementById('updateAvailable').classList.remove('hidden');
            document.getElementById('newVer').innerText = data.latest;
            status.innerText = '';
        } else {
            status.innerText = 'System is up to date.';
        }
    } catch (e) { status.innerText = 'Error checking update'; }
}

async function startUpdate() {
    if (!confirm('Start Firmware Update? Device will reboot.')) return;
    const status = document.getElementById('otaStatus');
    status.innerText = 'Starting update...';
    try {
        const res = await fetch('/api/ota/start', { method: 'POST' });
        if (res.ok) status.innerText = 'Update started! Wait for reboot...';
        else status.innerText = 'Failed to start update';
    } catch (e) { status.innerText = 'Error starting update'; }
}

async function saveWifi() {
    const ssid = document.getElementById('wifiSSID').value;
    const pass = document.getElementById('wifiPass').value;
    if (!ssid) { alert('SSID is required'); return; }
    if (!confirm('Save credentials and restart device?')) return;

    try {
        const res = await fetch('/api/wifi/config?ssid=' + encodeURIComponent(ssid) + '&password=' + encodeURIComponent(pass), { method: 'POST' });
        if (res.ok) alert('Settings saved. Device rebooting...');
        else alert('Failed to save settings');
    } catch (e) { alert('Error: ' + e); }
}

async function scanWifi() {
    const btn = document.getElementById('scanBtn');
    const list = document.getElementById('scanList');
    btn.disabled = true; btn.innerText = 'Scanning...';
    list.innerHTML = ''; list.classList.remove('hidden');
    try {
        const res = await fetch('/api/wifi/scan');
        const data = await res.json();
        if (data.length === 0) list.innerHTML = '<div class="status">No networks found</div>';
        data.forEach(net => {
            const div = document.createElement('div');
            div.className = 'key-item';
            div.style.cursor = 'pointer';
            div.innerHTML = `<div><strong>${net.ssid}</strong> <small>(${net.rssi}dBm)</small></div>`;
            div.onclick = () => { document.getElementById('wifiSSID').value = net.ssid; list.classList.add('hidden'); };
            list.appendChild(div);
        });
    } catch (e) { list.innerHTML = '<div class="status">Scan failed</div>'; }
    btn.disabled = false; btn.innerText = 'Scan';
}

function hexToRgb(hex) {
    var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
        r: parseInt(result[1], 16),
        g: parseInt(result[2], 16),
        b: parseInt(result[3], 16)
    } : null;
}

function componentToHex(c) { var hex = c.toString(16); return hex.length == 1 ? '0' + hex : hex; }
function rgbToHex(r, g, b) { return '#' + componentToHex(r) + componentToHex(g) + componentToHex(b); }

let ledColors = [];
let selectedLed = -1;
let currentEffect = 'static';

function initRing() {
    const ring = document.getElementById('ledRing');
    ring.innerHTML = '';
    const radius = 120; // Increased radius for 280px ring
    for (let i = 0; i < 8; i++) {
        const div = document.createElement('div');
        div.className = 'led-pixel';
        const angle = (i * 360 / 8) - 90;
        const rad = angle * Math.PI / 180;
        const x = Math.round(radius * Math.cos(rad));
        const y = Math.round(radius * Math.sin(rad));
        div.style.transform = `translate(${x}px, ${y}px)`;
        div.onclick = (e) => selectLed(i, e);
        div.id = 'led-' + i;
        ring.appendChild(div);
    }
}

function selectLed(i, e) {
    if (e) e.stopPropagation();
    selectedLed = i;
    document.querySelectorAll('.led-pixel').forEach(el => el.classList.remove('selected'));
    document.getElementById('led-' + i).classList.add('selected');
}

function updateRingUI() {
    ledColors.forEach((c, i) => {
        const el = document.getElementById('led-' + i);
        if (el) el.style.backgroundColor = `rgb(${c[0]}, ${c[1]}, ${c[2]})`;
    });
}

async function setTab(effect, btn) {
    currentEffect = effect;
    document.querySelectorAll('.effect-tab').forEach(b => b.classList.remove('active'));
    if (btn) btn.classList.add('active');
    await saveLedConfig(); // Save the new effect to server
}

async function applyColor(hex) {
    if (!hex) hex = document.getElementById('customColor').value;
    const c = hexToRgb(hex);
    if (selectedLed !== -1) {
        ledColors[selectedLed] = [c.r, c.g, c.b];
        updateRingUI();
        await fetch(`/api/led/config?effect=${currentEffect}&index=${selectedLed}&r=${c.r}&g=${c.g}&b=${c.b}`, { method: 'POST' });
    }
}

async function applyToAll() {
    const hex = document.getElementById('customColor').value;
    const c = hexToRgb(hex);
    for (let i = 0; i < 8; i++) ledColors[i] = [c.r, c.g, c.b];
    updateRingUI();
    await fetch(`/api/led/config?effect=${currentEffect}&index=255&r=${c.r}&g=${c.g}&b=${c.b}`, { method: 'POST' });
}

async function saveLedConfig() {
    const speed = document.getElementById('ledSpeed').value;
    const bright = document.getElementById('ledBright').value;
    document.getElementById('valSpeed').innerText = speed;
    document.getElementById('valBright').innerText = bright;
    await fetch(`/api/led/config?effect=${currentEffect}&speed=${speed}&brightness=${bright}`, { method: 'POST' });
}

async function savePreset() {
    try {
        const res = await fetch('/api/led/save_preset', { method: 'POST' });
        if (res.ok) {
            alert('LED preset saved to device!');
        } else {
            alert('Failed to save preset');
        }
    } catch (e) {
        alert('Error saving preset: ' + e.message);
    }
}

async function fetchLedConfig() {
    try {
        const res = await fetch('/api/led/config');
        const data = await res.json();
        currentEffect = data.effect;
        document.getElementById('ledSpeed').value = data.speed;
        document.getElementById('valSpeed').innerText = data.speed;
        document.getElementById('ledBright').value = data.brightness;
        document.getElementById('valBright').innerText = data.brightness;

        // Update tabs
        document.querySelectorAll('.effect-tab').forEach(b => {
            b.classList.remove('active');
            if (b.innerText.toLowerCase().includes(currentEffect.replace('_', ' '))) {
                b.classList.add('active');
            }
        });

        if (data.colors) {
            ledColors = data.colors;
            updateRingUI();
        }
    } catch (e) { }
}

async function updateDashboard() {
    try {
        const res = await fetch('/api/system/stats');
        const d = await res.json();

        // Always visible metrics
        document.getElementById('valTemp').innerText = d.temp.toFixed(1) + '\u00B0C';
        document.getElementById('progTemp').style.width = Math.min(d.temp, 100) + '%';
        document.getElementById('valRam').innerText = d.ram + '%';
        document.getElementById('progRam').style.width = d.ram + '%';

        // Simplified uptime (debug shows seconds)
        const hours = Math.floor(d.uptime / 3600);
        const mins = Math.floor((d.uptime % 3600) / 60);
        const secs = d.uptime % 60;
        document.getElementById('txtUptime').innerText = debugMode
            ? `Uptime: ${hours}h ${mins}m ${secs}s`
            : `Uptime: ${hours}h ${mins}m`;

        // Firmware version
        if (document.getElementById('txtFirmware')) {
            document.getElementById('txtFirmware').innerText = d.version || '1.0.0';
        }

        // LED Visibility
        const ledNav = document.getElementById('nav-led');
        if (ledNav) {
            if (d.led_enabled === false) {
                ledNav.classList.add('hidden');
                // If currently on LED page, redirect
                if (!document.getElementById('led').classList.contains('hidden')) {
                    navTo('dashboard');
                }
            } else {
                ledNav.classList.remove('hidden');
            }
        }

        // Debug mode only
        if (debugMode && document.getElementById('valCpu')) {
            document.getElementById('valCpu').innerText = 'N/A';
            document.getElementById('valPsram').innerText = 'N/A';

            if (document.getElementById('txtFreeHeap')) {
                document.getElementById('txtFreeHeap').innerText = (d.free_heap / 1024).toFixed(1) + ' KB';
            }
            if (document.getElementById('txtMinHeap')) {
                document.getElementById('txtMinHeap').innerText = (d.min_free_heap / 1024).toFixed(1) + ' KB';
            }
        }

        // Update charts
        // Heap chart (debug only)
        if (debugMode && window.heapChartInst) {
            const now = new Date().toLocaleTimeString();
            if (window.heapChartInst.data.labels.length > 20) {
                window.heapChartInst.data.labels.shift();
                window.heapChartInst.data.datasets[0].data.shift();
            }
            window.heapChartInst.data.labels.push(now);
            window.heapChartInst.data.datasets[0].data.push(d.ram);
            window.heapChartInst.update();
        }

        // RSSI chart (always visible)
        if (window.rssiChartInst) {
            const now = new Date().toLocaleTimeString();
            if (window.rssiChartInst.data.labels.length > 20) {
                window.rssiChartInst.data.labels.shift();
                window.rssiChartInst.data.datasets[0].data.shift();
            }
            window.rssiChartInst.data.labels.push(now);
            window.rssiChartInst.data.datasets[0].data.push(d.rssi);
            window.rssiChartInst.update();
        }

    } catch (e) { }
}

var heapCtx, rssiCtx;
function initCharts() {
    heapCtx = document.getElementById('heapChart').getContext('2d');
    rssiCtx = document.getElementById('rssiChart').getContext('2d');

    window.heapChartInst = new Chart(heapCtx, {
        type: 'line',
        data: { labels: [], datasets: [{ label: 'RAM Usage (%)', data: [], borderColor: '#3b82f6', tension: 0.4 }] },
        options: { responsive: true, maintainAspectRatio: false, scales: { y: { min: 0, max: 100 } } }
    });

    window.rssiChartInst = new Chart(rssiCtx, {
        type: 'line',
        data: { labels: [], datasets: [{ label: 'Wi-Fi Signal (dBm)', data: [], borderColor: '#22c55e', tension: 0.4 }] },
        options: { responsive: true, maintainAspectRatio: false }
    });
}

// ========== Custom Brand Management Functions ==========

async function fetchCustomBrands() {
    try {
        const res = await fetch('/api/brand/list');
        if (!res.ok) throw new Error('Failed');
        customBrands = await res.json();
        updateBrandDropdown();
    } catch (e) {
        console.error('Failed to fetch custom brands:', e);
        customBrands = [];
        updateBrandDropdown();
    }
}

function updateBrandDropdown() {
    const group = document.getElementById('customBrandsGroup');
    if (!group) return;

    group.innerHTML = '';
    customBrands.forEach(brand => {
        const opt = document.createElement('option');
        opt.value = brand.toLowerCase().replace(/ /g, '_');
        opt.textContent = brand;
        group.appendChild(opt);
    });

    group.style.display = customBrands.length > 0 ? '' : 'none';
}

function openBrandManager() {
    document.getElementById('brandModal').classList.remove('hidden');
    renderCustomBrandsList();
}

function closeBrandManager() {
    document.getElementById('brandModal').classList.add('hidden');
    document.getElementById('newBrandName').value = '';
}

function renderCustomBrandsList() {
    const list = document.getElementById('customBrandsList');
    if (customBrands.length === 0) {
        list.innerHTML = '<p class="status">No custom brands yet</p>';
        return;
    }

    list.innerHTML = customBrands.map(brand => {
        const displayName = decodeURIComponent(brand);
        return `
        <div class='brand-item'>
            <span>${displayName}</span>
            <div class='brand-item-actions'>
                <button class='btn btn-secondary' style='padding:6px 12px;font-size:0.85rem;min-height:auto' 
                        onclick="renameBrand('${displayName}')">Rename</button>
                <button class='btn btn-danger' style='padding:6px 12px;font-size:0.85rem;min-height:auto' 
                        onclick="deleteBrand('${displayName}')">Delete</button>
            </div>
        </div>
    `}).join('');
}

async function addCustomBrand() {
    const input = document.getElementById('newBrandName');
    const name = input.value.trim();

    if (!name) {
        alert('Please enter a brand name');
        return;
    }

    if (!/^[a-z A-Z0-9 -]+$/.test(name)) {
        alert('Brand name can only contain letters, numbers, spaces, and dashes');
        return;
    }

    try {
        const res = await fetch(`/api/brand/add?name=${encodeURIComponent(name)}`, {
            method: 'POST'
        });

        if (!res.ok) {
            const error = await res.text();
            throw new Error(error || 'Failed to add brand');
        }

        input.value = '';
        await fetchCustomBrands();
        renderCustomBrandsList();
        alert(`Custom brand "${name}" added!`);
    } catch (e) {
        alert('Error: ' + e.message);
    }
}

async function renameBrand(oldName) {
    const newName = prompt(`Rename brand "${oldName}" to:`, oldName);
    if (!newName || newName.trim() === oldName) return;

    if (!/^[a-zA-Z0-9 -]+$/.test(newName.trim())) {
        alert('Brand name can only contain letters, numbers, spaces, and dashes');
        return;
    }

    try {
        const res = await fetch(
            `/api/brand/rename?old=${encodeURIComponent(oldName)}&new=${encodeURIComponent(newName.trim())}`,
            { method: 'POST' }
        );

        if (!res.ok) {
            const error = await res.text();
            throw new Error(error || 'Failed');
        }

        await fetchCustomBrands();
        renderCustomBrandsList();
        alert(`Renamed to "${newName}"`);
    } catch (e) {
        alert('Error: ' + e.message);
    }
}

async function deleteBrand(name) {
    if (!confirm(`Delete custom brand "${name}"?\n\nThis will NOT delete saved IR keys.`)) {
        return;
    }

    try {
        const res = await fetch(`/api/brand/delete?name=${encodeURIComponent(name)}`, {
            method: 'POST'
        });

        if (!res.ok) {
            const error = await res.text();
            throw new Error(error || 'Failed');
        }

        await fetchCustomBrands();
        renderCustomBrandsList();
        alert(`Brand "${name}" deleted`);
    } catch (e) {
        alert('Error: ' + e.message);
    }
}
// ========== Debug Mode Functions ==========

function handleLogoClick() {
    logoClickCount++;

    if (logoClickTimer) clearTimeout(logoClickTimer);

    if (logoClickCount === 3) {
        toggleDebugMode();
        logoClickCount = 0;
    } else {
        logoClickTimer = setTimeout(() => {
            logoClickCount = 0;
        }, 500);
    }
}

function toggleDebugMode() {
    debugMode = !debugMode;
    localStorage.setItem('debugMode', debugMode);

    if (debugMode) {
        document.body.classList.add('debug-mode');
        document.getElementById('debugBadge').classList.remove('hidden');
        alert('✅ Debug Mode: ON\nAdvanced metrics are now visible.');
    } else {
        document.body.classList.remove('debug-mode');
        document.getElementById('debugBadge').classList.add('hidden');
        alert('❌ Debug Mode: OFF\nReturned to user-friendly view.');
    }

    // Refresh dashboard to show/hide debug info
    updateDashboard();
}

// ========== AC Control - Custom Brand Integration ==========

async function loadACBrands() {
    try {
        const res = await fetch('/api/brand/list');
        const brands = await res.json();

        // Populate AC Control dropdown
        const acCustomGroup = document.getElementById('customBrandsGroup');
        if (acCustomGroup) {
            if (brands && brands.length > 0) {
                acCustomGroup.style.display = '';
                acCustomGroup.innerHTML = '';

                brands.forEach(brand => {
                    const option = document.createElement('option');
                    option.value = 'custom_' + brand;  // Prefix for AC Control
                    option.textContent = '🔧 ' + decodeURIComponent(brand);  // Icon + Decoded Name
                    acCustomGroup.appendChild(option);
                });
            } else {
                acCustomGroup.style.display = 'none';
            }
        }

        // Populate IR Learning dropdown
        const learnCustomGroup = document.getElementById('learnCustomBrandsGroup');
        if (learnCustomGroup) {
            if (brands && brands.length > 0) {
                learnCustomGroup.style.display = '';
                learnCustomGroup.innerHTML = '';

                brands.forEach(brand => {
                    const option = document.createElement('option');
                    option.value = brand;
                    option.textContent = decodeURIComponent(brand);
                    learnCustomGroup.appendChild(option);
                });
            } else {
                learnCustomGroup.style.display = 'none';
            }
        }
    } catch (e) {
        console.error('Failed to load custom brands:', e);
    }
}

// ========== Toast Logic ==========
function showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    if (!container) return; // Should exist if HTML updated

    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.innerHTML = `
        <span>${type === 'success' ? '✅' : type === 'error' ? '❌' : 'ℹ️'}</span>
        <span>${message}</span>
    `;

    container.appendChild(toast);

    // Auto dismiss
    setTimeout(() => {
        toast.classList.add('hide');
        toast.addEventListener('animationend', () => toast.remove());
    }, 3000);
}

// Override default alerts for better UX
window.alert = (msg) => {
    // If msg is obviously success, show nice green toast
    let type = 'info';
    const lower = msg.toLowerCase();
    if (lower.includes('success') || lower.includes('saved') || lower.includes('added') || lower.includes('renamed') || lower.includes('deleted')) {
        type = 'success';
    } else if (lower.includes('fail') || lower.includes('error')) {
        type = 'error';
    }
    showToast(msg, type);
};

// ========== Brand Filter Logic ==========
function filterBrands(query) {
    const select = document.getElementById('brandSelect');
    if (!select) return;
    const term = query.toLowerCase();

    // We need to iterate OPTGROUPS then OPTIONS
    Array.from(select.getElementsByTagName('option')).forEach(opt => {
        const text = opt.textContent || opt.innerText;
        const match = text.toLowerCase().includes(term);
        // Toggle visibility
        if (match) {
            opt.style.display = '';
            opt.disabled = false;
        } else {
            opt.style.display = 'none';
            opt.disabled = true; // For mobile safari
        }
    });

    // Auto-select first visible
    if (select.selectedOptions[0].disabled) {
        const first = Array.from(select.options).find(o => !o.disabled);
        if (first) select.value = first.value;
    }
}
