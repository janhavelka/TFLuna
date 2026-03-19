#pragma once

static const char kIndexHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>TF-Luna Stability</title>
  <style>
    :root{--bg:#f4efe4;--panel:#fffaf1;--ink:#1f2937;--muted:#6b7280;--line:#d7c8ae;--accent:#9a3412;--ok:#0f766e;--bad:#b91c1c}
    *{box-sizing:border-box}body{margin:0;font:15px/1.4 "Segoe UI",system-ui,sans-serif;background:
    radial-gradient(circle at top left,#efe2c2 0,#f4efe4 40%,#eadfd1 100%);color:var(--ink)}
    .wrap{max-width:980px;margin:0 auto;padding:20px}
    .hero{display:grid;grid-template-columns:2fr 1fr;gap:16px;margin-bottom:16px}
    .card{background:rgba(255,250,241,.92);backdrop-filter:blur(6px);border:1px solid var(--line);border-radius:18px;padding:16px;box-shadow:0 10px 30px rgba(44,27,11,.08)}
    .eyebrow{font-size:12px;text-transform:uppercase;letter-spacing:.08em;color:var(--muted);margin-bottom:8px}
    .distance{font-size:72px;font-weight:700;line-height:1;color:var(--accent)}
    .sub{font-size:14px;color:var(--muted)}
    .pill{display:inline-block;padding:4px 9px;border-radius:999px;border:1px solid var(--line);font-size:12px;font-weight:700}
    .ok{color:var(--ok);border-color:rgba(15,118,110,.35);background:rgba(15,118,110,.08)}
    .bad{color:var(--bad);border-color:rgba(185,28,28,.28);background:rgba(185,28,28,.08)}
    .grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}
    .kv{display:grid;grid-template-columns:minmax(0,1fr) auto;gap:8px 12px;font:13px/1.4 Consolas,monospace}
    .kv div:nth-child(odd){color:var(--muted)}
    @media (max-width:720px){.hero,.grid{grid-template-columns:1fr}.distance{font-size:56px}}
  </style>
</head>
<body>
  <div class="wrap">
    <div class="hero">
      <section class="card">
        <div class="eyebrow">TF-Luna Live Reading</div>
        <div id="distance" class="distance">--</div>
        <div class="sub">
          <span id="valid" class="pill">waiting</span>
          <span id="time">time source: --</span>
        </div>
      </section>
      <section class="card">
        <div class="eyebrow">Health</div>
        <div class="kv">
          <div>Signal</div><div id="signal">--</div>
          <div>Strength</div><div id="strength">--</div>
          <div>Temp</div><div id="temp">--</div>
          <div>Frame Age</div><div id="age">--</div>
        </div>
      </section>
    </div>
    <div class="grid">
      <section class="card">
        <div class="eyebrow">Distance Stats</div>
        <div class="kv">
          <div>Valid Samples</div><div id="validCount">--</div>
          <div>Invalid Samples</div><div id="invalidCount">--</div>
          <div>Min</div><div id="min">--</div>
          <div>Max</div><div id="max">--</div>
          <div>Mean</div><div id="mean">--</div>
          <div>Stddev</div><div id="stddev">--</div>
          <div>Range</div><div id="range">--</div>
          <div>Checksum Errors</div><div id="checksum">--</div>
        </div>
      </section>
      <section class="card">
        <div class="eyebrow">Session</div>
        <div class="kv">
          <div>Logging</div><div id="logging">--</div>
          <div>Log File</div><div id="logFile">--</div>
          <div>Lines Written</div><div id="lines">--</div>
          <div>Dropped Lines</div><div id="dropped">--</div>
          <div>AP State</div><div id="apState">--</div>
          <div>Stations</div><div id="stations">--</div>
          <div>Web Clients</div><div id="clients">--</div>
          <div>WiFi Channel</div><div id="channel">--</div>
          <div>Clock Source</div><div id="clockSource">--</div>
          <div>Clock Text</div><div id="clockText">--</div>
        </div>
      </section>
    </div>
  </div>
  <script>
    const fmt=(value,suffix='')=>{
      if(value===null||value===undefined||Number.isNaN(Number(value))) return '--';
      return `${value}${suffix}`;
    };
    const setPill=(el,ok,text)=>{
      el.textContent=text;
      el.className=`pill ${ok?'ok':'bad'}`;
    };
    async function refresh(){
      try{
        const response=await fetch('/api/status',{cache:'no-store'});
        const data=await response.json();
        document.getElementById('distance').textContent=data.distance_cm===null?'--':`${data.distance_cm} cm`;
        setPill(document.getElementById('valid'),Boolean(data.signal_ok),Boolean(data.signal_ok)?'signal ok':'weak/invalid');
        document.getElementById('time').textContent=`time source: ${data.time_source} | ${data.timestamp}`;
        document.getElementById('signal').textContent=data.signal_ok?'ok':'bad';
        document.getElementById('strength').textContent=fmt(data.strength);
        document.getElementById('temp').textContent=fmt(data.temperature_c,' C');
        document.getElementById('age').textContent=fmt(data.frame_age_ms,' ms');
        document.getElementById('validCount').textContent=fmt(data.valid_sample_count);
        document.getElementById('invalidCount').textContent=fmt(data.invalid_sample_count);
        document.getElementById('min').textContent=fmt(data.min_distance_cm,' cm');
        document.getElementById('max').textContent=fmt(data.max_distance_cm,' cm');
        document.getElementById('mean').textContent=fmt(data.mean_distance_cm,' cm');
        document.getElementById('stddev').textContent=fmt(data.stddev_distance_cm,' cm');
        document.getElementById('range').textContent=fmt(data.distance_range_cm,' cm');
        document.getElementById('checksum').textContent=fmt(data.checksum_errors);
        document.getElementById('logging').textContent=data.logging_ok?'ok':'offline';
        document.getElementById('logFile').textContent=data.log_file||'--';
        document.getElementById('lines').textContent=fmt(data.log_lines_written);
        document.getElementById('dropped').textContent=fmt(data.log_dropped_lines);
        document.getElementById('apState').textContent=data.ap_running?'running':'off';
        document.getElementById('stations').textContent=fmt(data.stations);
        document.getElementById('clients').textContent=fmt(data.web_clients);
        document.getElementById('channel').textContent=fmt(data.wifi_channel);
        document.getElementById('clockSource').textContent=data.time_source||'--';
        document.getElementById('clockText').textContent=data.timestamp||'--';
      }catch(error){
        setPill(document.getElementById('valid'),false,'status fetch failed');
      }
    }
    refresh();
    setInterval(refresh,2000);
  </script>
</body>
</html>
)HTML";
