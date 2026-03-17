let settings = null;
let data_handler = null;
let websocket = null;

class DataHandler {
    constructor(visualizer) {
        this.visualizer = visualizer;
        this.fps = 0;
        this.total_frames = 0;
        this.last_frame_timestamp = null;
        this.first_frame_timestamp = null;
        this.footer_update_interval = null;

        // Cache to reduce DOM lookups
        this.status_dot = document.getElementById("control-ws-dot-div-id");
        this.status_text = document.getElementById("control-ws-text-id");
        this.ws_button = document.getElementById("control-ws-button-connect-id");
        this.settings_button = document.getElementById("control-settings-button-settings-id");
        this.loader = document.getElementById("loader-container-id");
        this.footer_elements = [
            document.getElementById("footer-element-frames-id").firstChild,
            document.getElementById("footer-element-fps-id").firstChild,
            document.getElementById("footer-element-seconds-id").firstChild
        ];
    }

    on_connecting() {
        this.loader.style.display = "flex";
    }

    on_connect() {
        this.first_frame_timestamp = Date.now();
        this.last_frame_timestamp = this.first_frame_timestamp;
        this.footer_update_interval = window.setInterval(this.update_footer.bind(this), 1000);
        this.loader.style.display = "none";
        this.status_dot.classList.add("connected");
        this.status_text.textContent = "Connected";
        this.ws_button.textContent = "Disconnect";
        this.ws_button.style.opacity = 0.5;
        this.settings_button.style.opacity = 0.5;
    }

    on_close(status = "Disconnected") {
        clearInterval(this.footer_update_interval);
        this.total_frames = 0;
        this.loader.style.display = "none";
        this.status_dot.classList.remove("connected");
        this.status_text.textContent = status;
        this.ws_button.textContent = "Connect";
        this.ws_button.style.opacity = 1.0;
        this.settings_button.style.opacity = 1.0;
    }

    on_data(data) {
        this.visualizer.parse_data(data);
        this.fps++;
        this.total_frames++;
    }

    update_footer() {
        const now = Date.now();
        const elapsed = now - this.last_frame_timestamp;
        if (elapsed >= 1000) {
            const fps = (this.fps / (elapsed / 1000)).toFixed(1);
            this.footer_elements[0].textContent = this.total_frames;
            this.footer_elements[1].textContent = fps;
            this.footer_elements[2].textContent = (now - this.first_frame_timestamp) / 1000;
            this.fps = 0;
            this.last_frame_timestamp = now;
        }
    }
}

function ws_connect() {
    websocket = new WebSocket(document.getElementById("control-ws-input-url-id").value);
    websocket.binaryType = "arraybuffer";
    data_handler.on_connecting();
    
    websocket.onopen = () => data_handler.on_connect();
    websocket.onmessage = (event) => data_handler.on_data(event.data);
    websocket.onerror = () => data_handler.on_close("Error");
    websocket.onclose = () => data_handler.on_close();
}

function ws_disconnect() {
    if (websocket) {
        websocket.close();
        websocket = null;
    }
}

function ws_toggle_connection() {
    if (websocket && (websocket.readyState == WebSocket.OPEN)) {
        ws_disconnect();
    } else {
        ws_connect();
    }
}

document.addEventListener("DOMContentLoaded", () => {
    settings = new Settings();
    const background = new Background(document.getElementById("background-canvas-id"), settings);
    const visualizer = new Visualizer(document.getElementById("bars-canvas-id"), settings, background);
    data_handler = new DataHandler(visualizer);
});