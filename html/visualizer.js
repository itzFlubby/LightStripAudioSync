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
        this.status_dot = document.getElementById("ws-status-dot-div-id");
        this.status_text = document.getElementById("ws-status-text-id");
        this.button = document.getElementById("ws-status-button-connect-id");
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
        this.button.textContent = "Disconnect";
    }

    on_close(status = "Disconnected") {
        clearInterval(this.footer_update_interval);
        this.loader.style.display = "none";
        this.status_dot.classList.remove("connected");
        this.status_text.textContent = status;
        this.button.textContent = "Connect";
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

class Visualizer {
    constructor(canvas, background) {
        this.canvas = canvas;
        this.background = background;
        this.context = this.canvas.getContext("2d");
        this.device_pixel_ratio = window.devicePixelRatio || 1;

        this.resize_canvas();
        window.addEventListener("resize", () => this.resize_canvas());

        this.change_bins_size(30);
    }

    resize_canvas() {
        this.canvas.width = this.canvas.offsetWidth * this.device_pixel_ratio;
        this.canvas.height = this.canvas.offsetHeight * this.device_pixel_ratio;

        this.context.setTransform(this.device_pixel_ratio, 0, 0, this.device_pixel_ratio, 0, 0);
        this.context.strokeStyle = "rgba(29, 145, 192, 0.75)"; // Opacity affects shadowColor opacity
        this.context.lineWidth = 0.5;
        this.context.shadowColor = "rgba(29, 145, 192, 0.75)";
        this.context.shadowBlur = 10;

        this.draw();
    }

    change_bins_size(bins_size) {
        this.bins_size = bins_size;
        this.magnitudes = [ // Allocate memory
            new Uint8Array(bins_size),
            new Uint8Array(bins_size)
        ];
        this.draw();
    }

    parse_data(data) {
        const view = new Uint8Array(data);

        if ((this.bins_size === 0) || (this.bins_size !== (view.length / 2))) {
            // Check if view has equal amounts of magnitudes per channel
            if (view.length % 2) {
                return;
            }
            this.bins_size = view.length / 2;
            this.change_bins_size(this.bins_size); // Re-init to set the correct number of bars
        }

        for (let i = 0; i < this.bins_size; ++i) {
            this.magnitudes[0][i] = (view[i]);
            this.magnitudes[1][i] = (view[i + this.bins_size]);
        }

        let simulated = (this.magnitudes[0][0] * 0.7 + this.magnitudes[0][1] * 0.2 + this.magnitudes[0][2] * 0.1) / (255. * 3.);

        this.background.set_normalized_magnitude(simulated);

        this.draw();
    }

    draw() {
        this.context.clearRect(0, 0, this.canvas.width, this.canvas.height);
        
        const offset_x = this.canvas.offsetHeight * 0.02; // Offset to avoid blur clipping on edges
        const canvas_width = this.canvas.offsetWidth - offset_x;
        const canvas_height = this.canvas.offsetHeight;

        const bar_width = canvas_width * 0.02;
        const bar_spacing = (canvas_width - (this.bins_size * bar_width)) / (this.bins_size - 1);

        const channel_bar_max_height = canvas_height / 2;

        for (let i = 0; i < this.bins_size; i++) {
            const left_channel_bar_height = Math.min((this.magnitudes[0][i] / 255.), 0.91) * channel_bar_max_height;
            const right_channel_bar_height = Math.max((this.magnitudes[1][i] / 255.), 0.01) * channel_bar_max_height;
            
            const left_channel_radius = Math.min(bar_width, left_channel_bar_height) * 0.3;
            const right_channel_radius = Math.min(bar_width, right_channel_bar_height) * 0.3;
            
            this.context.beginPath();
            
            // Top left curve
            let x = i * (bar_width + bar_spacing) + (offset_x / 2);
            let y = channel_bar_max_height - left_channel_bar_height;
            this.context.moveTo(x, y + left_channel_radius);
            this.context.quadraticCurveTo(x, y, x + left_channel_radius, y);

            // Top line
            x += bar_width - left_channel_radius;
            this.context.lineTo(x, y);

            // Top right curve
            x += left_channel_radius;
            this.context.quadraticCurveTo(x, y, x, y + left_channel_radius);

            // Right line
            y = channel_bar_max_height + right_channel_bar_height - right_channel_radius;
            this.context.lineTo(x, y);

            // Bottom right curve
            y += right_channel_radius;
            this.context.quadraticCurveTo(x, y, x - right_channel_radius, y);

            // Bottom line
            x -= bar_width - right_channel_radius;
            this.context.lineTo(x, y);

            // Bottom left curve
            x -= right_channel_radius;
            this.context.quadraticCurveTo(x, y, x, y - right_channel_radius);

            this.context.closePath();

            this.context.stroke();
        }
    }
}

function ws_connect() {
    websocket = new WebSocket(document.getElementById("ws-status-input-url-id").value);
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
    const background = new Background();
    const visualizer = new Visualizer(document.getElementById("bars-canvas-id"), background);
    data_handler = new DataHandler(visualizer);

    // Set URL from search
    const captured = /url=([^&]+)/.exec(location.search);
    if (captured) {
        document.getElementById("ws-status-input-url-id").value = `${captured[1]}`;
    }

    // Automatically connect to websocket
    if (location.search.includes("auto")) {
        ws_connect();
    }
});