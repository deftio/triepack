/* Language tabs for TriePack API reference.
 * No framework — plain DOM + localStorage for persistence.
 * Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
 */
document.addEventListener('DOMContentLoaded', function () {
    document.body.classList.add('tp-js-enabled');

    document.querySelectorAll('.tp-tabs').forEach(function (tabs) {
        var buttons = tabs.querySelectorAll('.tp-tab-btn');
        var panels = tabs.querySelectorAll('.tp-tab-panel');

        buttons.forEach(function (btn) {
            btn.addEventListener('click', function () {
                var lang = btn.dataset.lang;

                /* Sync every tab group on the page to the chosen language */
                document.querySelectorAll('.tp-tabs').forEach(function (t) {
                    t.querySelectorAll('.tp-tab-btn').forEach(function (b) {
                        b.classList.toggle('active', b.dataset.lang === lang);
                    });
                    t.querySelectorAll('.tp-tab-panel').forEach(function (p) {
                        p.classList.toggle('active', p.dataset.lang === lang);
                    });
                });

                try { localStorage.setItem('tp-lang', lang); } catch (e) { /* private mode */ }
            });
        });
    });

    /* Restore last-selected language */
    var saved = null;
    try { saved = localStorage.getItem('tp-lang'); } catch (e) { /* private mode */ }
    if (saved) {
        var btn = document.querySelector('.tp-tab-btn[data-lang="' + saved + '"]');
        if (btn) btn.click();
    }
});
