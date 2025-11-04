export const state = {
    settings: null,
};

let websocket;

export function initWebSocket() {
    const wsURL = `ws://${window.location.host}/ws`;
    websocket = new WebSocket(wsURL);
    websocket.onopen = () => {};
    websocket.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            document.dispatchEvent(new CustomEvent('sensorData', { detail: data }));
        } catch (e) {}
    };
    websocket.onclose = () => {
        setTimeout(initWebSocket, 3000);
    };
    websocket.onerror = (error) => {
        websocket.close();
    };
}

export async function sendCommand(command, payload = {}) {
    if (command === 'toggleMute') state.settings.is_muted = !state.settings.is_muted;
    if (command === 'toggleStream') state.settings.stream_active = !state.settings.stream_active;

    try {
        await fetch('/api/action', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: command, ...payload })
        });
    } catch (err) {}
}

export async function getSnapshot(params) {
    try {
        const response = await fetch('/api/snapshot');
        if (!response.ok) return null;
        const blob = await response.blob();
        return URL.createObjectURL(blob);
    } catch (err) {
        return null;
    }
}

export async function loadSettings() {
    if (state.settings) {
        return state.settings;
    }
    try {
        const response = await fetch('/api/settings');
        if (!response.ok) throw new Error('Network error');
        state.settings = await response.json();
        return state.settings;
    } catch (err) {
        return null;
    }
}

export async function saveSettings(settingsData) {
    try {
        const response = await fetch('/api/settings', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(settingsData)
        });
        if (!response.ok) throw new Error('Save failed');
        state.settings = settingsData;
        document.documentElement.style.setProperty('--transform-main', `scale(${settingsData.flip_h ? -1 : 1}, ${settingsData.flip_v ? -1 : 1})`);
        return true;
    } catch (err) {
        return false;
    }
}

export function getDefaultSettings() {
    return {
        thresh_yellow: 2.0, thresh_orange: 1.0, thresh_red: 0.5,
        bpm_min: 0, bpm_max: 300, auto_start: true,
        show_grid: true, cam_angle: 45, grid_opacity: 0.8, 
        grid_offset_x: 0, grid_offset_y: 0, grid_offset_z: 0,
        resolution: "XGA", jpeg_quality: 12, flip_h: true, flip_v: false,
        is_muted: false, stream_active: true,
        wifi_ssid: 'ESP32_AP', wifi_pass: '12345678',
    };
}