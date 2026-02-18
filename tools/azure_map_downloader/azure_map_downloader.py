import os
import sys
import math
import argparse
import concurrent.futures
from azure.core.credentials import AzureKeyCredential
from azure.maps.render import MapsRenderClient, TilesetID
from azure.core.exceptions import HttpResponseError

# -------------------------------------------------------------------------
# Helper: Mercator Projection Math
# -------------------------------------------------------------------------
def deg2num(lat_deg, lon_deg, zoom):
    """
    Converts Lat/Lon to Tile X/Y coordinates for a given zoom level.
    Based on Web Mercator projection.
    """
    lat_rad = math.radians(lat_deg)
    n = 2.0 ** zoom
    xtile = int((lon_deg + 180.0) / 360.0 * n)
    ytile = int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n)
    return (xtile, ytile)

# -------------------------------------------------------------------------
# Worker Function
# -------------------------------------------------------------------------
def download_tile(client, z, x, y, output_dir, tileset_id, force=False):
    """
    Downloads a single tile and saves it to output_dir/{z}/{x}/{y}.png
    Checks if file exists to prevent redundant downloads unless force is True.
    """
    # Create directory structure: output_dir/z/x/
    dir_path = os.path.join(output_dir, str(z), str(x))
    os.makedirs(dir_path, exist_ok=True)

    file_path = os.path.join(dir_path, f"{y}.png")

    # Check if already exists
    if not force and os.path.exists(file_path):
        # We also check if the file size is > 0 to ensure we don't skip empty/corrupted files
        if os.path.getsize(file_path) > 0:
            return f"Skipped (Exists): z={z} x={x} y={y}"

    try:
        # Request the tile
        # We request format="png" to ensure we get raster images.
        # If tileset is MICROSOFT_IMAGERY, this might return JPEG masquerading as PNG or actual PNG depending on Azure's config,
        # but we save it as-is.
        result = client.get_map_tile(
            tileset_id=tileset_id,
            z=z,
            x=x,
            y=y,
            tile_size="256",
        )

        # Write binary content directly without validation
        with open(file_path, "wb") as file:
            for chunk in result:
                file.write(chunk)

        return f"Downloaded: z={z} x={x} y={y}"

    except HttpResponseError as e:
        return f"Failed ({e.status_code}): z={z} x={x} y={y} - {e.message}"
    except Exception as e:
        return f"Error: z={z} x={x} y={y} - {str(e)}"

# -------------------------------------------------------------------------
# Main CLI Logic
# -------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="Download Azure Map tiles for a Bounding Box across multiple zoom levels.")

    # Auth
    parser.add_argument("--key", required=False, help="Azure Maps Subscription Key. Defaults to env var AZURE_SUBSCRIPTION_KEY.")

    # Bounding Box
    parser.add_argument("--min-lat", type=float, required=True, help="South latitude")
    parser.add_argument("--min-lon", type=float, required=True, help="West longitude")
    parser.add_argument("--max-lat", type=float, required=True, help="North latitude")
    parser.add_argument("--max-lon", type=float, required=True, help="East longitude")

    # Zoom Range
    parser.add_argument("--min-zoom", type=int, required=True, help="Minimum zoom level (e.g., 5)")
    parser.add_argument("--max-zoom", type=int, required=True, help="Maximum zoom level (e.g., 10)")

    # Output & Config
    parser.add_argument("--output", default="tiles", help="Output directory folder (default: tiles)")
    parser.add_argument("--threads", type=int, default=4, help="Number of concurrent download threads")
    parser.add_argument("--force", action="store_true", help="Force re-download of existing tiles")

    args = parser.parse_args()

    # 1. Validation
    subscription_key = args.key or os.getenv("AZURE_SUBSCRIPTION_KEY")
    if not subscription_key:
        print("Error: No subscription key provided via --key or AZURE_SUBSCRIPTION_KEY environment variable.")
        return

    if args.min_zoom > args.max_zoom:
        print(f"Error: min-zoom ({args.min_zoom}) cannot be greater than max-zoom ({args.max_zoom}).")
        return

    if args.max_zoom > 22:
        print("Warning: Zoom levels above 22 might not be available for all tilesets.")

    # 2. Initialize Client
    try:
        credential = AzureKeyCredential(subscription_key)
        maps_client = MapsRenderClient(credential=credential)
    except Exception as e:
        print(f"Error initializing Azure client: {e}")
        return

    # 3. Calculate all tiles to be downloaded
    tileset = TilesetID.MICROSOFT_IMAGERY

    tasks_metadata = [] # Stores (z, x, y) tuples
    total_tile_count = 0

    print(f"Calculating tiles for BBox [{args.min_lat}, {args.min_lon}] to [{args.max_lat}, {args.max_lon}]...")

    for z in range(args.min_zoom, args.max_zoom + 1):
        # Calculate X,Y bounds for this specific zoom level
        # Top Left (North West)
        min_x, min_y_raw = deg2num(args.max_lat, args.min_lon, z)
        # Bottom Right (South East)
        max_x, max_y_raw = deg2num(args.min_lat, args.max_lon, z)

        # Correct min/max ordering just in case
        start_x, end_x = min(min_x, max_x), max(min_x, max_x)
        start_y, end_y = min(min_y_raw, max_y_raw), max(min_y_raw, max_y_raw)

        count_for_level = (end_x - start_x + 1) * (end_y - start_y + 1)
        total_tile_count += count_for_level

        # Store the range for execution later
        tasks_metadata.append({
            'z': z,
            'x_range': range(start_x, end_x + 1),
            'y_range': range(start_y, end_y + 1)
        })

    print(f"Zoom Levels: {args.min_zoom}-{args.max_zoom}")
    print(f"Total tiles to download: {total_tile_count}")

    if total_tile_count == 0:
        print("No tiles found in this region. Check your coordinates.")
        return

    # 4. Execute Downloads
    print(f"Starting download with {args.threads} threads...")
    if args.force:
        print("Force mode enabled: Existing tiles will be overwritten.")

    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.threads) as executor:
            futures = []

            # Submit all tasks
            for meta in tasks_metadata:
                z = meta['z']
                for x in meta['x_range']:
                    for y in meta['y_range']:
                        futures.append(
                            executor.submit(download_tile, maps_client, z, x, y, args.output, tileset, args.force)
                        )

            # Process results as they complete
            completed_count = 0
            for future in concurrent.futures.as_completed(futures):
                result = future.result()
                completed_count += 1
                # Simple progress output
                # if completed_count % 10 == 0 or completed_count == total_tile_count:
                print(f"[{completed_count}/{total_tile_count}] ... {result}")

    except KeyboardInterrupt:
        print("\n\n[!] Operation cancelled by user (Ctrl+C).")
        print("Cancelling pending downloads (waiting for active threads to finish)...")

        for f in futures:
            f.cancel()

        sys.exit(0)

    print("\nDownload complete.")

if __name__ == "__main__":
    main()
