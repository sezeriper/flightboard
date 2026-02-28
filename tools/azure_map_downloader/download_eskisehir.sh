#!/bin/bash
python azure_map_downloader.py \
  --min-lat 39.740466 --min-lon 30.467101 \
  --max-lat 39.81863 --max-lon 30.614928 \
  --min-zoom 1 --max-zoom 19 \
  --output "./outputs/eskisehir" \
  --threads 16
