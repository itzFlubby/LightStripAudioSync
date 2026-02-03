let background = null;

let ws = null;
let frames_per_second = 0;
let total_frames = 0;
let last_frame_timestamp = null;
let first_frame_timestamp = null;
let bins_size = 0;
let update_interval = null;
let magnitudes = null;

let channel_bars_left = [];
let channel_bars_right = [];
let channel_bars_simulated = [];
let footer_elements = [];

function init() {
    const visualizer_channel_bars_container_left = document.getElementById("visualizer-channel-bars-container-left-id");
    const visualizer_channel_bars_container_right = document.getElementById("visualizer-channel-bars-container-right-id");
    
    visualizer_channel_bars_container_left.innerHTML = "";
    visualizer_channel_bars_container_right.innerHTML = "";

    // Clear potentially existing bars
    visualizer_channel_bars_container_left.replaceChildren()
    channel_bars_left = [];
    visualizer_channel_bars_container_right.replaceChildren()
    channel_bars_right = [];

    // Generate bars for left channel
    for (let i = 0; i < ((bins_size == 0) ? 30 : bins_size); ++i) {
        const bar = document.createElement("div");
        bar.className = "visualizer-channel-bar-left";
        bar.dataset.bin = i;
        bar.id = `visualizer-channel-bar-left-${i}-id`;
        visualizer_channel_bars_container_left.appendChild(bar);
        channel_bars_left.push(bar);
    }

    // Generate bars for right channel
    for (let i = 0; i < ((bins_size == 0) ? 30 : bins_size); ++i) {
        const bar = document.createElement("div");
        bar.className = "visualizer-channel-bar-right";
        bar.dataset.bin = i;
        bar.id = `visualizer-channel-bar-right-${i}-id`;
        visualizer_channel_bars_container_right.appendChild(bar);
        channel_bars_right.push(bar);
    }

    // Cache channel bars to reduce DOM lookups
    channel_bars_simulated = [
        document.getElementById("visualizer-channel-bar-simulated-left-id"),
        document.getElementById("visualizer-channel-bar-simulated-right-id")
    ];

    // Cache to reduce DOM lookups
    footer_elements = [
        document.getElementById("footer-element-frames-id").firstChild,
        document.getElementById("footer-element-fps-id").firstChild,
        document.getElementById("footer-element-seconds-id").firstChild
    ];

    // Alloc memory
    magnitudes = [
        new Uint8Array(bins_size),
        new Uint8Array(bins_size)
    ];

    // Set URL from search
    const captured = /url=([^&]+)/.exec(location.search);
    if (captured) {
        document.getElementById("ws-status-input-url-id").value = `ws://${captured[1]}`;
    }

    // Automatically connect to websocket
    if (location.search.includes("auto")) {
        ws_connect();
    }
}

function update_magnitudes(magnitudes) {
    for (let i = 0; i < bins_size; ++i) {
        channel_bars_left[i].style.height = `${magnitudes[0][i] / 2.55}%`;
        channel_bars_right[i].style.height = `${magnitudes[1][i] / 2.55}%`;
    }

    const magnitude_simulated_left = (magnitudes[0][0] * 0.7 + magnitudes[0][1] * 0.2 + magnitudes[0][2] * 0.1);
    channel_bars_simulated[0].style.width = `${magnitude_simulated_left / 2.55}%`;
    const magnitude_simulated_right = (magnitudes[1][0] * 0.7 + magnitudes[1][1] * 0.2 + magnitudes[1][2] * 0.1);
    channel_bars_simulated[1].style.width = `${magnitude_simulated_right / 2.55}%`;

    background.set_normalized_magnitude((magnitude_simulated_left + magnitude_simulated_right) / 510)

    total_frames++;
    frames_per_second++;
}

function update_footer() {
    const now = Date.now();
    const elapsed = now - last_frame_timestamp;
    if (elapsed >= 1000) {
        const fps = (frames_per_second / (elapsed / 1000)).toFixed(1);
        footer_elements[0].textContent = total_frames;
        footer_elements[1].textContent = fps;
        footer_elements[2].textContent = (now - first_frame_timestamp) / 1000;
        frames_per_second = 0;
        last_frame_timestamp = now;
    }
}

function parse_data(buffer) {
    const view = new Uint8Array(buffer);

    if ((bins_size === 0) || (bins_size !== (view.length / 2))) {
        // Check if view has equal amounts of magnitudes per channel
        if (view.length % 2) {
            return;
        }
        bins_size = view.length / 2;
        init(); // Re-init to set the correct number of bars
    }

    for (let i = 0; i < bins_size; ++i) {
        magnitudes[0][i] = (view[i]);
        magnitudes[1][i] = (view[i + bins_size]);
    }

    return magnitudes;
}

function ws_connect() {
    const url = document.getElementById("ws-status-input-url-id").value;
    const status_dot = document.getElementById("ws-status-dot-div-id");
    const status_text = document.getElementById("ws-status-text-id");
    const button = document.getElementById("ws-status-button-connect-id");

    ws = new WebSocket(url);
    ws.binaryType = "arraybuffer";

    // Start load animation
    document.getElementById("loader-container-id").style.display = "flex";

    ws.onopen = () => {
        status_dot.classList.add("connected");
        status_text.textContent = "Connected";
        button.textContent = "Disconnect";
        document.getElementById("loader-container-id").style.display = "none";
        first_frame_timestamp = Date.now();
        last_frame_timestamp = Date.now();
        update_interval = window.setInterval(update_footer, 1000);
    };

    ws.onmessage = (event) => {
        update_magnitudes(parse_data(event.data));
    };

    ws.onerror = (error) => {
        status_dot.classList.remove("connected");
        status_text.textContent = "Error";
        document.getElementById("loader-container-id").style.display = "none";
        clearInterval(update_interval) 
    };

    ws.onclose = () => {
        status_dot.classList.remove("connected");
        status_text.textContent = "Disconnected";
        button.textContent = "Connect";
        document.getElementById("loader-container-id").style.display = "none";
        clearInterval(update_interval) 
    };
}

function ws_disconnect() {
    if (ws) {
        ws.close();
        ws = null;
    }
}

function ws_toggle_connection() {
    if (ws && (ws.readyState == WebSocket.OPEN)) {
        ws_disconnect();
    } else {
        ws_connect();
    }
}

document.addEventListener("DOMContentLoaded", () => {
    background = new Background();
    init();
});