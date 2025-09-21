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
            ota: {
                status: '',
                uploading: false
            },
            pollingInterval: null,
        }
    },
    computed: {
        statusClass() {
            if (this.transfer.active || this.ota.uploading) return 'status-transferring';
            if (this.isEReaderConnected) return 'status-connected';
            return 'status-idle';
        }
    },
    methods: {
        async uploadFirmware() {
            const fileInput = document.getElementById('ota_file');
            const file = fileInput.files[0];

            if (!file) {
                this.ota.status = 'Please select a firmware file first.';
                return;
            }

            if (!confirm(`Are you sure you want to install "${file.name}"? The device will restart.`)) {
                return;
            }

            this.ota.uploading = true;
            this.ota.status = 'Uploading firmware... Do not navigate away.';

            try {
                const response = await fetch('/ota-update', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/octet-stream' },
                    body: file
                });

                const responseText = await response.text();
                if (response.ok) {
                    this.ota.status = `Update successful! ${responseText}`;
                } else {
                    this.ota.status = `Error: ${responseText}`;
                }
            } catch (error) {
                console.error('OTA Error:', error);
                this.ota.status = 'An error occurred during the update.';
            } finally {
                this.ota.uploading = false;
            }
        },
        async fetchData() {
            try {
                const response = await fetch('/status');
                const data = await response.json();
                this.isEReaderConnected = data.reader_connected;
                if (data.transfer_active) {
                    this.transfer.active = true;
                    this.transfer.filename = data.filename;
                    this.transfer.progress = data.total_bytes > 0 ? (data.bytes_transferred / data.total_bytes) * 100 : 0;
                } else {
                    if (this.transfer.active) {
                        // Transfer just finished, refresh file lists
                        this.fetchFileLists();
                    }
                    this.transfer.active = false;
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
    mounted() {
        this.fetchData();
        this.fetchFileLists();
        this.pollingInterval = setInterval(this.fetchData, 1000); // Poll for status updates
    },
    beforeUnmount() {
        clearInterval(this.pollingInterval);
    }
}).mount('#app')
