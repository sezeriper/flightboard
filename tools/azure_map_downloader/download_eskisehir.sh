#!/bin/bash
python azure_map_downloader.py \
  --min-lat 39.808129 --min-lon 30.497973 \
  --max-lat 39.820784 --max-lon 30.541466 \
  --min-zoom 1 --max-zoom 19 \
  --output "./eskisehir" \
  --threads 8
