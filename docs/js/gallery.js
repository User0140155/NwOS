(function () {
    const slides = document.querySelectorAll('.gallery-slide');
    if (!slides.length) return;

    const prevBtn = document.querySelector('.car-prev');
    const nextBtn = document.querySelector('.car-next');
    const counter = document.querySelector('.car-counter');
    const title = document.querySelector('.car-title');
    const desc = document.querySelector('.car-desc');

    let index = 0;

    function show(i) {
        index = (i + slides.length) % slides.length;

        slides.forEach(function (s, n) {
            s.classList.toggle('active', n === index);
        });

        const slide = slides[index];
        counter.textContent = (index + 1) + ' / ' + slides.length;
        title.textContent = slide.dataset.title;
        desc.textContent = slide.dataset.desc;
    }

    prevBtn.addEventListener('click', function () { show(index - 1); });
    nextBtn.addEventListener('click', function () { show(index + 1); });

    document.addEventListener('keydown', function (e) {
        if (e.key === 'ArrowLeft') show(index - 1);
        if (e.key === 'ArrowRight') show(index + 1);
    });

    show(0);
})();
