<?php
require_once "/opt/fpp/www/common.php";
require_once "/opt/fpp/www/config.php";

$pluginName = "fpp-mask";
$sequenceDir = $settings['sequenceDirectory'] ?? '/home/fpp/media/sequences';
$sequences = [];
if (is_dir($sequenceDir)) {
    foreach (scandir($sequenceDir) as $f) {
        if (substr($f, -5) === '.fseq') $sequences[] = $f;
    }
    sort($sequences);
}
?>
<!DOCTYPE html>
<html>
<head>
    <title>Mask Plugin</title>
    <link rel="stylesheet" href="/css/fpp.css">
    <style>
        body { font-family: sans-serif; padding: 1em; }
        .row { margin-bottom: 1em; }
        button { padding: 0.5em 1em; margin-right: 0.5em; }
        #status { background: #222; color: #0f0; padding: 0.5em; font-family: monospace; white-space: pre; }
        select { padding: 0.4em; min-width: 300px; }
    </style>
</head>
<body>
    <h1>Brightness Mask</h1>

    <div class="row">
        <strong>State:</strong>
        <button onclick="apiCall('on')">Enable</button>
        <button onclick="apiCall('off')">Disable</button>
        <button onclick="apiCall('toggle')">Toggle</button>
    </div>

    <div class="row">
        <strong>Mask sequence:</strong><br>
        <select id="maskFile">
            <?php foreach ($sequences as $s): ?>
                <option value="<?= htmlspecialchars($s) ?>"><?= htmlspecialchars($s) ?></option>
            <?php endforeach; ?>
        </select>
        <button onclick="loadMask()">Load</button>
    </div>

    <div class="row">
        <strong>Status:</strong>
        <button onclick="refresh()">Refresh</button>
        <pre id="status">Loading...</pre>
    </div>

    <h2>REST API</h2>
    <ul>
        <li><code>GET /api/plugin-apis/Mask</code> — status</li>
        <li><code>GET /api/plugin-apis/Mask/on</code> — enable</li>
        <li><code>GET /api/plugin-apis/Mask/off</code> — disable</li>
        <li><code>GET /api/plugin-apis/Mask/toggle</code></li>
        <li><code>GET /api/plugin-apis/Mask/load/&lt;filename.fseq&gt;</code></li>
    </ul>

    <h2>MQTT topics (relative to your FPP MQTT prefix)</h2>
    <ul>
        <li><code>event/Mask/Set</code> — payload <code>on</code> or <code>off</code></li>
        <li><code>event/Mask/Toggle</code></li>
        <li><code>event/Mask/Load</code> — payload = filename.fseq</li>
    </ul>

    <script>
    async function apiCall(action) {
        const r = await fetch('/api/plugin-apis/Mask/' + action);
        document.getElementById('status').textContent = await r.text();
    }
    async function refresh() {
        const r = await fetch('/api/plugin-apis/Mask');
        const j = await r.json();
        document.getElementById('status').textContent = JSON.stringify(j, null, 2);
        if (j.maskFile) {
            const sel = document.getElementById('maskFile');
            for (const opt of sel.options) if (opt.value === j.maskFile) sel.value = j.maskFile;
        }
    }
    async function loadMask() {
        const fn = document.getElementById('maskFile').value;
        const r = await fetch('/api/plugin-apis/Mask/load/' + encodeURIComponent(fn));
        document.getElementById('status').textContent = await r.text();
    }
    refresh();
    </script>
</body>
</html>
