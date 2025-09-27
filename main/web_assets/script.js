const { createApp } = Vue

createApp({
    data() {
        return {
            localFiles: [],
            ereaderFiles: [],
            isEReaderConnected: false,
            status: 'idle', // idle, connected, transferring
            transfer: {
                active: false,
                filename: '',
                progress: 0,
                error: ''
            },
            websocket: null,
        }
    },
    computed: {
        statusClass() {
            if (this.transfer.active) return 'status-transferring';
            if (this.isEReaderConnected) return 'status-connected';
            return 'status-idle';
        }
    },
    methods: {
        initWebSocket() {
            const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsHost = window.location.host;
            this.websocket = new WebSocket(`${wsProtocol}//${wsHost}/ws`);

            this.websocket.onopen = () => {
                console.log("WebSocket connection established");
                // You could send a message to get initial state if needed
                // this.websocket.send(JSON.stringify({ type: 'get_status' }));
            };

            this.websocket.onmessage = (event) => {
                const message = JSON.parse(event.data);
                console.log("WS Message received:", message);
                this.handleWsMessage(message);
            };

            this.websocket.onclose = () => {
                console.log("WebSocket connection closed. Attempting to reconnect...");
                this.transfer.active = false; // Stop any active transfer UI
                setTimeout(this.initWebSocket, 2000); // Reconnect after 2 seconds
            };

            this.websocket.onerror = (error) => {
                console.error("WebSocket error:", error);
                this.transfer.error = "WebSocket connection error.";
                this.websocket.close();
            };
        },
        handleWsMessage(message) {
            switch (message.type) {
                case 'start':
                    this.transfer.active = true;
                    this.transfer.filename = message.filename;
                    this.transfer.progress = 0;
                    this.transfer.error = '';
                    break;
                case 'progress':
                    this.transfer.progress = message.value;
                    break;
                case 'complete':
                    this.transfer.active = false;
                    if (!message.success) {
                        this.transfer.error = message.message;
                    }
                    // Refresh file lists after any transfer attempt
                    this.fetchFileLists();
                    break;
                case 'error':
                     this.transfer.error = message.message;
                     this.transfer.active = false;
                     break;
                 // You could add a 'status' message type to update isEReaderConnected
                 // if you remove the REST endpoint entirely.
            }
        },
        async fetchFileLists() {
            try {
                const [localRes, ereaderRes] = await Promise.all([
                    fetch('/list-files?type=sd'),
                    fetch('/list-files?type=usb')
                ]);
                this.localFiles = await localRes.json();
                this.ereaderFiles = await ereaderRes.json();
            } catch (error) {
                console.error('Error fetching file lists:', error);
            }
        },
        async performTransfer(source, destination, filename) {
            if (this.transfer.active) return;

            this.transfer.active = true;
            this.transfer.filename = filename;
            this.transfer.progress = 0;
            this.transfer.error = '';

            try {
                const response = await fetch('/transfer-file', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ source, destination, filename })
                });
                const result = await response.json();
                if (!result.success) {
                    this.transfer.error = result.message;
                }
            } catch (error) {
                console.error('Transfer error:', error);
                this.transfer.error = 'A network error occurred during the transfer.';
            } finally {
                this.transfer.active = false;
                // The polling will handle the final state update and file list refresh
            }
        },
        transferToEReader(filename) {
            this.performTransfer('sd', 'usb', filename);
        },
        transferToLibrary(filename) {
            this.performTransfer('usb', 'sd', filename);
        },
        async cancelTransfer() {
            if (!this.transfer.active) return;
            try {
                await fetch('/transfer-cancel', { method: 'POST' });
                // The backend will stop the transfer. The polling will update the state.
            } catch (error) {
                console.error('Error cancelling transfer:', error);
                this.transfer.error = 'Failed to send cancel request.';
            }
        },
        async enterSleepMode() {
            if (confirm('Are you sure you want to put the device to sleep? You will need to press the RESET button on the board to wake it up.')) {
                try {
                    await fetch('/enter-sleep', { method: 'POST' });
                    // If the request succeeds, the device will go to sleep and will stop responding.
                    this.transfer.error = 'Device is going to sleep. Press RESET to wake.';
                    this.isEReaderConnected = false; // Assume disconnection
                } catch (error) {
                    console.error('Error entering sleep mode:', error);
                    this.transfer.error = 'Failed to send sleep command.';
                }
            }
        }
    },
    async mounted() {
        // Fetch initial state via REST
        try {
            const response = await fetch('/status');
            const data = await response.json();
            this.isEReaderConnected = data.reader_connected;
        } catch (error) {
            console.error('Error fetching initial status:', error);
            this.transfer.error = "Could not load initial device status.";
        }

        this.fetchFileLists();

        // Start WebSocket for real-time updates
        this.initWebSocket();
    },
    beforeUnmount() {
        if (this.websocket) {
            this.websocket.close();
        }
    }
}).mount('#app')
