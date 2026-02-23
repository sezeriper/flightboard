#!/bin/bash
python azure_map_downloader.py \
  --min-lat "-85.05112878" --min-lon "-180" \
  --max-lat "85.05112878" --max-lon "180" \
  --min-zoom 1 --max-zoom 8 \
  --output "./eskisehir" \
  --threads 16
