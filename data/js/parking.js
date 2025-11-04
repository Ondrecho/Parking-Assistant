import { initWebSocket, sendCommand, loadSettings } from './main.js';

const gridScene = document.getElementById('grid-scene');
const streamOffMessage = document.getElementById('stream-off-message');
const gridPlane = document.getElementById('grid-plane');
const sensorBoxes = [document.getElementById('sensor-1'), document.getElementById('sensor-2'), document.getElementById('sensor-3')];
const toggleMuteBtn = document.getElementById('toggle-mute');
const toggleStreamBtn = document.getElementById('toggle-stream');
const videoStream = document.getElementById('video-stream');
const videoWrapper = document.querySelector('.video-wrapper');

const icons = {
    play: document.getElementById('play-icon'),
    pause: document.getElementById('pause-icon'),
    mute: document.getElementById('mute-icon'),
    unmute: document.getElementById('unmute-icon'),
};

let cells = [];
let settings = {};

async function init() {
    settings = await loadSettings();
    
    if (!settings) {
        document.body.innerHTML = `<div style="color: white; text-align: center; padding: 20px;">Ошибка: не удалось загрузить настройки с устройства. Перезагрузите страницу.</div>`;
        return;
    }

    createGridCells(12);
    applySettingsToGrid(settings);
    updateButtonStates();
    initWebSocket();
    addListeners();

    const resizeWrapper = () => {
        if (!settings.stream_active) return;
        videoWrapper.style.width = 'auto';
        videoWrapper.style.height = 'auto';
        const rect = videoStream.getBoundingClientRect();
        videoWrapper.style.width = `${rect.width}px`;
        videoWrapper.style.height = `${rect.height}px`;
    };
    
    videoStream.addEventListener('load', resizeWrapper);
    window.addEventListener('resize', resizeWrapper);
    if (videoStream.complete) resizeWrapper();
}

function createGridCells(count) {
    for (let i = 0; i < count; i++) {
        const cell = document.createElement('div');
        cell.className = 'cell';
        gridPlane.appendChild(cell);
        cells.push(cell);
    }
}

function applySettingsToGrid(settings) {
    const root = document.documentElement;
    root.style.setProperty('--cam-angle', `${settings.cam_angle || 0}deg`);
    root.style.setProperty('--grid-opacity', settings.grid_opacity);
    root.style.setProperty('--transform-main', `scale(${settings.flip_h ? -1 : 1}, ${settings.flip_v ? -1 : 1})`);
    root.style.setProperty('--grid-offset-x', `${settings.grid_offset_x || 0}px`);
    root.style.setProperty('--grid-offset-y', `${settings.grid_offset_y || 0}px`);
    root.style.setProperty('--grid-offset-z', `${settings.grid_offset_z || 0}px`);
}

function addListeners() {
    document.addEventListener('sensorData', handleSensorData);
    toggleMuteBtn.addEventListener('click', () => {
        sendCommand('toggleMute');
        updateButtonStates();
    });
    toggleStreamBtn.addEventListener('click', () => {
        sendCommand('toggleStream');
        updateButtonStates();
    });
}

function updateButtonStates() {
    icons.mute.classList.toggle('is-hidden', !settings.is_muted);
    icons.unmute.classList.toggle('is-hidden', settings.is_muted);
    icons.pause.classList.toggle('is-hidden', !settings.stream_active);
    icons.play.classList.toggle('is-hidden', settings.stream_active);

    const showGrid = settings.show_grid && settings.stream_active;
    gridScene.classList.toggle('is-hidden', !showGrid);
    
    videoStream.classList.toggle('is-hidden', !settings.stream_active);
    streamOffMessage.classList.toggle('is-hidden', settings.stream_active);
}

function handleSensorData(event) {
    const data = event.detail;
    if (data.sensors && data.sensors.length === 3) {
        updateSensorReadings(data.sensors);
        updateGridColors(data.sensors);
    }
}

function updateSensorReadings(sensorValues) {
    sensorValues.forEach((dist, i) => {
        const el = sensorBoxes[i];
        el.textContent = `${dist.toFixed(1)} м`;
        el.className = 'sensor-box ' + getDistanceClass(dist);
    });
}

function updateGridColors(sensorValues) {
    const colors = sensorValues.map(dist => `var(--color-${getDistanceClass(dist)})`);
    cells.forEach((cell, i) => {
        const colIndex = i % 3;
        cell.style.setProperty('--cell-color', colors[colIndex]);
    });
}

function getDistanceClass(distance) {
    if (distance <= settings.thresh_red) return 'danger';
    if (distance <= settings.thresh_orange) return 'approach';
    if (distance <= settings.thresh_yellow) return 'warn';
    return 'safe';
}

init();