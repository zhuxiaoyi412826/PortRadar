/* PortLens GUI 前端逻辑
 * 数据来源：Tauri 后端命令（cmd 见各 handler）；浏览器直接打开时使用内置 mock
 */
(() => {
  'use strict';

  // ---------- 基础工具 ----------
  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => Array.from(r.querySelectorAll(s));
  const esc = (s) => String(s == null ? '' : s)
    .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
  const nowTime = () => new Date().toLocaleTimeString('zh-CN', { hour12: false });
  const pad = (n) => String(n).padStart(2, '0');
  const stamp = () => {
    const d = new Date();
    return `${d.getFullYear()}${pad(d.getMonth() + 1)}${pad(d.getDate())}_${pad(d.getHours())}${pad(d.getMinutes())}${pad(d.getSeconds())}`;
  };

  const IS_TAURI = !!window.__TAURI__;
  const invoke = (cmd, args) => {
    if (IS_TAURI) return window.__TAURI__.core.invoke(cmd, args);
    return mockInvoke(cmd, args || {});
  };

  const state = {
    proto: 2,
    page: 'map',
    rows: [],
    selected: new Set(),
    sort: { key: 'port', dir: 1 },
    filter: '',
    config: null,
    autoTimer: null,
    auto: false,
    monitor: new Set(),
    monitorState: {},
    events: [],
    detectPort: null,
  };

  // ---------- 浏览器预览用 mock ----------
  const MOCK_PROC = [
    ['nginx.exe', 'C:\\nginx\\nginx.exe', 80, 'TCP', 'nginx 默认站点'],
    ['mysqld.exe', 'C:\\Program Files\\MySQL\\bin\\mysqld.exe', 3306, 'TCP', 'MySQL 数据库'],
    ['redis-server.exe', 'C:\\Program Files\\Redis\\redis-server.exe', 6379, 'TCP', 'Redis 缓存'],
    ['node.exe', 'C:\\Program Files\\nodejs\\node.exe', 5173, 'TCP', 'Vite 开发服务器'],
    ['java.exe', 'C:\\Program Files\\Java\\bin\\java.exe', 8080, 'TCP', 'Spring Boot 应用'],
    ['python.exe', 'C:\\Python313\\python.exe', 8000, 'TCP', 'Python HTTP 服务'],
    ['com.docker.backend.exe', 'C:\\Program Files\\Docker\\Docker\\com.docker.backend.exe', 5432, 'TCP', 'PostgreSQL 容器'],
    ['wslrelay.exe', 'C:\\Windows\\System32\\wslrelay.exe', 3000, 'TCP', 'WSL 前端服务'],
    ['svchost.exe', 'C:\\Windows\\System32\\svchost.exe', 135, 'TCP', 'RPC 端点映射'],
    ['explorer.exe', 'C:\\Windows\\explorer.exe', 139, 'TCP', 'NetBIOS 会话'],
    ['node.exe', 'C:\\Program Files\\nodejs\\node.exe', 9229, 'TCP', 'Node 调试端口'],
    ['java.exe', 'C:\\Program Files\\Java\\bin\\java.exe', 1099, 'TCP', 'JMX 远程'],
    ['python.exe', 'C:\\Python313\\python.exe', 5000, 'TCP', 'Flask 服务'],
    ['svchost.exe', 'C:\\Windows\\System32\\svchost.exe', 5040, 'TCP', 'Windows 服务'],
    ['vmmem', '', 0, 'UDP', 'Hyper-V 虚拟机'],
  ];
  const mockFav = new Set([3306, 8080]);
  const mockHistory = [];
  const mockAliases = [
    { name: 'mysql', port: 3306 }, { name: 'redis', port: 6379 },
    { name: 'mongo', port: 27017 }, { name: 'postgres', port: 5432 },
    { name: 'nginx', port: 80 },
  ];
  const mockConfig = {
    configPath: 'C:\\Users\\Administrator\\AppData\\Local\\PortLens\\port.conf',
    dataDir: 'C:\\Users\\Administrator\\AppData\\Local\\PortLens',
    scanStart: 1, scanEnd: 1024, historyLimit: 50, watchInterval: 1000,
    aliases: mockAliases, favorites: [3306, 8080],
  };

  function mockRows() {
    return MOCK_PROC.map(([name, path, port, proto, desc], i) => ({
      port, protocol: proto, state: 'LISTENING', localAddr: `0.0.0.0:${port}`,
      remoteAddr: '*:*', remotePort: 0, pid: 4000 + i, name, path,
      special: name.indexOf('docker') >= 0 ? '[Docker: pg-dev]' : (name === 'wslrelay.exe' ? '[WSL]' : (name === 'vmmem' ? '[Hyper-V]' : '')),
      desc, fav: mockFav.has(port),
    }));
  }

  function mockInvoke(cmd, a) {
    const rows = mockRows();
    switch (cmd) {
      case 'list_ports':
        return Promise.resolve(a.proto === 0 ? rows.filter(r => r.protocol === 'TCP')
          : a.proto === 1 ? rows.filter(r => r.protocol === 'UDP') : rows);
      case 'check_port': {
        const r = rows.find(x => x.port === a.port);
        const empty = { occupied: false, pid: 0, name: '', path: '', localAddr: '', special: '' };
        return Promise.resolve({
          port: a.port, desc: r ? r.desc : '未知端口', fav: mockFav.has(a.port),
          occupied: !!r, tcp: r ? { occupied: true, pid: r.pid, name: r.name, path: r.path, localAddr: r.localAddr, special: r.special } : empty,
          udp: empty,
          connections: r ? [{ protocol: r.protocol, state: 'LISTENING', localAddr: r.localAddr, remoteAddr: '*:*', remotePort: 0, pid: r.pid }] : [],
        });
      }
      case 'scan_ports':
        return Promise.resolve(rows.filter(r => r.port >= a.start && r.port <= a.end));
      case 'process_detail': {
        const r = rows.find(x => x.pid === a.pid) || {};
        return Promise.resolve({
          pid: a.pid, name: r.name || 'unknown', path: r.path || '', memoryKb: 86400,
          peakMemoryKb: 120000, threadCount: 24, parentPid: 1200, startTime: '2026-11-13 09:12:00',
          company: 'Mock Company', description: r.desc || '',
        });
      }
      case 'find_process':
        return Promise.resolve(rows.filter(r => (!a.name || r.name.toLowerCase().includes(a.name.toLowerCase()))
          && (!a.portFilter || r.port === a.portFilter))
          .map(r => ({ pid: r.pid, name: r.name, path: r.path, special: r.special, ports: [r.protocol + ':' + r.port] })));
      case 'port_connections':
        return Promise.resolve(rows.filter(r => r.port === a.port)
          .map(r => ({ protocol: r.protocol, state: 'LISTENING', localAddr: r.localAddr, remoteAddr: '*:*', remotePort: 0, pid: r.pid })));
      case 'kill_pid': {
        const r = rows.find(x => x.pid === a.pid);
        if (r) mockHistory.unshift({ time: new Date().toLocaleString('zh-CN'), pid: r.pid, procName: r.name, path: r.path, ports: r.protocol + ':' + r.port });
        return Promise.resolve(!!r);
      }
      case 'find_free_port':
        return Promise.resolve(a.start + 3);
      case 'export_ports':
        return Promise.resolve(true);
      case 'config_info':
        return Promise.resolve(mockConfig);
      case 'favorites':
        return Promise.resolve([...mockFav].map(p => {
          const r = rows.find(x => x.port === p);
          return {
            port: p, occupied: !!r, pid: r ? r.pid : 0, name: r ? r.name : '',
            path: r ? r.path : '', special: r ? r.special : '',
            desc: r ? r.desc : (p === 3306 ? 'MySQL 数据库' : 'Spring Boot 应用'),
          };
        }));
      case 'fav_add': mockFav.add(a.port); return Promise.resolve(true);
      case 'fav_remove': mockFav.delete(a.port); return Promise.resolve(true);
      case 'history': return Promise.resolve(mockHistory);
      case 'history_clear': mockHistory.length = 0; return Promise.resolve(true);
      case 'install_path': return Promise.resolve(true);
      case 'app_version': return Promise.resolve('0.8.0');
      case 'quit_app': return Promise.resolve();
      default: return Promise.resolve(null);
    }
  }

  // ---------- 通用 UI ----------
  const toast = (msg, type = 'success') => {
    const el = document.createElement('div');
    el.className = `toast ${type}`;
    el.innerHTML = `<span class="toast-dot"></span><div>${esc(msg)}</div>`;
    $('#toasts').appendChild(el);
    setTimeout(() => { el.classList.add('out'); setTimeout(() => el.remove(), 220); }, 2600);
  };

  const loading = (show, text = '加载中…') => {
    $('#loadingText').textContent = text;
    $('#loadingOverlay').classList.toggle('show', !!show);
  };

  let modalResolve = null;
  const confirmBox = ({ title, body, okText = '确认', danger = true }) => new Promise((resolve) => {
    modalResolve = resolve;
    $('#modalTitle').textContent = title;
    $('#modalBody').innerHTML = body;
    $('#modalActions').innerHTML = `
      <button class="btn ghost" data-action="modal-cancel">取消</button>
      <button class="btn ${danger ? 'danger' : 'primary'}" data-action="modal-ok">${esc(okText)}</button>`;
    $('#modalMask').classList.add('show');
  });
  const closeModal = (val) => {
    $('#modalMask').classList.remove('show');
    if (modalResolve) { modalResolve(val); modalResolve = null; }
  };

  const specialTag = (special) => {
    if (!special) return '';
    let cls = 'tag-docker';
    if (special.indexOf('WSL') >= 0) cls = 'tag-wsl';
    else if (special.indexOf('Hyper') >= 0) cls = 'tag-hyperv';
    const text = special.replace(/[\[\]]/g, '');
    return `<span class="tag ${cls}">${esc(text)}</span>`;
  };

  const protoTag = (p) => `<span class="badge">${esc(p)}</span>`;

  const emptyBox = (title, text, action) => `
    <div class="empty">
      <div class="empty-icon">◍</div>
      <h3>${esc(title)}</h3>
      <p>${esc(text)}</p>
      ${action || ''}
    </div>`;

  // ---------- 页面切换 ----------
  function goPage(page) {
    state.page = page;
    $$('.nav-item').forEach(n => n.classList.toggle('active', n.dataset.page === page));
    $$('.page').forEach(p => p.classList.toggle('active', p.id === 'page-' + page));
    closeDrawer();
    if (page === 'map') renderMap();
    if (page === 'list') renderList();
    if (page === 'detect') { renderAliasChips(); if (state.detectPort) runDetect(state.detectPort); }
    if (page === 'monitor') { renderMonitor(); }
    if (page === 'fav') renderFavorites();
    if (page === 'history') renderHistory();
    if (page === 'settings') renderSettings();
  }

  // ---------- 数据刷新 ----------
  async function refresh(showLoading = false) {
    if (showLoading) loading(true);
    try {
      state.rows = await invoke('list_ports', { proto: state.proto });
      if (!Array.isArray(state.rows)) state.rows = [];
      $('#listMeta').textContent = `共 ${state.rows.length} 条`;
      if (state.page === 'map') renderMap();
      if (state.page === 'list') renderList();
      if (state.page === 'fav') renderFavorites();
      if (state.page === 'detect' && state.detectPort) runDetect(state.detectPort, true);
    } catch (e) {
      toast('读取端口失败：' + e, 'error');
    } finally {
      if (showLoading) loading(false);
    }
  }

  // ---------- 端口地图 ----------
  function renderMap() {
    const rows = filteredRows();
    const groups = new Map();
    rows.forEach(r => {
      const key = r.pid + '|' + r.name;
      if (!groups.has(key)) groups.set(key, { pid: r.pid, name: r.name, special: r.special, path: r.path, ports: [] });
      groups.get(key).ports.push(r);
    });
    const list = [...groups.values()].sort((a, b) => b.ports.length - a.ports.length);

    const favCount = rows.filter(r => r.fav).length;
    const specCount = list.filter(g => g.special).length;
    $('#mapStats').innerHTML = `
      <div class="stat-card"><div class="stat-icon">◉</div><div><div class="stat-num">${rows.length}</div><div class="stat-label">监听端口</div></div></div>
      <div class="stat-card"><div class="stat-icon">▤</div><div><div class="stat-num">${list.length}</div><div class="stat-label">进程数</div></div></div>
      <div class="stat-card"><div class="stat-icon">◇</div><div><div class="stat-num">${specCount}</div><div class="stat-label">容器 / 特殊环境</div></div></div>
      <div class="stat-card"><div class="stat-icon">★</div><div><div class="stat-num">${favCount}</div><div class="stat-label">收藏端口</div></div></div>`;

    if (!rows.length) {
      $('#mapBoard').innerHTML = emptyBox('没有监听端口', '当前筛选条件下没有检测到监听端口，试试切换协议或刷新。',
        '<button class="btn primary" data-action="refresh">重新扫描</button>');
      return;
    }

    $('#mapBoard').innerHTML = `
      <div class="map-scale">
        <div class="map-groups">
          ${list.map(g => `
            <div class="map-group" data-group="${g.pid}" data-name="${esc(g.name)}">
              <div class="mg-top"><span class="mg-name">${esc(g.name || 'PID ' + g.pid)}</span>${specialTag(g.special)}</div>
              <div class="mg-meta"><span class="mg-count">${g.ports.length} 个端口</span><span>PID ${g.pid}</span></div>
            </div>`).join('')}
        </div>
        <div class="map-ports">
          ${list.map(g => g.ports.map(p => `
            <div class="map-port ${p.fav ? 'fav' : ''}" data-port="${p.port}" data-pid="${p.pid}" data-group-of="${g.pid}">
              <span class="mp-port">${p.port}</span>
              <span class="mp-proto">${esc(p.protocol)}</span>
              <span class="mp-desc">${esc(p.desc || p.localAddr || '')}</span>
              ${p.fav ? '<span class="mp-star">★</span>' : ''}
            </div>`).join('')).join('')}
        </div>
      </div>
      <svg class="map-links"></svg>`;

    requestAnimationFrame(drawLinks);
  }

  function drawLinks() {
    const board = $('#mapBoard');
    const svg = $('.map-links', board);
    if (!svg) return;
    // 以 SVG 自身左上角为原点，尺寸按像素 1:1 铺设，避免 viewBox 缩放导致连线错位
    const w = board.scrollWidth;
    const h = board.scrollHeight;
    svg.removeAttribute('viewBox');
    svg.setAttribute('width', w);
    svg.setAttribute('height', h);
    svg.style.width = w + 'px';
    svg.style.height = h + 'px';
    const base = svg.getBoundingClientRect();
    const paths = [];
    $$('.map-port', board).forEach(el => {
      const g = $(`.map-group[data-group="${el.dataset.groupOf}"]`, board);
      if (!g) return;
      const a = g.getBoundingClientRect();
      const b = el.getBoundingClientRect();
      const x1 = a.right - base.left;
      const y1 = a.top - base.top + a.height / 2;
      const x2 = b.left - base.left;
      const y2 = b.top - base.top + b.height / 2;
      const cx = (x1 + x2) / 2;
      const fav = el.classList.contains('fav') ? ' fav-link' : '';
      paths.push(`<path class="link${fav}" data-port="${el.dataset.port}" d="M${x1},${y1} C${cx},${y1} ${cx},${y2} ${x2},${y2}"/>`);
    });
    svg.innerHTML = paths.join('');
  }

  // ---------- 端口列表 ----------
  function filteredRows() {
    const kw = state.filter.trim().toLowerCase();
    let rows = state.rows.slice();
    if (kw) {
      rows = rows.filter(r => String(r.port).includes(kw)
        || (r.name || '').toLowerCase().includes(kw)
        || (r.desc || '').toLowerCase().includes(kw)
        || String(r.pid) === kw);
    }
    const { key, dir } = state.sort;
    rows.sort((a, b) => {
      let v = 0;
      if (key === 'name') v = String(a.name).localeCompare(String(b.name));
      else if (key === 'pid') v = a.pid - b.pid;
      else if (key === 'protocol') v = String(a.protocol).localeCompare(String(b.protocol));
      else v = a.port - b.port;
      return v * dir;
    });
    return rows;
  }

  function renderList() {
    const rows = filteredRows();
    const arrow = (k) => state.sort.key === k ? `<span class="sort-arrow">${state.sort.dir > 0 ? '▲' : '▼'}</span>` : '';
    $('#listWrap').innerHTML = rows.length ? `
      <div class="table-wrap">
        <table class="table">
          <thead><tr>
            <th class="col-check"><input type="checkbox" id="checkAll" ${rows.every(r => state.selected.has(r.pid + ':' + r.port)) ? 'checked' : ''}></th>
            <th class="col-port sortable" data-sort="port">端口${arrow('port')}</th>
            <th class="col-proto sortable" data-sort="protocol">协议${arrow('protocol')}</th>
            <th class="col-state">状态</th>
            <th class="sortable" data-sort="name">进程${arrow('name')}</th>
            <th class="col-pid sortable" data-sort="pid">PID${arrow('pid')}</th>
            <th>路径 / 说明</th>
            <th class="col-ops">操作</th>
          </tr></thead>
          <tbody>
          ${rows.map(r => {
            const key = r.pid + ':' + r.port;
            const sel = state.selected.has(key);
            return `<tr class="${sel ? 'selected' : ''}" data-key="${key}">
              <td class="col-check"><input type="checkbox" class="row-check" ${sel ? 'checked' : ''}></td>
              <td class="col-port port-cell" style="color:${r.fav ? 'var(--warn)' : '#FCA5A5'}">${r.port}</td>
              <td class="col-proto">${protoTag(r.protocol)}</td>
              <td class="col-state"><span class="status-line busy"><span class="dot busy"></span>${esc(r.state || 'LISTENING')}</span></td>
              <td><div class="proc-cell"><span class="dot busy"></span><span class="proc-name" title="${esc(r.name)}">${esc(r.name || '未知')}</span>${specialTag(r.special)}</div></td>
              <td class="col-pid mono">${r.pid}</td>
              <td><div class="path-cell" title="${esc(r.path)}">${esc(r.desc || r.path || '—')}</div></td>
              <td class="col-ops"><div class="row-actions">
                <button class="btn tiny ghost" data-action="detail-port" data-port="${r.port}" data-pid="${r.pid}">详情</button>
                <button class="btn tiny ghost" data-action="fav-toggle" data-port="${r.port}" title="收藏">${r.fav ? '<span class="fav-star">★</span>' : '☆'}</button>
                <button class="btn tiny danger-ghost" data-action="kill-port" data-port="${r.port}" data-pid="${r.pid}">释放</button>
              </div></td>
            </tr>`;
          }).join('')}
          </tbody>
        </table>
      </div>` : emptyBox('没有匹配的端口', '当前筛选或搜索条件下没有结果。', '<button class="btn ghost" data-action="clear-filter">清除搜索</button>');
    updateSelCount();
  }

  function updateSelCount() {
    $('#selCount').textContent = String(state.selected.size);
    $('#batchKillBtn').disabled = state.selected.size === 0;
  }

  // ---------- 端口检测 ----------
  function renderAliasChips() {
    const aliases = (state.config && state.config.aliases) || [];
    $('#aliasChips').innerHTML = aliases.map(a =>
      `<span class="chip" data-action="detect-alias" data-alias="${esc(a.name)}">${esc(a.name)} <b>${a.port}</b></span>`).join('');
  }

  async function runDetect(input, silent = false) {
    let raw = String(input == null ? '' : input).trim();
    if (!raw) { toast('请输入端口号或别名', 'warn'); return; }
    let port = Number(raw);
    if (!Number.isFinite(port) || port <= 0) {
      const alias = ((state.config && state.config.aliases) || []).find(a => a.name.toLowerCase() === raw.toLowerCase());
      if (!alias) { toast(`无法识别「${raw}」，请输入 1-65535 的端口号或已配置别名`, 'error'); return; }
      port = alias.port;
    }
    if (port < 1 || port > 65535) { toast('端口范围应为 1-65535', 'error'); return; }
    state.detectPort = port;
    if (!silent) loading(true, '正在检测端口…');
    try {
      const d = await invoke('check_port', { port });
      renderDetect(d);
    } catch (e) {
      toast('检测失败：' + e, 'error');
    } finally {
      if (!silent) loading(false);
    }
  }

  function renderDetect(d) {
    const card = (title, info) => `
      <div class="status-card ${info.occupied ? 'busy' : 'ok'}">
        <div class="status-head">
          <span class="status-proto">${title}</span>
          <span class="status-line ${info.occupied ? 'busy' : 'ok'}"><span class="dot ${info.occupied ? 'busy' : 'ok'}"></span>${info.occupied ? '已被占用' : '空闲'}</span>
        </div>
        ${info.occupied ? `
          <div class="kv">
            <span class="k">进程</span><span class="v">${esc(info.name || '未知')}${specialTag(info.special)}</span>
            <span class="k">PID</span><span class="v mono">${info.pid}</span>
            <span class="k">地址</span><span class="v mono">${esc(info.localAddr || '—')}</span>
            <span class="k">路径</span><span class="v mono">${esc(info.path || '—')}</span>
          </div>` : '<div class="status-empty">该协议下没有进程监听此端口</div>'}
      </div>`;

    const conns = (d.connections || []).length ? `
      <div class="panel">
        <div class="panel-title">连接详情 <span class="muted">${d.connections.length} 条</span></div>
        <div class="conn-list">
          <div class="conn-row head"><span>协议</span><span>状态</span><span>本地地址</span><span>远端地址</span><span>远端端口</span><span>PID</span></div>
          ${d.connections.map(c => `<div class="conn-row">
            <span>${esc(c.protocol)}</span><span>${esc(c.state)}</span>
            <span class="mono">${esc(c.localAddr)}</span><span class="mono">${esc(c.remoteAddr)}</span>
            <span class="mono">${c.remotePort}</span><span class="mono">${c.pid}</span>
          </div>`).join('')}
        </div>
      </div>` : '';

    const pid = (d.tcp && d.tcp.pid) || (d.udp && d.udp.pid) || 0;
    $('#detectResult').innerHTML = `
      <div class="port-hero">
        <div class="hero-main">
          <div class="hero-port">${d.port}</div>
          <div class="hero-info">
            <h2>${esc(d.desc || '未知端口')}${d.fav ? ' <span class="fav-star">★</span>' : ''}</h2>
            <p>${d.occupied ? '当前被占用' : '当前空闲，可以使用'}</p>
          </div>
        </div>
        <div class="hero-actions">
          <button class="btn ghost" data-action="fav-toggle" data-port="${d.port}">${d.fav ? '取消收藏' : '加入收藏'}</button>
          ${pid ? `<button class="btn danger-ghost" data-action="kill-port" data-port="${d.port}" data-pid="${pid}">释放进程</button>` : ''}
          <button class="btn ghost" data-action="add-monitor" data-port="${d.port}">加入监控</button>
        </div>
      </div>
      <div class="result-grid" style="margin-top:14px">
        ${card('TCP', d.tcp || {})}
        ${card('UDP', d.udp || {})}
      </div>
      <div style="margin-top:14px">${conns}</div>`;
  }

  // ---------- 监控时间轴 ----------
  function loadMonitor() {
    try {
      const saved = JSON.parse(localStorage.getItem('portlens.monitor') || '[]');
      state.monitor = new Set(saved);
      state.events = JSON.parse(localStorage.getItem('portlens.events') || '[]');
    } catch (_) { /* 忽略损坏数据 */ }
  }
  function saveMonitor() {
    localStorage.setItem('portlens.monitor', JSON.stringify([...state.monitor]));
    localStorage.setItem('portlens.events', JSON.stringify(state.events.slice(0, 300)));
  }

  function renderMonitorChips() {
    $('#monitorChips').innerHTML = [...state.monitor].sort((a, b) => a - b).map(p =>
      `<span class="chip">:${p} <span class="chip-x" data-action="remove-monitor" data-port="${p}">✕</span></span>`).join('');
    const favs = ((state.config && state.config.favorites) || []).filter(p => !state.monitor.has(p));
    $('#monitorFavChips').innerHTML = favs.map(p =>
      `<span class="chip" data-action="add-monitor" data-port="${p}">★ 收藏 ${p}</span>`).join('');
  }

  function renderMonitor() {
    renderMonitorChips();
    $('#monitorCount').textContent = state.events.length ? `共 ${state.events.length} 条` : '';
    $('#monitorEvents').innerHTML = state.events.length ? state.events.slice(0, 120).map(e => `
      <div class="tl-item ${e.busy ? 'busy' : 'ok'}">
        <span class="tl-dot"></span>
        <div class="tl-card">
          <div class="tl-time">${esc(e.time)} · 端口 <span class="hl">${e.port}</span></div>
          <div class="tl-text">${e.busy
            ? `被 <span class="hl busy">${esc(e.name || '未知进程')}</span> 占用（PID ${e.pid || '—'}）`
            : `<span class="hl ok">已释放</span>，端口恢复空闲`}</div>
        </div>
      </div>`).join('') : emptyBox('暂无事件', '添加要监控的端口后，端口占用状态变化会记录在这里。');
  }

  let pollTimer = null;
  function startMonitorPolling() {
    stopMonitorPolling();
    const interval = Math.max(800, (state.config && state.config.watchInterval) || 1000);
    pollTimer = setInterval(async () => {
      for (const port of state.monitor) {
        try {
          const d = await invoke('check_port', { port });
          const busy = !!d.occupied;
          const prev = state.monitorState[port];
          if (prev === undefined) { state.monitorState[port] = busy; continue; }
          if (prev !== busy) {
            state.monitorState[port] = busy;
            const name = busy ? ((d.tcp && d.tcp.name) || '未知进程') : '';
            const pid = busy ? (d.tcp && d.tcp.pid) || 0 : 0;
            // 同一端口短时间内去重
            const last = state.events.find(e => e.port === port);
            if (!last || Date.now() - last.ts > 1500) {
              state.events.unshift({ port, busy, name, pid, time: nowTime(), ts: Date.now() });
              state.events = state.events.slice(0, 300);
              saveMonitor();
              if (state.page === 'monitor') { renderMonitor(); }
              flash(busy ? `端口 ${port} 被占用` : `端口 ${port} 已释放`, busy);
              beep(busy);
            }
          }
        } catch (_) { /* 单次失败忽略 */ }
      }
    }, interval);
  }
  function stopMonitorPolling() { if (pollTimer) { clearInterval(pollTimer); pollTimer = null; } }

  let flashTimer = null;
  function flash(text, busy) {
    const el = $('#monitorFlash');
    el.textContent = text;
    el.style.color = busy ? 'var(--danger)' : 'var(--success)';
    el.classList.add('show');
    clearTimeout(flashTimer);
    flashTimer = setTimeout(() => el.classList.remove('show'), 4000);
  }

  let audioCtx = null;
  function beep(busy) {
    try {
      audioCtx = audioCtx || new (window.AudioContext || window.webkitAudioContext)();
      const o = audioCtx.createOscillator();
      const g = audioCtx.createGain();
      o.type = 'sine';
      o.frequency.value = busy ? 880 : 620;
      g.gain.value = 0.05;
      o.connect(g); g.connect(audioCtx.destination);
      o.start(); o.stop(audioCtx.currentTime + 0.12);
    } catch (_) { /* 浏览器未授权时忽略 */ }
  }

  // ---------- 收藏夹 ----------
  async function renderFavorites() {
    let list = [];
    try { list = await invoke('favorites'); } catch (e) { toast('读取收藏失败：' + e, 'error'); }
    if (!list || !list.length) {
      $('#favGrid').innerHTML = emptyBox('还没有收藏端口', '在端口列表或详情里点击 ☆ 即可收藏常用端口。',
        '<button class="btn primary" data-action="page" data-page="list">去端口列表</button>');
      return;
    }
    $('#favGrid').innerHTML = list.map(f => `
      <div class="fav-card">
        <div class="fav-top">
          <div>
            <div class="fav-port">${f.port}</div>
            <div class="fav-desc">${esc(f.desc || '')}</div>
          </div>
          ${specialTag(f.special)}
        </div>
        <div class="fav-proc">
          <span class="status-line ${f.occupied ? 'busy' : 'ok'}"><span class="dot ${f.occupied ? 'busy' : 'ok'}"></span>${f.occupied ? '占用中' : '空闲'}</span>
          ${f.occupied ? `<span class="muted">${esc(f.name)} · PID ${f.pid}</span>` : ''}
        </div>
        <div class="fav-path" title="${esc(f.path)}">${esc(f.path || '—')}</div>
        <div class="fav-actions">
          <button class="btn tiny ghost" data-action="detail-port" data-port="${f.port}" data-pid="${f.pid || 0}">查看</button>
          ${f.occupied ? `<button class="btn tiny danger-ghost" data-action="kill-port" data-port="${f.port}" data-pid="${f.pid}">释放</button>` : ''}
          <button class="btn tiny ghost" data-action="fav-remove" data-port="${f.port}">取消收藏</button>
        </div>
      </div>`).join('');
  }

  // ---------- 历史记录 ----------
  async function renderHistory() {
    let list = [];
    try { list = await invoke('history'); } catch (e) { toast('读取历史失败：' + e, 'error'); }
    if (!list || !list.length) {
      $('#historyWrap').innerHTML = emptyBox('暂无操作记录', '通过 PortLens 释放进程后，这里会保留操作痕迹，便于追溯。');
      return;
    }
    $('#historyWrap').innerHTML = `
      <div class="table-wrap"><table class="table">
        <thead><tr><th>时间</th><th>PID</th><th>进程</th><th>端口</th><th>路径</th><th class="col-ops">操作</th></tr></thead>
        <tbody>${list.map(h => `<tr>
          <td class="mono" style="white-space:nowrap">${esc(h.time)}</td>
          <td class="mono">${h.pid}</td>
          <td>${esc(h.procName)}</td>
          <td class="mono">${esc(h.ports || '—')}</td>
          <td><div class="path-cell" title="${esc(h.path)}">${esc(h.path || '—')}</div></td>
          <td class="col-ops"><button class="btn tiny ghost" data-action="copy" data-text="${esc(h.path)}">复制路径</button></td>
        </tr>`).join('')}</tbody>
      </table></div>`;
  }

  // ---------- 设置 ----------
  async function renderSettings() {
    let info = state.config;
    let ver = '';
    try { info = await invoke('config_info'); ver = await invoke('app_version'); state.config = info; } catch (_) { /* 用缓存 */ }
    if (!info) { $('#settingsBody').innerHTML = emptyBox('无法读取配置', '配置模块未响应，请重启程序。'); return; }
    const path = info.configPath || '';
    const dir = info.dataDir || '';
    $('#settingsBody').innerHTML = `
      <div class="settings-grid">
        <div class="panel">
          <div class="panel-title">运行环境</div>
          <table class="info-table">
            <tr><td class="muted">程序版本</td><td>v${esc(ver || '0.8.0')}</td></tr>
            <tr><td class="muted">配置文件</td><td class="mono">${esc(path)}<br><button class="btn tiny ghost" style="margin-top:6px" data-action="copy" data-text="${esc(path)}">复制路径</button></td></tr>
            <tr><td class="muted">数据目录</td><td class="mono">${esc(dir)}<br><button class="btn tiny ghost" style="margin-top:6px" data-action="copy" data-text="${esc(dir)}">复制路径</button></td></tr>
            <tr><td class="muted">默认扫描范围</td><td>${info.scanStart} - ${info.scanEnd}</td></tr>
            <tr><td class="muted">历史上限</td><td>${info.historyLimit} 条</td></tr>
            <tr><td class="muted">监控间隔</td><td>${info.watchInterval} ms</td></tr>
          </table>
        </div>
        <div class="panel">
          <div class="panel-title">端口别名</div>
          <table class="info-table">
            ${(info.aliases || []).length ? info.aliases.map(a => `
              <tr><td>${esc(a.name)}</td><td class="mono">:${a.port}</td>
              <td><button class="btn tiny ghost" data-action="detect-port" data-port="${a.port}">检测</button></td></tr>`).join('')
              : '<tr><td class="muted">未配置别名</td></tr>'}
          </table>
          <div class="input-row">
            <button class="btn small" data-action="install-path">安装 / 修复 port 命令</button>
            <button class="btn small ghost" data-action="reload-config">重新加载配置</button>
          </div>
        </div>
      </div>
      <div class="panel">
        <div class="panel-title">关于</div>
        <div class="input-row" style="justify-content:space-between">
          <span class="muted">PortLens v${esc(ver || '0.8.0')} · 双击 exe 打开图形界面，命令行执行 port 使用终端模式</span>
          <span>
            <button class="btn small ghost" data-action="page" data-page="detect">打开端口检测</button>
            <button class="btn small danger-ghost" data-action="quit-app">退出程序</button>
          </span>
        </div>
      </div>`;
  }

  // ---------- 详情抽屉 ----------
  function closeDrawer() {
    $('#drawer').classList.remove('open');
    $('#drawerMask').classList.remove('show');
  }

  async function openPort(port, pid) {
    $('#drawer').classList.add('open');
    $('#drawerMask').classList.add('show');
    $('#drawerTitle').textContent = '端口 ' + port;
    $('#drawerSub').textContent = '正在读取详情…';
    $('#drawerBody').innerHTML = '<div class="drawer-loading">加载中…</div>';
    try {
      const d = await invoke('check_port', { port });
      let proc = null;
      const realPid = pid || (d.tcp && d.tcp.pid) || 0;
      if (realPid) proc = await invoke('process_detail', { pid: realPid });
      $('#drawerSub').textContent = d.desc || '';
      const info = d.tcp && d.tcp.occupied ? d.tcp : (d.udp && d.udp.occupied ? d.udp : null);
      $('#drawerBody').innerHTML = `
        <div class="drawer-section">
          <h4>状态</h4>
          <div class="drawer-card">
            <span class="status-line ${d.occupied ? 'busy' : 'ok'}"><span class="dot ${d.occupied ? 'busy' : 'ok'}"></span>${d.occupied ? '已被占用' : '空闲'}</span>
            <div class="kv" style="margin-top:10px">
              <span class="k">TCP</span><span class="v">${d.tcp && d.tcp.occupied ? '占用 · ' + esc(d.tcp.name) : '空闲'}</span>
              <span class="k">UDP</span><span class="v">${d.udp && d.udp.occupied ? '占用 · ' + esc(d.udp.name) : '空闲'}</span>
            </div>
          </div>
        </div>
        ${info ? `
        <div class="drawer-section">
          <h4>进程</h4>
          <div class="drawer-card">
            <div class="proc-cell"><span class="dot busy"></span><span>${esc(info.name)}</span>${specialTag(info.special)}</div>
            <div class="kv" style="margin-top:10px">
              <span class="k">PID</span><span class="v mono">${info.pid}</span>
              <span class="k">地址</span><span class="v mono">${esc(info.localAddr || '—')}</span>
              <span class="k">路径</span><span class="v mono">${esc(info.path || '—')}</span>
              ${proc ? `<span class="k">内存</span><span class="v">${(proc.memoryKb / 1024).toFixed(1)} MB</span>
              <span class="k">线程</span><span class="v">${proc.threadCount}</span>
              <span class="k">父进程</span><span class="v mono">${proc.parentPid}</span>
              <span class="k">启动</span><span class="v">${esc(proc.startTime || '—')}</span>` : ''}
            </div>
          </div>
        </div>` : ''}
        <div class="drawer-section">
          <h4>连接（${(d.connections || []).length}）</h4>
          ${(d.connections || []).length ? `<div class="drawer-card">
            ${d.connections.map(c => `<div class="muted mono" style="font-size:11.5px;padding:3px 0">${esc(c.protocol)} · ${esc(c.state)} · ${esc(c.localAddr)} → ${esc(c.remoteAddr)}:${c.remotePort} · PID ${c.pid}</div>`).join('')}
          </div>` : '<div class="drawer-loading">没有活动连接</div>'}
        </div>
        <div class="drawer-actions">
          <button class="btn small ghost" data-action="fav-toggle" data-port="${port}">${d.fav ? '取消收藏' : '加入收藏'}</button>
          <button class="btn small ghost" data-action="add-monitor" data-port="${port}">加入监控</button>
          ${realPid ? `<button class="btn small danger-ghost" data-action="kill-port" data-port="${port}" data-pid="${realPid}">释放进程</button>` : ''}
        </div>`;
    } catch (e) {
      $('#drawerBody').innerHTML = `<div class="drawer-loading">读取失败：${esc(e)}</div>`;
    }
  }

  // ---------- 操作 ----------
  async function killPort(port, pid) {
    if (!pid) { toast('该端口当前空闲，无需释放', 'warn'); return; }
    const ok = await confirmBox({
      title: '确认释放端口 ' + port + '？',
      body: `将结束占用该端口的进程 <span class="hl">PID ${pid}</span>。<span class="warn-text">该操作不可撤销</span>，请确认不是系统关键进程。`,
      okText: '结束进程',
    });
    if (!ok) return;
    loading(true, '正在结束进程…');
    try {
      const done = await invoke('kill_pid', { pid });
      if (done) { toast(`端口 ${port} 已释放`); closeDrawer(); await refresh(); if (state.page === 'fav') renderFavorites(); }
      else toast('释放失败，可能需要管理员权限或进程已退出', 'error');
    } catch (e) {
      toast('释放失败：' + e, 'error');
    } finally { loading(false); }
  }

  async function toggleFav(port) {
    const row = state.rows.find(r => r.port === port);
    const isFav = row ? row.fav : ((state.config && state.config.favorites) || []).includes(port);
    try {
      const ok = isFav ? await invoke('fav_remove', { port }) : await invoke('fav_add', { port });
      if (!ok) { toast('操作失败', 'error'); return; }
      toast(isFav ? `已取消收藏 ${port}` : `已收藏 ${port}`);
      await refresh();
      if (state.page === 'fav') renderFavorites();
    } catch (e) { toast('操作失败：' + e, 'error'); }
  }

  async function exportList(fmt) {
    const name = `ports_${stamp()}.${fmt === 0 ? 'csv' : 'json'}`;
    const dir = (state.config && state.config.dataDir) || '';
    const path = dir ? `${dir}\\${name}` : name;
    try {
      const ok = await invoke('export_ports', { path, proto: state.proto, fmt });
      if (ok) toast(`已导出到 ${path}`);
      else toast('导出失败，请检查目录权限', 'error');
    } catch (e) { toast('导出失败：' + e, 'error'); }
  }

  async function batchKill() {
    const items = [...state.selected].map(k => {
      const [pid, port] = k.split(':').map(Number);
      return { pid, port };
    }).filter(i => i.pid);
    if (!items.length) return;
    const ok = await confirmBox({
      title: `确认批量释放 ${items.length} 个端口？`,
      body: `将结束以下进程：<br>` + items.map(i => `· 端口 <span class="hl">${i.port}</span> · PID ${i.pid}`).join('<br>') +
        `<br><span class="warn-text">系统关键进程会被自动跳过。</span>`,
      okText: '批量释放',
    });
    if (!ok) return;
    loading(true, '正在批量释放…');
    let n = 0;
    for (const it of items) {
      try { if (await invoke('kill_pid', { pid: it.pid })) n++; } catch (_) { /* 跳过失败项 */ }
    }
    loading(false);
    state.selected.clear();
    toast(`批量释放完成：成功 ${n} / ${items.length}`, n === items.length ? 'success' : 'warn');
    await refresh();
  }

  // ---------- 点击事件统一分发 ----------
  document.addEventListener('click', async (ev) => {
    const hit = ev.target.closest('[data-action]');
    if (!hit) {
      const portEl = ev.target.closest('.map-port');
      if (portEl) { openPort(Number(portEl.dataset.port), Number(portEl.dataset.pid)); }
      return;
    }
    const a = hit.dataset.action;
    switch (a) {
      case 'page': goPage(hit.dataset.page); break;
      case 'proto': {
        state.proto = Number(hit.dataset.proto);
        $$('#protoSeg button').forEach(b => b.classList.toggle('active', b === hit));
        await refresh();
        break;
      }
      case 'refresh': await refresh(true); break;
      case 'detect': await runDetect($('#detectInput').value); break;
      case 'detect-alias': $('#detectInput').value = hit.dataset.alias; goPage('detect'); await runDetect(hit.dataset.alias); break;
      case 'detect-port': $('#detectInput').value = hit.dataset.port; goPage('detect'); await runDetect(hit.dataset.port); break;
      case 'find-free': {
        const start = Number($('#freePortInput').value) || 8000;
        try {
          const p = await invoke('find_free_port', { start });
          $('#freePortResult').innerHTML = p ? `可用端口：<b>${p}</b>` : '该范围没有可用端口';
        } catch (e) { toast('查找失败：' + e, 'error'); }
        break;
      }
      case 'detail-port': openPort(Number(hit.dataset.port), Number(hit.dataset.pid)); break;
      case 'fav-toggle': await toggleFav(Number(hit.dataset.port)); break;
      case 'fav-remove': await invoke('fav_remove', { port: Number(hit.dataset.port) }); toast('已取消收藏'); await renderFavorites(); break;
      case 'kill-port': await killPort(Number(hit.dataset.port), Number(hit.dataset.pid)); break;
      case 'batch-kill': await batchKill(); break;
      case 'export': await exportList(Number(hit.dataset.fmt)); break;
      case 'add-monitor': {
        const p = Number(hit.dataset.port || $('#monitorInput').value);
        if (!p || p < 1 || p > 65535) { toast('请输入有效端口号', 'warn'); break; }
        state.monitor.add(p); saveMonitor();
        if (state.page !== 'monitor') goPage('monitor');
        renderMonitorChips();
        toast(`已添加监控端口 ${p}`);
        break;
      }
      case 'remove-monitor': {
        const p = Number(hit.dataset.port);
        state.monitor.delete(p); delete state.monitorState[p]; saveMonitor();
        renderMonitorChips();
        break;
      }
      case 'clear-monitor': {
        if (!state.events.length) break;
        const ok = await confirmBox({ title: '清空监控事件？', body: '只清除时间轴记录，不会停止正在监控的端口。', okText: '清空' });
        if (ok) { state.events = []; saveMonitor(); renderMonitor(); }
        break;
      }
      case 'clear-history': {
        const ok = await confirmBox({ title: '清空操作历史？', body: '历史记录清空后无法恢复。', okText: '清空' });
        if (ok) { await invoke('history_clear'); toast('历史已清空'); renderHistory(); }
        break;
      }
      case 'clear-filter': state.filter = ''; $('#globalSearch').value = ''; renderList(); break;
      case 'close-drawer': closeDrawer(); break;
      case 'modal-ok': closeModal(true); break;
      case 'modal-cancel': closeModal(false); break;
      case 'copy': {
        try { await navigator.clipboard.writeText(hit.dataset.text || ''); toast('已复制'); }
        catch (_) { toast('复制失败', 'error'); }
        break;
      }
      case 'install-path': {
        loading(true, '正在安装 port 命令…');
        try { const ok = await invoke('install_path'); toast(ok ? 'port 命令已安装 / 已修复' : '安装失败，请用管理员身份运行', ok ? 'success' : 'error'); }
        finally { loading(false); }
        break;
      }
      case 'reload-config': {
        await loadConfig();
        toast('配置已重新加载');
        if (state.page === 'settings') renderSettings();
        break;
      }
      case 'quit-app': {
        const ok = await confirmBox({ title: '退出 PortLens？', body: '图形界面将关闭，托盘图标也会移除。', okText: '退出' });
        if (ok) invoke('quit_app');
        break;
      }
      default: break;
    }
  });

  // 表格：排序、勾选
  document.addEventListener('click', (ev) => {
    const th = ev.target.closest('th.sortable');
    if (th) {
      const key = th.dataset.sort;
      state.sort = { key, dir: state.sort.key === key ? -state.sort.dir : 1 };
      renderList();
      return;
    }
    if (ev.target.id === 'checkAll') {
      const rows = filteredRows();
      if (ev.target.checked) rows.forEach(r => state.selected.add(r.pid + ':' + r.port));
      else state.selected.clear();
      renderList();
      return;
    }
    const cb = ev.target.closest('.row-check');
    if (cb) {
      const tr = cb.closest('tr');
      const key = tr.dataset.key;
      if (cb.checked) state.selected.add(key); else state.selected.delete(key);
      tr.classList.toggle('selected', cb.checked);
      updateSelCount();
    }
  });

  // 搜索
  const applyFilter = () => {
    state.filter = $('#globalSearch').value;
    if (state.page === 'list') renderList();
    if (state.page === 'map') renderMap();
  };
  $('#globalSearch').addEventListener('input', applyFilter);
  $('#globalSearch').addEventListener('keydown', async (ev) => {
    if (ev.key !== 'Enter') return;
    const v = $('#globalSearch').value.trim();
    if (!v) return;
    if (/^\d+$/.test(v)) { $('#detectInput').value = v; goPage('detect'); await runDetect(v); return; }
    // 按进程名过滤列表
    goPage('list');
    renderList();
  });

  $('#detectInput').addEventListener('keydown', (ev) => { if (ev.key === 'Enter') runDetect(ev.target.value); });
  $('#monitorInput').addEventListener('keydown', (ev) => {
    if (ev.key !== 'Enter') return;
    const p = Number(ev.target.value);
    if (p >= 1 && p <= 65535) {
      state.monitor.add(p); saveMonitor(); ev.target.value = ''; renderMonitorChips(); toast(`已添加监控端口 ${p}`);
    }
  });

  // 自动刷新
  $('#autoRefresh').addEventListener('change', (ev) => {
    state.auto = ev.target.checked;
    clearInterval(state.autoTimer);
    state.autoTimer = null;
    if (state.auto) {
      const interval = Math.max(1500, (state.config && state.config.watchInterval) || 1000);
      state.autoTimer = setInterval(() => refresh(false), interval);
      toast(`自动刷新已开启（${interval} ms）`);
    }
  });

  // 快捷键
  document.addEventListener('keydown', (ev) => {
    if (ev.key === 'Escape') { closeDrawer(); closeModal(false); }
    if ((ev.ctrlKey || ev.metaKey) && ev.key.toLowerCase() === 'r') { ev.preventDefault(); refresh(true); }
    if ((ev.ctrlKey || ev.metaKey) && ev.key.toLowerCase() === 'f') { ev.preventDefault(); $('#globalSearch').focus(); }
  });

  window.addEventListener('resize', () => {
    if (state.page === 'map') drawLinks();
  });

  // ---------- 启动 ----------
  async function loadConfig() {
    try { state.config = await invoke('config_info'); } catch (_) { /* 保持缓存 */ }
  }

  async function boot() {
    if (!IS_TAURI) {
      const badge = $('#previewBadge');
      if (badge) badge.hidden = false;
    }
    loadMonitor();
    await loadConfig();
    const ver = await invoke('app_version').catch(() => '0.8.0');
    $('#appVersion').textContent = ver || '0.8.0';
    renderAliasChips();
    await refresh(true);
    renderMonitorChips();
    startMonitorPolling();
  }

  boot();
})();
