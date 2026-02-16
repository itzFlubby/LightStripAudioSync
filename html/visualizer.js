class Visualizer {
    constructor(canvas, settings, background) {
        this.canvas = canvas;
        this.settings = settings;
        this.background = background;
        this.context = this.canvas.getContext("2d");
        this.primaryColor = getComputedStyle(document.documentElement).getPropertyValue("--primary-color").trim();

        this.resize_canvas();
        window.addEventListener("resize", () => this.resize_canvas());

        this.change_bins_size(30);
    }

    resize_canvas() {
        this.canvas.width = this.canvas.parentElement.clientWidth;
        this.canvas.height = this.canvas.parentElement.clientHeight;

        this.context.strokeStyle = `rgb(${this.primaryColor})`; // Opacity affects shadowColor opacity
        this.context.lineWidth = this.settings.get("Visualizer", "Bar line width");
        this.context.shadowColor = `rgba(${this.primaryColor})`;
        this.context.shadowBlur = this.settings.get("Visualizer", "Bar shadow blur");

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

        let simulated = 
            (this.magnitudes[0][0] * 0.7 + this.magnitudes[0][1] * 0.2 + this.magnitudes[0][2] * 0.1
            + this.magnitudes[1][0] * 0.7 + this.magnitudes[1][1] * 0.2 + this.magnitudes[1][2] * 0.1) / (255. * 6.);
        this.background.set_normalized_magnitude(simulated);

        this.draw();
    }

    draw() {
        this.context.clearRect(0, 0, this.canvas.width, this.canvas.height);
        
        const offset_x = this.canvas.height * 0.03; // Offset to avoid blur clipping on edges
        const canvas_width = this.canvas.width - offset_x;
        const canvas_height = this.canvas.height;

        const bar_width = canvas_width * this.settings.get("Visualizer", "Bar width");;
        const bar_spacing = (canvas_width - (this.bins_size * bar_width)) / (this.bins_size - 1);

        const channel_bar_max_height = canvas_height / 2;

        const min_bar_height = this.settings.get("Visualizer", "Min. bar height");

        for (let i = 0; i < this.bins_size; i++) {
            const left_channel_bar_height = Math.max((this.magnitudes[0][i] / 255.), min_bar_height) * channel_bar_max_height;
            const right_channel_bar_height = Math.max((this.magnitudes[1][i] / 255.), min_bar_height) * channel_bar_max_height;
            
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