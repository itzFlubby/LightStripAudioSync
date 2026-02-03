class Dot {
    constructor(canvas) {
        this.canvas = canvas;
        this.x = Math.random() * canvas.width;
        this.x_base_velocity = (Math.random() - 0.5) * 0.3;
        this.x_velocity = this.x_base_velocity;
        this.y = Math.random() * canvas.height;
        this.radius = Math.random() * 1.5 + 0.5;
        this.y_base_velocity = (Math.random() - 0.5) * 0.3;
        this.y_velocity = this.y_base_velocity;
        this.angle = Math.random() * Math.PI * 2;
        this.opacity = Math.random() * 0.5 + 0.2;
        this.color = this.generate_random_color();
    }

    generate_random_color() {
        const colors = [
            "rgba(12, 44, 132, ",   // #0c2c84
            "rgba(34, 94, 168, ",   // #225ea8
            "rgba(29, 145, 192, ",  // #1d91c0
            "rgba(65, 182, 196, ",  // #41b6c4
            "rgba(127, 205, 187, ", // #7fcdbb
            "rgba(199, 233, 180, ", // #c7e9b4
        ];
        return colors[Math.floor(Math.random() * colors.length)];
    }

    update(magnitude) {
        // Speed increases with audio magnitude
        const multiplier = 1 + magnitude * 3;
        
        this.x_velocity = this.x_base_velocity * multiplier;
        this.y_velocity = this.y_base_velocity * multiplier;
        
        this.x += this.x_velocity;
        this.y += this.y_velocity;

        // Wrap around edges
        if (this.x - this.radius > this.canvas.width) {
            this.x = -this.radius;
        }
        if (this.x + this.radius < 0) {
            this.x = this.canvas.width + this.radius;
        }
        if (this.y - this.radius > this.canvas.height) {
            this.y = -this.radius;
        }
        if (this.y + this.radius < 0) {
            this.y = this.canvas.height + this.radius;
        }

        // Pulse opacity based on audio
        this.opacity = Math.random() * 0.3 + 0.2 + magnitude * 0.3;
    }

    draw(ctx) {
        ctx.fillStyle = this.color + this.opacity + ")";
        ctx.beginPath();
        ctx.arc(this.x, this.y, this.radius, 0, Math.PI * 2);
        ctx.fill();

        // Glow effect
        ctx.strokeStyle = this.color + (this.opacity * 0.6) + ")";
        ctx.lineWidth = 0.5;
        ctx.stroke();
    }
}

class Background {
    constructor() {
        this.canvas = document.getElementById("background-canvas-id");
        this.ctx = this.canvas.getContext("2d");
        this.dots = [];
        this.magnitude = 0;
        this.gradient = null;
        
        this.resize_canvas();
        window.addEventListener("resize", () => this.resize_canvas());
        
        this.initialize_dots();
        this.animate();
    }

    resize_canvas() {
        this.canvas.width = window.innerWidth;
        this.canvas.height = window.innerHeight;
        this.gradient = this.ctx.createLinearGradient(0, 0, this.canvas.width, this.canvas.height);
        this.gradient.addColorStop(0.0, "#02091a");
        this.gradient.addColorStop(0.5, "#071322");
        this.gradient.addColorStop(1.0, "#061d26");
    }

    initialize_dots() {
        const dots_size = Math.floor((this.canvas.width * this.canvas.height) / 30000);
        this.dots = [];
        for (let i = 0; i < dots_size; i++) {
            this.dots.push(new Dot(this.canvas));
        }
    }

    set_normalized_magnitude(magnitude) {
        this.magnitude = magnitude;
    }

    draw() {
        this.ctx.fillStyle = this.gradient;
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
    }

    animate() {
        this.draw();

        // Update dots
        for (let dot of this.dots) {
            dot.update(this.magnitude);
            dot.draw(this.ctx);
        }

        requestAnimationFrame(() => this.animate());
    }
}