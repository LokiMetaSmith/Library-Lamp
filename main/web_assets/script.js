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
            pollingInterval: null,
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
