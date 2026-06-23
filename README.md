# DHT11 Dashboard

A web dashboard for an IoT setup built around a DHT11 temperature/humidity sensor.
It reads live sensor data from Firebase Realtime Database, charts it, lets you
toggle connected devices (motor, etc.), and set threshold values that are saved
back to Firebase.

## Live Preview

🔗 **[View live demo](PUT_YOUR_LIVE_URL_HERE)**

> Replace `PUT_YOUR_LIVE_URL_HERE` with your deployed URL (e.g. your GitHub Pages link).

## Features

- Live temperature & humidity readings from Firebase
- Real-time charts (Chart.js)
- Device controls (turn devices on/off)
- Configurable thresholds saved to Firebase

## Tech

- **Dashboard:** plain HTML/CSS/JS — Bootstrap, Chart.js, and Firebase (Realtime Database), all via CDN.
- **Firmware:** ESP32 (`main.cpp`) built with **[PlatformIO](https://platformio.org/)** — see `platformio.ini`. Reads the DHT11, controls LEDs/motor, pushes data to Firebase, and sends Gmail SMTP alerts.

## Dashboard setup

1. Copy the config template and fill in your Firebase project values:
   ```sh
   cp config.example.js config.js
   ```
2. Open `index.html` in a browser (or serve the folder with any static host).

`config.js` is gitignored so your Firebase credentials stay out of the repo.

## Firmware setup (PlatformIO)

1. Copy the secrets template and fill in your credentials (WiFi auth user, Firebase, Gmail App Password):
   ```sh
   cp secrets.example.h secrets.h
   ```
2. Build and upload to the ESP32:
   ```sh
   pio run --target upload
   ```

`secrets.h` is gitignored so your credentials stay out of the repo.

## Deploy (GitHub Pages)

1. Push this repo to GitHub.
2. Settings → Pages → deploy from the `main` branch.
3. Your live URL appears there — paste it into the **Live Preview** section above.
