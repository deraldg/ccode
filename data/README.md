# Sample Data (Curated)

This `data/` folder contains **small** sample tables to try DotTalk++ without extra downloads.
We intentionally keep this set tiny for the public repo.

## What’s included
- A few `.dbf` tables (and matching memo/index files if present: `.dbt`/`.fpt`, `.cdx`/`.idx`/`.inx`/`.cnx`)
- Occasional `.csv` exports for quick inspection

## What’s not included
- Large bundles, archives, or generated exports
- Heavy or proprietary datasets

## Tips
- Use `USE <table>` then `BROWSE`, `LIST`, `COUNT`, `DISPLAY`, etc.
- If you have bigger datasets locally, place them under your working `data/` but don’t commit them.
