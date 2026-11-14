# Report-only cleanup

Scope: only reports/. User-present mentor_update.tex and report_content_draft.tex deletions were retained; useful content is consolidated and rechecked in reports.tex. README was replaced with the current build/evidence guide.

Removed 15 unused report copies/provenance files after checking the unified LaTeX references:

- reports/images/authored_full_ai.png — SHA256 d253f926bd4ca611a0659b7c9e0a48977f94e6d394b7467960bd8317cb0c4a34
- reports/images/authored_patrol.png — SHA256 c37b4f0cad718c9929d5db5b666554c3b9c054b6af1fb077f90c513315a48510
- reports/images/buoyancy_latest.png — SHA256 d29a97a221dc4bc794543c1b8fc1c1bbbe6d8b535c0ec4aff5e7b3997c645bdc
- reports/images/burning_ship.png — SHA256 f7d0faacb750fb79eb4c8e5477852cfaa583b50214915005f303dbf8ca5083a0
- reports/images/global_minimap.png — SHA256 5c162708660faa6643efa2b9509cbca5e29916dcc3be488b2c1e20dbbbd3163f
- reports/images/island_field.png — SHA256 888cab4c80a282d2e7384afddd9e1d3b58b378ebe5b3391cd3516a2c444e66b8
- reports/images/player_defeat.png — SHA256 5b6b5fb2e4db7f07e6086a6899bc598e3f891ea28e01e0c7a5fef49b1054eed0
- reports/images/result_return.png — SHA256 f06d249cbcbb3fd5be7384007d5e45957875ba4840e5f613c19b9dbc19484980
- reports/images/setup_authored.png — SHA256 d0fbf9dd5e6959c8c2ec03cc7a2f99e98db229b9455f27b5f590f2e02645b321
- reports/images/setup_menu.png — SHA256 dde2fd79241ede35ed4a78f279f6c106610e9eac359bd173646eaf69aa4134e6
- reports/images/shoreline_medium.png — SHA256 fedb87b83706d7e00d605040c268190ebb0ebc5bf821e7a485c7a5705ab715b4
- reports/images/shoreline_small.png — SHA256 c8337b2b04ff60e9457562a31ec61947e016e9c995238624d068cdc8b2bd8e09
- reports/images/sinking_ship.png — SHA256 65cae5e6489079ecd57e25adb346f62aa4caf9f8bac79a692a4a2fadbb7a8ab5
- reports/images/small_gun.png — SHA256 9e561a810fc4d9c1c9c8dcbd8f1d436168b7989b979c73d5666889b5cc9b4f42
- reports/images/SOURCES.json — SHA256 2bc9fd88c2022305b061db04bd8e7f02c5b6d402c06c0bcf9966a8234926bb9d

Removed root-generated LaTeX files: reports.aux, reports.log, reports.toc, reports.out, reports.lof, reports.lot, reports.synctex.gz.

Image sources remain in Docs/Saved or Git history; used-image provenance was replaced by IMAGE_INVENTORY.md. No file outside reports was deleted. No recursive deletion was used. Kept unified LaTeX/PDF, supplied logo, referenced images, build script, requested research inventories and ignored build/QA evidence.

Unrelated root .gitignore blank-line change was preserved and excluded from the report commit. A combined cleanup command was rejected by automatic review; the smaller explicit allowlist removals succeeded.
