(function () {
    const canvas = document.getElementById('pixelCanvas');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const hero = canvas.parentElement;

    const CELL = 22;
    const SIZE = 10;
    const REACH_CELLS = 4;
    const DUST = ['#2b2b2b', '#3a3a3a', '#474747'];
    const BLOB = ['#5f5f5f', '#7a7a7a', '#969696'];

    let pixels = [];
    let mouse = { x: 0, y: 0 };
    let pos = { x: 0, y: 0 };
    let active = false;

    function cellHash(col, row) {
        const s = Math.sin(col * 12.9898 + row * 78.233) * 43758.5453;
        return s - Math.floor(s);
    }

    function build() {
        canvas.width = hero.clientWidth;
        canvas.height = hero.clientHeight;
        pixels = [];
        const cols = Math.ceil(canvas.width / CELL);
        const rows = Math.ceil(canvas.height / CELL);
        for (let row = 0; row < rows; row++) {
            for (let col = 0; col < cols; col++) {
                if (cellHash(col, row) > 0.13) continue;
                pixels.push({
                    x: col * CELL + CELL / 2,
                    y: row * CELL + CELL / 2,
                    color: DUST[(cellHash(row, col) * DUST.length) | 0]
                });
            }
        }
    }

    function draw() {
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        ctx.fillStyle = '#3a3a3a';
        for (const p of pixels) {
            ctx.globalAlpha = 0.6;
            ctx.fillStyle = p.color;
            ctx.fillRect(p.x - SIZE / 2, p.y - SIZE / 2, SIZE, SIZE);
        }

        if (active) {
            const cCol = Math.floor(pos.x / CELL);
            const cRow = Math.floor(pos.y / CELL);
            for (let dr = -REACH_CELLS; dr <= REACH_CELLS; dr++) {
                for (let dc = -REACH_CELLS; dc <= REACH_CELLS; dc++) {
                    const col = cCol + dc;
                    const row = cRow + dr;
                    const dist = Math.sqrt(dc * dc + dr * dr);
                    if (dist > REACH_CELLS + 0.5) continue;
                    if (cellHash(col, row) > 0.55) continue;
                    const x = col * CELL + CELL / 2;
                    const y = row * CELL + CELL / 2;
                    ctx.globalAlpha = 0.45 + 0.55 * (1 - dist / (REACH_CELLS + 0.5));
                    ctx.fillStyle = BLOB[(cellHash(row, col) * BLOB.length) | 0];
                    ctx.fillRect(x - SIZE / 2, y - SIZE / 2, SIZE, SIZE);
                }
            }
        }

        ctx.globalAlpha = 1;
    }

    function loop() {
        if (active) {
            pos.x += (mouse.x - pos.x) * 0.14;
            pos.y += (mouse.y - pos.y) * 0.14;
        }
        draw();
        requestAnimationFrame(loop);
    }

    hero.addEventListener('mousemove', function (e) {
        const rect = canvas.getBoundingClientRect();
        mouse.x = e.clientX - rect.left;
        mouse.y = e.clientY - rect.top;
        if (!active) { pos.x = mouse.x; pos.y = mouse.y; }
        active = true;
    });

    hero.addEventListener('mouseleave', function () {
        active = false;
    });

    window.addEventListener('resize', build);

    build();
    loop();
})();
