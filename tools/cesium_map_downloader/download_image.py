import argparse
import math
import os
import requests
import concurrent.futures
from tqdm import tqdm
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry

# Constants from the Bing Maps documentation
MIN_LATITUDE = -85.05112878
MAX_LATITUDE = 85.05112878
MIN_LONGITUDE = -180
MAX_LONGITUDE = 180

def clip(n, min_value, max_value):
    """Clips a number to be within a specified range."""
    return max(min_value, min(n, max_value))

def map_size(level_of_detail):
    """Determines the map width and height in pixels at a specific zoom level."""
    return 256 << level_of_detail

def lat_long_to_pixel_xy(latitude, longitude, level_of_detail):
    """
    Converts a point from latitude/longitude WGS-84 coordinates (in degrees)
    into pixel XY coordinates at a specified level of detail.
    """
    latitude = clip(latitude, MIN_LATITUDE, MAX_LATITUDE)
    longitude = clip(longitude, MIN_LONGITUDE, MAX_LONGITUDE)

    x = (longitude + 180) / 360
    sin_latitude = math.sin(latitude * math.pi / 180)
    y = 0.5 - math.log((1 + sin_latitude) / (1 - sin_latitude)) / (4 * math.pi)

    size = map_size(level_of_detail)
    pixel_x = int(clip(x * size + 0.5, 0, size - 1))
    pixel_y = int(clip(y * size + 0.5, 0, size - 1))
    
    return pixel_x, pixel_y

def pixel_xy_to_tile_xy(pixel_x, pixel_y):
    """
    Converts pixel XY coordinates into tile XY coordinates of the tile containing
    the specified pixel.
    """
    tile_x = pixel_x // 256
    tile_y = pixel_y // 256
    return tile_x, tile_y

def tile_xy_to_quadkey(tile_x, tile_y, level_of_detail):
    """
    Converts tile XY coordinates into a QuadKey at a specified level of detail.
    """
    quadkey = []
    for i in range(level_of_detail, 0, -1):
        digit = 0
        mask = 1 << (i - 1)
        if (tile_x & mask) != 0:
            digit += 1
        if (tile_y & mask) != 0:
            digit += 2
        quadkey.append(str(digit))
    return "".join(quadkey)

def download_tile(session, url, save_path):
    """
    Downloads a single tile using a requests.Session object and saves it.
    Includes a timeout for the request.
    """
    try:
        # Ensure the directory exists
        os.makedirs(os.path.dirname(save_path), exist_ok=True)
        
        # Download the tile if it doesn't already exist
        if not os.path.exists(save_path):
            # Use the session object to make the request, which reuses connections
            # Added a timeout (connect, read) to prevent hanging
            response = session.get(url, stream=True, timeout=(5, 15))
            response.raise_for_status()  # Will raise an HTTPError for bad responses (4xx or 5xx)
            with open(save_path, 'wb') as f:
                for chunk in response.iter_content(chunk_size=8192):
                    f.write(chunk)
        return True
    except requests.exceptions.RequestException as e:
        # Log the error but don't crash the whole process
        tqdm.write(f"Error downloading {url}: {e}")
        return False

def main():
    """Main function to parse arguments and start the download process."""
    parser = argparse.ArgumentParser(
        description="Download Bing Maps Aerial tiles for offline use via Cesium ion.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument(
        '--bbox', type=float, nargs=4, metavar=('MIN_LON', 'MIN_LAT', 'MAX_LON', 'MAX_LAT'),
        default=None,
        help="Optional bounding box. If not specified, all available tiles will be downloaded.\nExample: --bbox -74.02 40.70 -73.98 40.72"
    )
    parser.add_argument(
        '--min-zoom', type=int, default=0,
        help="Minimum zoom level to download (inclusive, default: 0)."
    )
    parser.add_argument(
        '--max-zoom', type=int, default=14,
        help="Maximum zoom level to download (inclusive, default: 14)."
    )
    parser.add_argument(
        '--asset-id', type=int, default=2,
        help="Cesium ion asset ID (default: 2 for Bing Maps Aerial)."
    )
    parser.add_argument(
        '--access-token', type=str, required=True,
        help="Your main Cesium ion access token (this is required)."
    )
    parser.add_argument(
        '--output-dir', type=str, default="bing_aerial_tiles",
        help="The directory to save the downloaded tiles (default: 'bing_aerial_tiles')."
    )
    parser.add_argument(
        '--concurrency', type=int, default=20,
        help="Number of concurrent download threads (default: 20)."
    )

    args = parser.parse_args()

    # --- Setup Session with Retries ---
    session = requests.Session()
    # Configure a retry strategy
    retry_strategy = Retry(
        total=3,  # Total number of retries
        status_forcelist=[429, 500, 502, 503, 504],  # A set of HTTP status codes to retry on
        backoff_factor=1  # A backoff factor to apply between attempts
    )
    # Mount the retry strategy to the session
    adapter = HTTPAdapter(max_retries=retry_strategy)
    session.mount("http://", adapter)
    session.mount("https://", adapter)
    # --- End Session Setup ---


    # 1. Get Bing Maps key from Cesium ion
    print(f"Fetching metadata for asset {args.asset_id} from Cesium ion...")
    cesium_api_url = f"https://api.cesium.com/v1/assets/{args.asset_id}/endpoint"
    headers = {"Authorization": f"Bearer {args.access_token}"}
    
    try:
        response = session.get(cesium_api_url, headers=headers, timeout=30)
        response.raise_for_status()
        cesium_data = response.json()
        bing_key = cesium_data['options']['key']
        print("Successfully retrieved Bing Maps API key.")
    except requests.exceptions.RequestException as e:
        print(f"Failed to get metadata from Cesium ion: {e}")
        exit(1)
    except KeyError:
        print("Could not find Bing Maps key in Cesium ion response. Is this a Bing Maps asset?")
        exit(1)

    # 2. Get tile URL template from Bing Maps (Virtual Earth) API
    print("Fetching tile metadata from Bing Maps API...")
    bing_metadata_url = f"https://dev.virtualearth.net/REST/v1/Imagery/Metadata/Aerial?output=json&key={bing_key}"
    
    try:
        response = session.get(bing_metadata_url, timeout=30)
        response.raise_for_status()
        bing_metadata = response.json()
        resource = bing_metadata['resourceSets'][0]['resources'][0]
        tile_url_template = resource['imageUrl']
        subdomains = resource['imageUrlSubdomains']
        print("Successfully retrieved tile URL template.")
    except requests.exceptions.RequestException as e:
        print(f"Failed to get metadata from Bing Maps: {e}")
        exit(1)
    except (KeyError, IndexError):
        print("Could not parse tile URL from Bing Maps metadata response.")
        exit(1)

    # 3. Calculate tile ranges and create download list
    tiles_to_download = []
    print(f"Calculating tile range for zoom levels {args.min_zoom} to {args.max_zoom}...")
    for zoom in range(args.min_zoom, args.max_zoom + 1):
        if args.bbox:
            min_lon, min_lat, max_lon, max_lat = args.bbox
            min_pixel_x, min_pixel_y = lat_long_to_pixel_xy(max_lat, min_lon, zoom)
            max_pixel_x, max_pixel_y = lat_long_to_pixel_xy(min_lat, max_lon, zoom)
            
            min_tile_x, min_tile_y = pixel_xy_to_tile_xy(min_pixel_x, min_pixel_y)
            max_tile_x, max_tile_y = pixel_xy_to_tile_xy(max_pixel_x, max_pixel_y)
        else:
            # If no bbox, download all tiles for the zoom level
            num_tiles = 2 ** zoom
            min_tile_x, min_tile_y = 0, 0
            max_tile_x, max_tile_y = num_tiles - 1, num_tiles - 1

        for x in range(min_tile_x, max_tile_x + 1):
            for y in range(min_tile_y, max_tile_y + 1):
                quadkey = tile_xy_to_quadkey(x, y, zoom)
                subdomain = subdomains[(x + y) % len(subdomains)]
                
                url = tile_url_template.replace('{subdomain}', subdomain).replace('{quadkey}', quadkey)
                
                save_path = os.path.join(args.output_dir, str(zoom), str(x), f"{y}.jpeg")
                tiles_to_download.append((url, save_path))

    if not tiles_to_download:
        print("No tiles found for the given bounding box and zoom levels.")
        exit(0)
        
    print(f"Found {len(tiles_to_download)} tiles to download.")

    # 4. Download tiles concurrently
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.concurrency) as executor:
        # Pass the session object to each worker
        futures = {executor.submit(download_tile, session, url, path): (url, path) for url, path in tiles_to_download}
        
        with tqdm(total=len(futures), desc="Downloading Tiles") as pbar:
            for future in concurrent.futures.as_completed(futures):
                pbar.update(1)

    print("Download complete.")

if __name__ == "__main__":
    main()
