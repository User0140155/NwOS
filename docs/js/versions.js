(function () {
    const list = document.querySelector('.versionss');
    if (!list) return;
    const btn = document.getElementById('versionsToggle');
    const lead = document.getElementById('versionsLead');

    let currentExtras = [];
    let open = false;

    function initToggle() {
        currentExtras = Array.prototype.slice.call(list.querySelectorAll('.version-row.extra'));
        if (!currentExtras.length || !btn) {
            if (btn) btn.style.display = 'none';
            return;
        }
        btn.style.display = '';
        open = false;
        btn.textContent = 'Show all versions';
        currentExtras.forEach(function (row, i) {
            row.style.transitionDelay = (i * 0.03) + 's';
            row.classList.add('extra-collapsed');
        });

        if (btn.dataset.bound) return;
        btn.dataset.bound = '1';
        btn.addEventListener('click', function () {
            open = !open;
            btn.textContent = open ? 'Show fewer versions' : 'Show all versions';
            currentExtras.forEach(function (row) {
                row.classList.toggle('extra-collapsed', !open);
            });
        });
    }

    initToggle();

    function semverKey(tag) {
        const p = tag.replace(/^v/i, '').split(/[.\-+]/);
        return (Number(p[0]) || 0) * 1000000 +
               (Number(p[1]) || 0) * 1000 +
               (Number(p[2]) || 0);
    }

    function badgeFor(index, total) {
        if (index === 0) return { text: 'Last', cls: 'lastv' };
        if (index > 0 && index < 4) return { text: 'Previous', cls: 'prev' };
        if (index === total - 1) return { text: 'First version', cls: 'old' };
        if (index >= total - 4 && index <= total - 2) return { text: 'First versions', cls: 'old' };
        return { text: 'Old', cls: 'old' };
    }

    function escapeHtml(s) {
        return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    }

    const OLD_REPO = 'User014015/NwOS';
    const NEW_REPO = 'User0140155/NwOS';
    const CUT = 'v1.4.6';

    function getReleases(repo) {
        return fetch('https://api.github.com/repos/' + repo + '/releases?per_page=100', {
            headers: { 'Accept': 'application/vnd.github+json' }
        }).then(function (res) { return res.ok ? res.json() : Promise.reject(res.status); });
    }

    const CACHE_KEY = 'nwos-releases-cache';
    const TTL = 30 * 60 * 1000;

    function render(items) {
        let html = '';
        items.forEach(function (it, i) {
            const b = badgeFor(i, items.length);
            html += '<a href="' + it.url + '" class="version-row' + (i < 5 ? '' : ' extra') + '">' +
                    '<span class="version-num">' + escapeHtml(it.tag) + '</span>' +
                    '<span class="' + b.cls + '">' + b.text + '</span></a>';
        });
        list.innerHTML = html;

        if (lead && items.length > 1) {
            lead.textContent = 'All releases from ' + items[items.length - 1].tag +
                               ' to ' + items[0].tag + ' on GitHub';
        }

        initToggle();
    }

    const cached = loadCache();
    if (cached) render(cached);

    Promise.all([getReleases(OLD_REPO), getReleases(NEW_REPO)])
        .then(function (results) {
            const raw = []
                .concat(Array.isArray(results[1]) ? results[1] : [])
                .concat(Array.isArray(results[0]) ? results[0] : []);

            const seen = {};
            const items = raw.filter(function (r) {
                if (!r.tag_name || seen[r.tag_name]) return false;
                const fromNew = r.html_url && r.html_url.indexOf(NEW_REPO) !== -1;
                if (!fromNew && semverKey(r.tag_name) > semverKey(CUT)) return false;
                seen[r.tag_name] = true;
                return true;
            }).map(function (r) {
                return { tag: r.tag_name, url: r.html_url, date: r.published_at || r.created_at || '' };
            });

            items.sort(function (a, b) {
                const d = semverKey(b.tag) - semverKey(a.tag);
                if (d !== 0) return d;
                return new Date(b.date) - new Date(a.date);
            });

            render(items);
            return items;
        })
        .then(function (items) { if (items) saveCache(items); })
        .catch(function () {});

    function loadCache() {
        try {
            const c = JSON.parse(localStorage.getItem(CACHE_KEY));
            if (c && Date.now() - c.t < TTL && Array.isArray(c.d) && c.d.length) return c.d;
        } catch (e) {}
        return null;
    }

    function saveCache(items) {
        try {
            localStorage.setItem(CACHE_KEY, JSON.stringify({ t: Date.now(), d: items }));
        } catch (e) {}
    }
})();
