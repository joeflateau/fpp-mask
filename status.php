<?php
$kvFile = "/home/fpp/media/config/plugin.fpp-mask";
$sequenceDir = "/home/fpp/media/sequences";

function readSettings($file) {
    $s = [];
    if (file_exists($file)) {
        foreach (file($file) as $line) {
            $line = trim($line);
            if ($line === '' || $line[0] === '#') continue;
            $eq = strpos($line, '=');
            if ($eq === false) continue;
            $s[substr($line, 0, $eq)] = substr($line, $eq + 1);
        }
    }
    return $s;
}

function writeSettings($file, $settings) {
    $out = '';
    foreach ($settings as $k => $v) $out .= "$k=$v\n";
    file_put_contents($file, $out);
}

$saved = false;
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $current = readSettings($kvFile);
    $current['Enabled']  = isset($_POST['Enabled']) ? 'true' : 'false';
    $current['MaskFile'] = $_POST['MaskFile'] ?? '';
    writeSettings($kvFile, $current);
    $saved = true;
}

$current = readSettings($kvFile);

$sequences = [];
if (is_dir($sequenceDir)) {
    foreach (scandir($sequenceDir) as $f) {
        if (substr($f, -5) === '.fseq') $sequences[] = $f;
    }
    sort($sequences);
}
?>
<style>
    .mask-page { padding: 1em; max-width: 700px; }
    .mask-page .row { margin-bottom: 1em; }
    .mask-page select { padding: 0.4em; min-width: 320px; }
    .mask-page button { padding: 0.5em 1em; }
    .mask-page .saved { color: #2a7; font-weight: bold; }
    .mask-page code { background: #eee; padding: 1px 4px; }
</style>

<div class="mask-page">
    <h2>Brightness Mask</h2>

    <?php if ($saved): ?>
        <p class="saved">Saved. Plugin will pick up changes within a second.</p>
    <?php endif; ?>

    <form method="POST">
        <div class="row">
            <label>
                <input type="checkbox" name="Enabled" value="true"
                    <?= ($current['Enabled'] ?? '') === 'true' ? 'checked' : '' ?>>
                <strong>Mask enabled</strong>
            </label>
        </div>

        <div class="row">
            <label><strong>Mask sequence file:</strong><br>
                <select name="MaskFile">
                    <option value="">(none &mdash; passthrough)</option>
                    <?php foreach ($sequences as $s): ?>
                        <option value="<?= htmlspecialchars($s) ?>"
                            <?= ($current['MaskFile'] ?? '') === $s ? 'selected' : '' ?>>
                            <?= htmlspecialchars($s) ?>
                        </option>
                    <?php endforeach; ?>
                </select>
            </label>
        </div>

        <button type="submit">Save</button>
    </form>

    <h3>MQTT (Home Assistant)</h3>
    <p>Topics relative to your FPP MQTT prefix (Settings &rarr; MQTT):</p>
    <ul>
        <li><code>event/Mask/Set</code> &mdash; payload <code>on</code> or <code>off</code></li>
        <li><code>event/Mask/Toggle</code></li>
        <li><code>event/Mask/Load</code> &mdash; payload = filename.fseq</li>
    </ul>

    <h3>Current settings file</h3>
    <pre><?= htmlspecialchars(file_exists($kvFile) ? file_get_contents($kvFile) : '(empty)') ?></pre>
</div>
