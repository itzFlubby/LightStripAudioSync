class Settings {
    constructor() {
        // Try to load settings from url
        this.settings = this.load_from_url();
        if (this.settings === null) {
            // Try to load from localstorage
            this.settings = this.load_from_localstorage();
        }

        // Fallback load defaults
        if (this.settings === null) {
            this.settings = this.get_default();
            this.store();
        }

        document.getElementById("control-ws-input-url-id").value = this.get("General", "WebSocket URL");
    }

    get_default() {
        const settings = {
            "General": {
                "WebSocket URL": { "value": "ws://localhost:3335", "type": "String" }
            },
            "Background": {
                "Velocity multiplier": { "value": 15, "type": "Number", "min": 1, "step": 1 },
                "Dot count": { "value": 0, "type": "Number", "min": 0, "step": 1 },
                "Dot count (auto)" : { "value": true, "type": "Boolean" },
                "Dot size multiplier": { "value": 1.5, "type": "Number", "min": 0.1, "step": 0.1 }
            },
            "Visualizer": {
                "Bar width": { "value": 0.02, "type": "Number", "min": 0.01, "max": 1.0, "step": 0.01 },
                "Min. bar height": { "value": 0.01, "type": "Number", "min": 0.01, "max": 1.0, "step": 0.01 },
                "Bar line width": { "value": 1.0, "type": "Number", "min": 0.1, "step": 0.1 },
                "Bar shadow blur": { "value": 10, "type": "Number", "min": 0, "step": 1 },
            }
        };
        return settings;
    }

    get(group, item) {
        return this.settings[group][item]["value"];
    }

    set(group, item, value) {
        this.settings[group][item]["value"] = value;
    }

    validate_settings_structure(a, b, recursive_call = false) {
        // Ignore when an actual value is checked on a recursive call
        if (recursive_call && (typeof a !== "object")) {
            return true;
        }
        
        // Check if a and b are valid object (only on first call)
        if (((typeof a !== "object") || (a === null)) || ((typeof b !== "object") || (b === null))) {
            return false;
        }
        
        const a_keys = Object.keys(a);
        const b_keys = Object.keys(b);
        
        // Check if objects have the same number of keys
        if (a_keys.length !== b_keys.length) {
            return false;
        }

        return a_keys.every(key => b_keys.includes(key) && this.validate_settings_structure(a[key], b[key], true));
    }

    store() {
        localStorage.setItem("settings", JSON.stringify(this.settings));
    }

    load_from_localstorage() {
        if ("localStorage" in window && window["localStorage"] !== null) {
            try {
                const settings = JSON.parse(localStorage.getItem("settings"));
                if ((localStorage.length !== 0) || (this.validate_settings_structure(this.get_default(), settings))) {
                    return settings;
                }
            } catch {
                return null;
            }
        }
        return null;
    }

    load_from_url() {
        if (window.location.search !== "") {
            try {
                const settings = JSON.parse(decodeURIComponent(window.location.search.split("?settings=")[1]));
                if (this.validate_settings_structure(this.get_default(), settings)) {
                    return settings;
                }
            } catch {
                return null;
            }
        }
        return null;
    }
}

function show_settings_page() {
    document.getElementById("visualizer-container-id").style.display = "none";

    const settings_container = document.getElementById("settings-container-id");
    settings_container.style.display = "flex";

    const settings_button = document.getElementById("control-settings-button-settings-id");
    settings_button.textContent = "Abort";
    settings_button.onclick = function () { window.location.href = ""; }

    for (let group_key of Object.keys(settings.settings)) {
        // Create group div
        const group_div = document.createElement("div");
        group_div.classList.add("settings-group")
        group_div.innerHTML = group_key;

        for (let element_key of Object.keys(settings.settings[group_key])) {
            // Create element div
            const element_div = document.createElement("div");
            element_div.classList.add("settings-item");
            element_div.classList.add("settings-input");

            // Create element key
            const element_key_span = document.createElement("span");
            element_key_span.textContent = element_key;
            element_div.appendChild(element_key_span);

            // Create element value
            const setting = settings.settings[group_key][element_key];
            switch (setting["type"]) {
                case "Number": {
                    const input_number = document.createElement("input");
                    input_number.type = "number";
                    input_number.lang = "en";
                    input_number.value = setting["value"];
                    for (let attribute of [ "min", "max", "step" ]) {
                        if (attribute in setting) { input_number[attribute] = setting[attribute]; }
                    }
                    input_number.onchange = function () {
                        settings.set(group_key, element_key, Number(this.value));
                        update_url();
                    };
                    element_div.appendChild(input_number);
                    break;
                }
                case "Boolean": {
                    const input_boolean = document.createElement("input");
                    input_boolean.type = "checkbox";
                    input_boolean.checked = setting["value"];
                    input_boolean.onchange = function () {
                        settings.set(group_key, element_key, Boolean(this.checked));
                        update_url();
                    };
                    element_div.appendChild(input_boolean);
                    break;
                }
                case "String": {
                    const input_string = document.createElement("input");
                    input_string.type = "text";
                    input_string.value = setting["value"];
                    input_string.classList.add("settings-input");
                    input_string.onchange = function () {
                        settings.set(group_key, element_key, String(this.value));
                        update_url();
                    };
                    element_div.appendChild(input_string);
                    break;
                }
                default: {
                    break;
                }
            }
            group_div.appendChild(element_div);
        }
        settings_container.appendChild(group_div);
    }
    update_url();
}

function update_url() {
    document.getElementById("settings-url-id").value = `${window.location.href}?settings=${encodeURIComponent(JSON.stringify(settings.settings))}`;
}