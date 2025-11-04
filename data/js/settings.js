import { loadSettings, saveSettings, getSnapshot, getDefaultSettings } from './main.js';

document.addEventListener('DOMContentLoaded', () => {
    const settingsForm = document.getElementById('settings-form');
    const tabLinks = document.querySelectorAll('.tab-link');
    const tabContents = document.querySelectorAll('.tab-content');
    const saveStatus = document.getElementById('save-status');
    const resetButton = document.getElementById('reset-settings');
    const previewWrapper = document.querySelector('.settings-preview .video-wrapper');
    const previewImage = document.getElementById('preview-image');
    const previewScene = document.getElementById('preview-scene');
    const previewGridPlane = document.getElementById('preview-grid-plane');
    const gridSettingsFieldset = document.getElementById('grid-settings-fieldset');
    
    let isSnapshotLoaded = false;
    let previewCells = [];

    const rangeValueDisplays = {
        'cam-angle': document.getElementById('cam-angle-value'),
        'jpeg-quality': document.getElementById('jpeg-quality-value'),
        'grid-opacity': document.getElementById('grid-opacity-value'),
        'grid-offset-x': document.getElementById('grid-offset-x-value'),
        'grid-offset-y': document.getElementById('grid-offset-y-value'),
        'grid-offset-z': document.getElementById('grid-offset-z-value'),
    };

    function createPreviewGridCells(count) {
        if(previewCells.length > 0) return;
        for (let i = 0; i < count; i++) {
            const cell = document.createElement('div');
            cell.className = 'cell';
            previewGridPlane.appendChild(cell);
            previewCells.push(cell);
        }
    }

    async function switchTab(tabId) {
        tabContents.forEach(content => content.classList.remove('active'));
        tabLinks.forEach(link => link.classList.remove('active'));

        document.getElementById(tabId).classList.add('active');
        document.querySelector(`.tab-link[data-tab="${tabId}"]`).classList.add('active');

        if (tabId === 'tab-camera' && !isSnapshotLoaded) {
            isSnapshotLoaded = true;
            const snapshotUrl = await getSnapshot();
            if (snapshotUrl) {
                previewImage.src = snapshotUrl;
            } else {
                previewImage.alt = "Не удалось загрузить снимок";
            }
        }
    }

    function populateForm(settings) {
        if (!settings) return;
        for (const key in settings) {
            const input = settingsForm.elements[key];
            if (input) {
                if (input.type === 'checkbox') input.checked = settings[key];
                else input.value = settings[key];
                if (input.type === 'range') updateRangeValue(input.id, input.value); 
            }
        }
    }

    function updateRangeValue(id, value) {
        if (!rangeValueDisplays[id]) return;
        let text = value;
        if (id === 'cam-angle') text += '°';
        else if (id.startsWith('grid-offset')) text += ' px';
        rangeValueDisplays[id].textContent = text;
    }
     
    function updatePreview() {
        const formData = new FormData(settingsForm);
        const settings = Object.fromEntries(formData.entries());
        const transformValue = `rotateX(${settings.cam_angle || 0}deg) translate3d(${settings.grid_offset_x || 0}px, ${settings.grid_offset_y || 0}px, ${settings.grid_offset_z || 0}px)`;
        previewGridPlane.style.transform = transformValue.trim();
        
        const flipH = settingsForm.elements['flip_h'].checked;
        const flipV = settingsForm.elements['flip_v'].checked;
        previewImage.style.transform = `scale(${flipH ? -1 : 1}, ${flipV ? -1 : 1})`;

        previewScene.style.opacity = settings.grid_opacity;
        previewScene.style.display = settingsForm.elements['show_grid'].checked ? 'flex' : 'none';
        
        gridSettingsFieldset.disabled = !settingsForm.elements['show_grid'].checked;
    }

    async function handleSave(e) {
        e.preventDefault();
        saveStatus.textContent = 'Сохранение...';

        const settings = {};
        Array.from(settingsForm.elements).forEach(el => {
            if (!el.name) return;
            if (el.type === 'checkbox') settings[el.name] = el.checked;
            else if (el.type === 'number' || el.type === 'range') settings[el.name] = parseFloat(el.value);
            else settings[el.name] = el.value;
        });

        const success = await saveSettings(settings);
        saveStatus.textContent = success ? 'Сохранено!' : 'Ошибка!';
        setTimeout(() => { saveStatus.textContent = ''; }, 2000);
    }
    
    async function resetToDefaults() {
        if (!confirm('Вы уверены, что хотите сбросить все настройки к значениям по умолчанию?')) return;
        const defaultSettings = getDefaultSettings();
        populateForm(defaultSettings);
        updatePreview();
        const success = await saveSettings(defaultSettings);
        saveStatus.textContent = success ? 'Настройки сброшены и сохранены!' : 'Ошибка сброса!';
        setTimeout(() => { saveStatus.textContent = ''; }, 3000);
        const newSnapshot = await getSnapshot();
        if(newSnapshot) previewImage.src = newSnapshot;
    }
    
    async function init() {
        const settings = await loadSettings();
        if (!settings) {
            alert('Ошибка: не удалось загрузить настройки с устройства.');
            settingsForm.style.display = 'none';
            return;
        }

        createPreviewGridCells(12);
        populateForm(settings);
        
        tabLinks.forEach(link => link.addEventListener('click', () => switchTab(link.dataset.tab)));
        
        settingsForm.addEventListener('input', async (event) => {
            const target = event.target;
            if (target.type === 'range' || target.type === 'checkbox') {
                updateRangeValue(target.id, target.value);
                updatePreview();
            }
            if (target.name === 'resolution' || target.name === 'jpeg_quality') {
                 const newSnapshot = await getSnapshot();
                 if (newSnapshot) previewImage.src = newSnapshot;
            }
        });
        
        updatePreview();
        settingsForm.addEventListener('submit', handleSave);
        resetButton.addEventListener('click', resetToDefaults); 

        const resizePreviewWrapper = () => {
            previewWrapper.style.width = 'auto';
            previewWrapper.style.height = 'auto';
            const rect = previewImage.getBoundingClientRect();
            previewWrapper.style.width = `${rect.width}px`;
            previewWrapper.style.height = `${rect.height}px`;
        };
        previewImage.addEventListener('load', resizePreviewWrapper);
        window.addEventListener('resize', resizePreviewWrapper);
    }
    
    init();
});