const { createApp } = Vue

createApp({
    data() {
        return {
            localFiles: [],
            ereaderFiles: [],
            isEReaderConnected: false,
            isWsConnected: false,
            transfer: {
                active: false,
                filename: '',
                progress: 0,
                error: ''
            },
            socket: null,
            statusPollingInterval: null,
        }
    },
    computed: {
        statusClass() {
            if (this.transfer.active) return 'status-transferring';
            if (this.isEReaderConnected) return 'status-connected';
            return 'status-idle';
        },
        isTransferDisabled() {
            // Disable transfer if WS is not connected or another transfer is active
            return !this.isWsConnected || this.transfer.active;
        }
    },
    methods: {
        connectWebSocket() {
            const wsProtocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
            this.socket = new WebSocket(`${wsProtocol}//${location.host}/ws`);

            this.socket.onopen = () => {
                console.log('WebSocket connection established');
                this.isWsConnected = true;
            };

            this.socket.onmessage = (event) => {
                const data = JSON.parse(event.data);
                if (data.type === 'progress' && data.filename === this.transfer.filename) {
                    this.transfer.progress = data.value;
                } else if (data.type === 'complete') {
                    this.transfer.active = false;
                    this.fetchFileLists(); // Refresh lists after transfer
                    if (!data.success) {
                        this.transfer.error = data.error || 'An unknown error occurred.';
                    }
                }
            };

            this.socket.onclose = () => {
                console.log('WebSocket connection closed. Reconnecting in 2 seconds...');
                this.isWsConnected = false;
                setTimeout(() => this.connectWebSocket(), 2000);
            };

            this.socket.onerror = (error) => {
                console.error('WebSocket error:', error);
                this.isWsConnected = false;
                this.socket.close();
            };
        },
        async fetchStatus() {
            try {
                const response = await fetch('/status');
                const data = await response.json();
                const wasConnected = this.isEReaderConnected;
                this.isEReaderConnected = data.reader_connected;

                if(wasConnected !== this.isEReaderConnected) {
                    this.fetchFileLists();
                }
            } catch (error) {
                console.error('Error fetching status:', error);
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
        performTransfer(source, destination, filename) {
            if (this.isTransferDisabled) return;

            this.transfer.active = true;
            this.transfer.filename = filename;
            this.transfer.progress = 0;
            this.transfer.error = '';

            fetch('/transfer-file', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ source, destination, filename })
            }).catch(error => {
                console.error('Transfer initiation error:', error);
                this.transfer.error = 'Failed to start the transfer.';
                this.transfer.active = false;
            });
        },
        transferToEReader(filename) {
            this.performTransfer('sd', 'usb', filename);
        },
        transferToLibrary(filename) {
            this.performTransfer('usb', 'sd', filename);
        },
        cancelTransfer() {
            if (!this.transfer.active || !this.isWsConnected) return;
            this.socket.send(JSON.stringify({ type: 'cancel' }));
        },
        async enterSleepMode() {
            if (confirm('Are you sure you want to put the device to sleep? You will need to press the RESET button on the board to wake it up.')) {
                try {
                    await fetch('/enter-sleep', { method: 'POST' });
                    this.transfer.error = 'Device is going to sleep. Press RESET to wake.';
                    this.isEReaderConnected = false;
                } catch (error) {
                    console.error('Error entering sleep mode:', error);
                    this.transfer.error = 'Failed to send sleep command.';
                }
            }
        }
    },
    mounted() {
        this.fetchStatus();
        this.fetchFileLists();
        this.connectWebSocket();
        this.statusPollingInterval = setInterval(this.fetchStatus, 2000);
    },
    beforeUnmount() {
        clearInterval(this.statusPollingInterval);
        if (this.socket) {
            this.socket.close();
        }
    }
}).mount('#app')
