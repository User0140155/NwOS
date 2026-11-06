/* NwWiki: article loader + search over everything in main/.
 *
 * A static site cannot list a folder, so main/index.json is the list of articles.
 * To add an article: put <name>.html into main/ and add one entry to main/index.json.
 * Search then indexes it automatically (title, headings and full text).
 *
 * Routes (hash):  #install          -> article with id "install"
 *                 #search=keyboard  -> search results page
 */
(function () {
    'use strict';

    var MAIN = 'main/';
    var articleEl = document.getElementById('wikiArticle');
    var sidebarEl = document.getElementById('wikiSidebar');
    var inputEl = document.getElementById('wikiSearch');
    var dropEl = document.getElementById('wikiDropdown');
    var menuBtn = document.getElementById('wikiMenuBtn');

    var pages = [];      // manifest entries + loaded content
    var byId = {};
    var ready = null;    // promise

    function esc(s) {
        return String(s).replace(/[&<>"']/g, function (c) {
            return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
        });
    }

    function slug(s) {
        return s.toLowerCase().replace(/[^a-z0-9а-яё]+/gi, '-').replace(/^-|-$/g, '');
    }

    function fetchText(url) {
        return fetch(url).then(function (r) {
            if (!r.ok) throw new Error(url + ' -> ' + r.status);
            return r.text();
        });
    }

    /* ---------- loading ---------- */

    function load() {
        return fetchText(MAIN + 'index.json')
            .then(function (txt) { return JSON.parse(txt); })
            .then(function (list) {
                pages = list.map(function (p) {
                    p.text = ''; p.html = ''; p.headings = [];
                    byId[p.id] = p;
                    return p;
                });
                return Promise.all(pages.map(function (p) {
                    return fetchText(MAIN + p.file).then(function (html) {
                        p.html = html;
                        var box = document.createElement('div');
                        box.innerHTML = html;
                        p.headings = Array.prototype.map.call(
                            box.querySelectorAll('h2,h3,h4'),
                            function (h) { return h.textContent.trim(); }
                        );
                        p.text = box.textContent.replace(/\s+/g, ' ').trim();
                    }).catch(function () { p.missing = true; });
                }));
            });
    }

    /* ---------- sidebar ---------- */

    function categories() {
        var order = [], map = {};
        pages.forEach(function (p) {
            var c = p.category || 'Other';
            if (!map[c]) { map[c] = []; order.push(c); }
            map[c].push(p);
        });
        return order.map(function (c) { return { name: c, pages: map[c] }; });
    }

    function renderSidebar(activeId) {
        var h = '<h4>NwWiki</h4><ul><li><a href="#home"' + (activeId === 'home' ? ' class="active"' : '') + '>Main page</a></li></ul>';
        categories().forEach(function (c) {
            var items = c.pages.filter(function (p) { return p.id !== 'home'; });
            if (!items.length) return;
            h += '<h4>' + esc(c.name) + '</h4><ul>';
            items.forEach(function (p) {
                h += '<li><a href="#' + esc(p.id) + '"' + (p.id === activeId ? ' class="active"' : '') + '>' + esc(p.title) + '</a></li>';
            });
            h += '</ul>';
        });
        h += '<h4>Links</h4><ul><li><a href="../index.html#versions">Download versions</a></li>' +
             '<li><a href="https://github.com/User0140155/NwOS/">Source code</a></li></ul>';
        sidebarEl.innerHTML = h;
    }

    /* ---------- article view ---------- */

    function buildToc(root) {
        var hs = root.querySelectorAll('h2,h3');
        if (hs.length < 3) return null;
        var used = {};
        var html = '', open3 = false, n2 = 0;
        Array.prototype.forEach.call(hs, function (h) {
            var id = slug(h.textContent) || 'section';
            while (used[id]) id += '-x';
            used[id] = 1;
            h.id = id;
            if (h.tagName === 'H2') {
                if (open3) { html += '</ol></li>'; open3 = false; } else if (n2) { html += '</li>'; }
                n2++;
                html += '<li><a href="#" data-go="' + id + '">' + esc(h.textContent) + '</a>';
            } else {
                if (!n2) return;
                if (!open3) { html += '<ol>'; open3 = true; }
                html += '<li><a href="#" data-go="' + id + '">' + esc(h.textContent) + '</a></li>';
            }
        });
        if (open3) html += '</ol></li>'; else if (n2) html += '</li>';
        var toc = document.createElement('nav');
        toc.className = 'wiki-toc';
        toc.innerHTML = '<strong>Contents</strong><ol>' + html + '</ol>';
        toc.addEventListener('click', function (e) {
            var a = e.target.closest('a[data-go]');
            if (!a) return;
            e.preventDefault();
            var t = document.getElementById(a.getAttribute('data-go'));
            if (t) t.scrollIntoView({ behavior: 'smooth', block: 'start' });
        });
        return toc;
    }

    function showArticle(id, q) {
        var p = byId[id];
        if (!p) return showNotFound(id);
        document.title = p.title + ' — NwWiki';
        renderSidebar(id);

        var body = document.createElement('div');
        body.className = 'wiki-body';
        body.innerHTML = p.missing ? '<div class="box warning"><span class="box-title">Missing</span>' +
            '<p>File <code>main/' + esc(p.file) + '</code> could not be loaded.</p></div>' : p.html;

        // internal links like <a href="#keyboard"> already work via hashchange
        var toc = id === 'home' ? null : buildToc(body);

        var out = '';
        if (id !== 'home') {
            out += '<div class="wiki-breadcrumb"><a href="#home">NwWiki</a> &rsaquo; ' + esc(p.category || 'Other') + '</div>';
        }
        if (id === 'home') {
            out += '<div class="wiki-hero"><h1>NwWiki</h1><p>' + esc(p.summary || '') + '</p></div>';
        } else {
            out += '<h1 class="wiki-title">' + esc(p.title) + '</h1>';
            if (p.summary) out += '<div class="wiki-meta">' + esc(p.summary) + '</div>';
        }

        articleEl.innerHTML = out;
        if (id === 'home') articleEl.appendChild(homeHeader());
        if (toc) articleEl.appendChild(toc);
        articleEl.appendChild(body);
        if (id === 'home') body.appendChild(homeCategories());

        addCopyButtons(body);
        if (q) highlight(body, q);
        window.scrollTo(0, 0);
        sidebarEl.classList.remove('open');
    }

    function addCopyButtons(root) {
        root.querySelectorAll('pre').forEach(function (pre) {
            var b = document.createElement('button');
            b.type = 'button';
            b.className = 'copy-btn';
            b.textContent = 'Copy';
            b.addEventListener('click', function () {
                var text = pre.querySelector('code') ? pre.querySelector('code').textContent : pre.textContent;
                var done = function () { b.textContent = 'Copied'; setTimeout(function () { b.textContent = 'Copy'; }, 1200); };
                if (navigator.clipboard) navigator.clipboard.writeText(text).then(done, function () {});
            });
            pre.appendChild(b);
        });
    }

    function homeHeader() {
        var d = document.createElement('div');
        d.className = 'wiki-search-big';
        d.innerHTML = '<input id="wikiSearchBig" type="search" placeholder="Search all guides (e.g. install, keyboard, VBE…)" autocomplete="off" aria-label="Search NwWiki">';
        d.querySelector('input').addEventListener('keydown', function (e) {
            if (e.key === 'Enter' && this.value.trim()) location.hash = 'search=' + encodeURIComponent(this.value.trim());
        });
        return d;
    }

    function homeCategories() {
        var d = document.createElement('div');
        d.className = 'cat-grid';
        categories().forEach(function (c) {
            var items = c.pages.filter(function (p) { return p.id !== 'home'; });
            if (!items.length) return;
            var card = '<div class="cat-card"><h3>' + esc(c.name) + '</h3><ul>';
            items.forEach(function (p) {
                card += '<li><a href="#' + esc(p.id) + '">' + esc(p.title) + '</a>' +
                    (p.summary ? '<br><small style="color:rgb(140,140,140)">' + esc(p.summary) + '</small>' : '') + '</li>';
            });
            d.insertAdjacentHTML('beforeend', card + '</ul></div>');
        });
        return d;
    }

    function showNotFound(id) {
        renderSidebar('');
        articleEl.innerHTML = '<h1 class="wiki-title">Page not found</h1><div class="wiki-body">' +
            '<p>There is no article <code>' + esc(id) + '</code>. Try the search or go to the <a href="#home">main page</a>.</p></div>';
    }

    /* ---------- search ---------- */

    function terms(q) {
        return q.toLowerCase().split(/[\s,.;:!?()]+/).filter(function (t) { return t.length > 0; });
    }

    function countIn(hay, needle) {
        var c = 0, i = 0;
        while ((i = hay.indexOf(needle, i)) !== -1) { c++; i += needle.length; }
        return c;
    }

    function search(q) {
        var ts = terms(q);
        if (!ts.length) return [];
        var res = [];
        pages.forEach(function (p) {
            if (p.missing) return;
            var title = p.title.toLowerCase();
            var heads = p.headings.join(' | ').toLowerCase();
            var meta = ((p.summary || '') + ' ' + (p.category || '') + ' ' + (p.keywords || []).join(' ')).toLowerCase();
            var text = p.text.toLowerCase();
            var score = 0, all = true;
            ts.forEach(function (t) {
                var s = 0;
                if (title.indexOf(t) !== -1) s += 12;
                if (title === t) s += 8;
                s += Math.min(countIn(heads, t), 3) * 5;
                if (meta.indexOf(t) !== -1) s += 6;
                s += Math.min(countIn(text, t), 10);
                if (!s) all = false;
                score += s;
            });
            if (all && score) res.push({ page: p, score: score });
        });
        res.sort(function (a, b) { return b.score - a.score; });
        return res;
    }

    function snippet(p, ts) {
        var low = p.text.toLowerCase(), pos = -1;
        for (var i = 0; i < ts.length && pos < 0; i++) pos = low.indexOf(ts[i]);
        if (pos < 0) return esc((p.summary || p.text).slice(0, 150));
        var start = Math.max(0, pos - 60), end = Math.min(p.text.length, pos + 120);
        var s = (start > 0 ? '…' : '') + p.text.slice(start, end) + (end < p.text.length ? '…' : '');
        return markTerms(esc(s), ts);
    }

    function markTerms(html, ts) {
        ts.forEach(function (t) {
            var re = new RegExp('(' + t.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + ')', 'gi');
            html = html.replace(re, '<mark>$1</mark>');
        });
        return html;
    }

    function highlight(root, q) {
        var ts = terms(q);
        if (!ts.length) return;
        var re = new RegExp('(' + ts.map(function (t) { return t.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'); }).join('|') + ')', 'gi');
        var walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, null);
        var nodes = [], n;
        while ((n = walker.nextNode())) {
            if (n.parentNode.closest('pre,script,style')) continue;
            if (re.test(n.nodeValue)) nodes.push(n);
            re.lastIndex = 0;
        }
        nodes.forEach(function (node) {
            var span = document.createElement('span');
            span.innerHTML = esc(node.nodeValue).replace(re, '<mark>$1</mark>');
            node.parentNode.replaceChild(span, node);
        });
    }

    function showSearch(q) {
        document.title = 'Search: ' + q + ' — NwWiki';
        renderSidebar('');
        var res = search(q), ts = terms(q);
        var h = '<h1 class="wiki-title">Search</h1>';
        h += '<div class="wiki-search-big"><input id="wikiSearchBig" type="search" value="' + esc(q) + '" autocomplete="off" aria-label="Search NwWiki"></div>';
        h += '<p class="wiki-status">' + res.length + ' result' + (res.length === 1 ? '' : 's') + ' for “' + esc(q) + '”</p>';
        res.forEach(function (r) {
            var p = r.page;
            h += '<div class="result"><a class="r-title" href="#' + esc(p.id) + '" data-q="' + esc(q) + '">' + markTerms(esc(p.title), ts) + '</a>' +
                '<span class="r-cat">' + esc(p.category || 'Other') + '</span><p>' + snippet(p, ts) + '</p></div>';
        });
        if (!res.length) h += '<div class="wiki-body"><div class="box note"><span class="box-title">No matches</span><p>Try fewer or different words, or browse the menu.</p></div></div>';
        articleEl.innerHTML = h;
        var big = document.getElementById('wikiSearchBig');
        big.addEventListener('keydown', function (e) {
            if (e.key === 'Enter' && this.value.trim()) location.hash = 'search=' + encodeURIComponent(this.value.trim());
        });
        articleEl.querySelectorAll('a.r-title').forEach(function (a) {
            a.addEventListener('click', function () { pendingHighlight = a.getAttribute('data-q'); });
        });
        window.scrollTo(0, 0);
    }

    /* ---------- dropdown ---------- */

    var sel = -1;

    function updateDropdown() {
        var q = inputEl.value.trim();
        if (!q || !ready) { dropEl.hidden = true; return; }
        ready.then(function () {
            var res = search(q).slice(0, 6), ts = terms(q);
            if (!res.length) {
                dropEl.innerHTML = '<a href="#search=' + encodeURIComponent(q) + '">No matches — open full search</a>';
            } else {
                dropEl.innerHTML = res.map(function (r) {
                    return '<a href="#' + esc(r.page.id) + '" data-q="' + esc(q) + '">' + esc(r.page.title) +
                        '<small>' + esc(r.page.category || '') + '</small></a>';
                }).join('') + '<a class="more" href="#search=' + encodeURIComponent(q) + '">All results &rarr;</a>';
            }
            dropEl.hidden = false;
            sel = -1;
        });
    }

    inputEl.addEventListener('input', updateDropdown);
    inputEl.addEventListener('focus', updateDropdown);
    inputEl.addEventListener('keydown', function (e) {
        var links = dropEl.querySelectorAll('a');
        if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
            e.preventDefault();
            if (!links.length) return;
            sel = (sel + (e.key === 'ArrowDown' ? 1 : -1) + links.length) % links.length;
            links.forEach(function (a, i) { a.classList.toggle('sel', i === sel); });
        } else if (e.key === 'Enter') {
            e.preventDefault();
            if (sel >= 0 && links[sel]) { links[sel].click(); }
            else if (inputEl.value.trim()) location.hash = 'search=' + encodeURIComponent(inputEl.value.trim());
            dropEl.hidden = true;
            inputEl.blur();
        } else if (e.key === 'Escape') {
            dropEl.hidden = true;
            inputEl.blur();
        }
    });
    dropEl.addEventListener('click', function (e) {
        var a = e.target.closest('a');
        if (a && a.getAttribute('data-q')) pendingHighlight = a.getAttribute('data-q');
        dropEl.hidden = true;
    });
    document.addEventListener('click', function (e) {
        if (!e.target.closest('.wiki-header-search')) dropEl.hidden = true;
    });
    document.addEventListener('keydown', function (e) {
        if (e.key === '/' && !/^(INPUT|TEXTAREA)$/.test(document.activeElement.tagName)) {
            e.preventDefault();
            inputEl.focus();
            inputEl.select();
        }
    });

    if (menuBtn) menuBtn.addEventListener('click', function () { sidebarEl.classList.toggle('open'); });

    /* ---------- routing ---------- */

    var pendingHighlight = '';

    function route() {
        var h = decodeURIComponent(location.hash.replace(/^#/, ''));
        if (!ready) return;
        ready.then(function () {
            if (h.indexOf('search=') === 0) {
                inputEl.value = h.slice(7);
                showSearch(h.slice(7));
            } else {
                var q = pendingHighlight; pendingHighlight = '';
                showArticle(h || 'home', q);
            }
        });
    }

    window.addEventListener('hashchange', route);

    ready = load().catch(function (err) {
        var local = location.protocol === 'file:';
        sidebarEl.innerHTML = '';
        articleEl.innerHTML = '<h1 class="wiki-title">NwWiki</h1><div class="wiki-body"><div class="box warning">' +
            '<span class="box-title">Could not load the articles</span><p>' + esc(err.message) + '</p>' +
            '<p>Check the layout: <code>NwWiki/main/index.json</code> (the list) and the article files must both be inside <code>NwWiki/main/</code>.</p>' +
            (local ? '<p>The page is opened from <code>file://</code>, where browsers block <code>fetch()</code>. ' +
                'Run a local server in the <code>docs/</code> folder, e.g. <code>python3 -m http.server</code>, and open <code>http://localhost:8000/NwWiki/nwWiki.html</code>.</p>' : '') +
            '</div></div>';
        throw err;
    });
    ready.then(route, function () { /* error already shown */ });
})();