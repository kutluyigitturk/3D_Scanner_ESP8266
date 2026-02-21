const char* renderer = R"cthulhu(
<html>
  <head>
  <meta charset="utf-8">
  <title>3D Scanner</title>
  <script>

// ── Shaders ──────────────────────────────────────────────────
fragmentShaderCode =
  'varying mediump float d;' +
  'void main(void){' +
  '  gl_FragColor = vec4(1.0 - d, 0.0, d, 1.0);' +
  '}';

vertexShaderCode =
  'uniform mat4 mvp;' +
  'attribute vec3 ppos;' +
  'varying mediump float d;' +
  'void main(void){' +
  '  vec4 v = mvp * vec4(ppos.x, ppos.y, ppos.z, 1.0);' +
  '  gl_Position = v;' +
  '  d = clamp(sqrt(dot(v.xyz, v.xyz)), 0.0, 1.0);' +
  '  gl_PointSize = 3.0;' +
  '}';

// ── State ────────────────────────────────────────────────────
var gl       = null;
var program  = null;
var vbuffer  = null;
var vattrib  = -1;

var rawPoints = [];    // [x,y,z, x,y,z, ...]  numeric flat array
var scale     = 1.0;
var method    = 1;     // 1 = POINTS, 0 = LINE_STRIP
var scanning  = false; // True while a scan is in progress

var angle  = 0;
var sangle = 0.002;
var tilt   = 1.2;
var nx = 1, ny = 0, nz = 0;

// ── Live polling ─────────────────────────────────────────────
var pollInterval = null;

function startPolling() {
  if (pollInterval) return;
  scanning = true;
  setStatus("Scan in progress... Points: <b id='cnt'>0</b>");
  pollInterval = setInterval(fetchNewPoints, 2000);
  fetchNewPoints(); // Immediate first query
}

function stopPolling() {
  if (pollInterval) {
    clearInterval(pollInterval);
    pollInterval = null;
  }
  scanning = false;
}

function fetchNewPoints() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/newpoints", true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState !== 4) return;
    if (xhr.status !== 200) return;

    try {
      var pts = JSON.parse(xhr.responseText); // ["x,y,z", ...]
      if (pts.length > 0) {
        for (var i = 0; i < pts.length; i++) {
          var parts = pts[i].split(",");
          if (parts.length === 3) {
            rawPoints.push(parseFloat(parts[0]),
                           parseFloat(parts[1]),
                           parseFloat(parts[2]));
          }
        }
        rebuildBuffer();
        var cntEl = document.getElementById("cnt");
        if (cntEl) cntEl.textContent = rawPoints.length / 3;
      }
    } catch(e) {}
  };
  xhr.send();

  // Check status — stop polling when scan is done
  checkStatus();
}

function checkStatus() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/status", true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState !== 4 || xhr.status !== 200) return;
    try {
      var s = JSON.parse(xhr.responseText);
      if (s.state === "done") {
        stopPolling();
        setStatus("Scan complete! Total points: <b>" + (rawPoints.length/3) + "</b>");
      } else if (s.state === "idle") {
        stopPolling();
        setStatus("Ready. <a href='/scan'>Start Scan</a> or <a href='/reset'>Reset</a>");
      }
    } catch(e) {}
  };
  xhr.send();
}

function setStatus(html) {
  var el = document.getElementById("status");
  if (el) el.innerHTML = html;
}

// ── Update WebGL buffer ───────────────────────────────────────
function rebuildBuffer() {
  if (!gl || rawPoints.length === 0) return;

  // Recalculate scale to fit all points within view
  var m = 0.01;
  for (var i = 0; i < rawPoints.length; i++) {
    var v = Math.abs(rawPoints[i]);
    if (v > m) m = v;
  }
  scale = 1.0 / m;

  var fa = new Float32Array(rawPoints);
  gl.bindBuffer(gl.ARRAY_BUFFER, vbuffer);
  gl.bufferData(gl.ARRAY_BUFFER, fa, gl.DYNAMIC_DRAW);
}

// ── Initialize WebGL ─────────────────────────────────────────
function start() {
  var canvas = document.querySelector("canvas");

  try { gl = canvas.getContext("webgl") || canvas.getContext("experimental-webgl"); }
  catch(e) { alert("WebGL error: " + e); return; }
  if (!gl) { alert("WebGL not supported."); return; }

  // Compile shaders
  var fs = gl.createShader(gl.FRAGMENT_SHADER);
  gl.shaderSource(fs, fragmentShaderCode);
  gl.compileShader(fs);
  if (!gl.getShaderParameter(fs, gl.COMPILE_STATUS)) {
    alert("Fragment shader error: " + gl.getShaderInfoLog(fs)); return; }

  var vs = gl.createShader(gl.VERTEX_SHADER);
  gl.shaderSource(vs, vertexShaderCode);
  gl.compileShader(vs);
  if (!gl.getShaderParameter(vs, gl.COMPILE_STATUS)) {
    alert("Vertex shader error: " + gl.getShaderInfoLog(vs)); return; }

  program = gl.createProgram();
  gl.attachShader(program, fs);
  gl.attachShader(program, vs);
  gl.linkProgram(program);
  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    alert("Program link error: " + gl.getProgramInfoLog(program)); return; }
  gl.useProgram(program);

  vattrib = gl.getAttribLocation(program, "ppos");
  gl.enableVertexAttribArray(vattrib);

  vbuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, vbuffer);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([0,0,0]), gl.DYNAMIC_DRAW);
  gl.vertexAttribPointer(vattrib, 3, gl.FLOAT, false, 0, 0);

  // If vertices.js already loaded (completed scan), use those points
  if (typeof vertices !== "undefined" && vertices.length > 0) {
    for (var i = 0; i < vertices.length; i++) rawPoints.push(vertices[i]);
    rebuildBuffer();
    setStatus("Previous scan loaded. Points: <b>" + (rawPoints.length/3) + "</b>. " +
              "<a href='/scan'>New Scan</a> | <a href='/reset'>Reset</a>");
  } else {
    // Check current scan state
    checkStatus();
  }

  window.requestAnimationFrame(draw);

  // Polling: auto-detect if a scan starts while the page is open
  setInterval(function() {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/status", true);
    xhr.onreadystatechange = function() {
      if (xhr.readyState !== 4 || xhr.status !== 200) return;
      try {
        var s = JSON.parse(xhr.responseText);
        if (s.state === "scanning" && !scanning) startPolling();
      } catch(e) {}
    };
    xhr.send();
  }, 3000);
}

// ── Render loop ───────────────────────────────────────────────
function draw() {
  angle += sangle;
  var amvp = gl.getUniformLocation(program, "mvp");

  var rot = rotationMatrix(angle, 0, 0, 1);
  var t   = rotationMatrix(tilt, nx, ny, nz);
  var sc  = scaleMatrix(scale);
  var mat = multMatrix(multMatrix(rot, t), sc);

  gl.uniformMatrix4fv(amvp, false, mat);
  gl.clearColor(0.0, 0.0, 0.0, 1.0);
  gl.clear(gl.COLOR_BUFFER_BIT);

  if (rawPoints.length > 0) {
    gl.drawArrays(method ? gl.POINTS : gl.LINE_STRIP, 0, rawPoints.length / 3);
  }
  gl.flush();
  window.requestAnimationFrame(draw);
}

// ── Matrix helpers ────────────────────────────────────────────
function rotationMatrix(angle, nx, ny, nz) {
  var l = Math.sqrt(nx*nx+ny*ny+nz*nz);
  nx/=l; ny/=l; nz/=l;
  var c=Math.cos(angle), c1=1-c, s=Math.sin(angle);
  return new Float32Array([
    nx*nx*c1+c,    nx*ny*c1-nz*s, nx*nz*c1+ny*s, 0,
    nx*ny*c1+nz*s, ny*ny*c1+c,    ny*nz*c1-nx*s, 0,
    nx*nz*c1-ny*s, ny*nz*c1+nx*s, nz*nz*c1+c,    0,
    0,             0,             0,             1]);
}
function scaleMatrix(f) {
  return new Float32Array([f,0,0,0, 0,f,0,0, 0,0,f,0, 0,0,0,1]);
}
function multMatrix(m1,m2) {
  var v=new Float32Array(16);
  for(var y=0;y<4;y++) for(var x=0;x<4;x++) {
    v[x+y*4]=0;
    for(var i=0;i<4;i++) v[x+y*4]+=m1[i+y*4]*m2[x+i*4];
  }
  return v;
}
  </script>
  <script src="vertices.js"></script>
  <style>
    body { margin:0; background:#000; color:#aaa; font-family:monospace; }
    canvas { display:block; }
    #controls { padding:6px 10px; font-size:13px; display:flex; gap:16px; align-items:center; flex-wrap:wrap; }
    #controls input { width:60px; background:#111; color:#0f0; border:1px solid #333; padding:2px 4px; }
    #controls button { background:#222; color:#0f0; border:1px solid #0f0; padding:3px 10px; cursor:pointer; }
    #controls a { color:#0af; }
    #status { padding:4px 10px; font-size:12px; color:#0f0; min-height:18px; }
  </style>
  </head>
  <body onload="start();">
    <canvas width="1000" height="900"></canvas>
    <div id="status">Checking status...</div>
    <div id="controls">
      <button onclick="method=method^1">Mode: Points/Lines</button>
      <span>Rot speed <input value="0.002" onchange="sangle=+this.value"/></span>
      <span>Tilt <input value="1.2" onchange="tilt=+this.value"/></span>
      <span>nx <input value="1" onchange="nx=+this.value"/></span>
      <span>ny <input value="0" onchange="ny=+this.value"/></span>
      <span>nz <input value="0" onchange="nz=+this.value"/></span>
      <a href="/scan">&#9654; Scan</a>
      <a href="/reset">&#8635; Reset</a>
      <a href="/status">&#9679; Status</a>
    </div>
  </body>
</html>
)cthulhu";
