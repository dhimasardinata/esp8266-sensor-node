import requests
from requests.adapters import HTTPAdapter
from requests.packages.urllib3.util.retry import Retry
import sys
import os
import urllib3
import re
import urllib.parse
import json
import argparse
import time
import configparser

# Read Config
config = configparser.ConfigParser()
config.read("node_id.ini")
try:
    NODE_ID = config.get("settings", "node_id")
    FIRMWARE_VERSION = config.get("settings", "firmware_version")
except Exception as e:
    print(f"Warning: Could not read node_id.ini or missing keys: {e}")
    NODE_ID = "5"
    FIRMWARE_VERSION = "0.0.0"

# python scripts/test_ota_upload.py "REDACTED" "REDACTED" "REDACTED" --token "REDACTED"
# Suppress InsecureRequestWarning
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

# Configuration
BASE_URL = "https://example.com"
target_endpoint = "/api/files"


def login_and_upload(
    username, password, file_path, extra_token=REDACTED
):
    if not os.path.exists(file_path):
        print(f"Error: File not found at {file_path}")
        return

    # Setup Session with Retries
    session = requests.Session()
    retry_strategy = Retry(
        total=REDACTED
        backoff_factor=1,
        status_forcelist=[429, 500, 502, 503, 504],
        allowed_methods=["HEAD", "GET", "OPTIONS", "POST"],
    )
    adapter = HTTPAdapter(max_retries=retry_strategy)
    session.mount("https://", adapter)
    session.mount("http://", adapter)

    # Spoof a real browser
    session.headers.update(
        {
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
            "Referer": f"{BASE_URL}/login",
            "Origin": BASE_URL,
            "Accept": "application/json",
            "X-Requested-With": "XMLHttpRequest",
            "Connection": "keep-alive",
        }
    )

    # MANUAL COOKIE OVERRIDE
    if manual_cookie:
        print(f"[*] Logic: Manual Cookie Bypass (Skipping Login)")
        # Parse cookie string "key=value; key2=value2"
        session.headers.update({"Cookie": manual_cookie})

        # Try to extract Xsrf from the manual cookie string if possible
        match = re.search(r"XSRF-TOKEN=([^;]+)", manual_cookie)
        if match:
            decoded = urllib.parse.unquote(match.group(1))
            session.headers.update({"X-XSRF-TOKEN": REDACTED
            print(f"REDACTED")
        else:
            print(
                f"REDACTED"
            )

    else:
        print(f"[*] Logic: Auto-Login + Upload to {target_endpoint}")

    print("-" * 50)

    try:
        # 1. GET Login Page (Only if no manual cookie)
        if not manual_cookie:
            print("1. Fetching Login Page...")
            time.sleep(2)
            r = session.get(f"{BASE_URL}/login", verify=False, timeout=30)

            # DEBUG: Check what we got
            page_title = "Unknown"
            title_match = re.search("<title>(.*?)</title>", r.text, re.IGNORECASE)
            if title_match:
                page_title = title_match.group(1)

            xsrf_token = REDACTED
            if not xsrf_token:
                print("REDACTED")
                print(f"   [DEBUG] Page Title: {page_title}")
                print(f"   [DEBUG] Status Code: {r.status_code}")
                if "Imunify360" in r.text or "Security" in page_title:
                    print("\n   !!! BLOCK DETECTED !!!")
                    print("   Server is serving a Security Challenge page.")
                    print(
                        "   SOLUTION: Copy 'Cookie' from your browser (F12 -> Network -> request header) and use --cookie argument."
                    )
                return

            csrf_token = REDACTED

            # 2. POST Login
            print("2. Logging in...")

            session.headers.update(
                {"X-XSRF-TOKEN": REDACTED
            )

            login_data = {"username": username, "password": password, "remember": True}

            time.sleep(1)
            r_login = session.post(
                f"{BASE_URL}/login", json=login_data, verify=False, timeout=30
            )

            if r_login.status_code in [200, 204, 302]:
                print("   [SUCCESS] Logged in!")
            elif r_login.status_code == 422:
                print("   [FAILED] Login Validation Error.")
                print(r_login.text)
                return
            elif r_login.status_code == 409:
                print(
                    "   [FAILED] 409 Conflict (Inertia Mismatch). Retrying without Inertia Headers..."
                )
                # Logic to handle this if needed, but we disabled inertia headers already
                return
            else:
                print(f"   [ERROR] Login Request Failed: {r_login.status_code}")
                return

            # Refresh Token
            new_xsrf = session.cookies.get("XSRF-TOKEN")
            if new_xsrf:
                csrf_token = REDACTED
                session.headers.update({"X-XSRF-TOKEN": REDACTED

        # 3. Upload File
        if "Content-Type" in session.headers:
            del session.headers["Content-Type"]

        if extra_token:
            session.headers.update({"Authorization": REDACTED

        files = {
            "file": open(file_path, "rb"),
        }

        data = {
            "sensor_id": NODE_ID,
            "status": "1",
            "version": FIRMWARE_VERSION,
            "description": "OTA Update via API",
        }

        url = BASE_URL + target_endpoint
        print(f"REDACTED")
        # print(f"   Payload: {data}")
        files["file"].seek(0)

        time.sleep(1)
        resp = session.post(url, files=files, data=data, verify=False, timeout=60)

        if resp.status_code in [200, 201]:
            print(f"   [SUCCESS] Uploaded! Server response: {resp.text}")
            print("-" * 50)
            print("   READY FOR PRODUCTION!")
            return
        elif resp.status_code == 422:
            print("   [FAILED] 422 Validation Error.")
            print(f"   Response: {resp.text}")
        else:
            print(f"   [FAILED] {resp.status_code} - {resp.text[:200]}")

    except Exception as e:
        print(f"   [ERROR] {str(e)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("username")
    parser.add_argument("REDACTED")
    parser.add_argument("file")
    parser.add_argument("--token", default=REDACTED
    parser.add_argument(
        "--cookie", default=None, help="Manual Cookie String (Bypass Login)"
    )

    args = parser.parse_args()

    login_and_upload(args.username, args.password, args.file, args.token, args.cookie)
