const { createApp } = Vue

createApp({
    data() {
        return {
            localFiles: [],
            ereaderFiles: [],
            isEReaderConnected: false,
            systemErrors: [],
            systemStatus: [],
            status: 'idle', // idle, connected, transferring

            transfer: {
                active: false,
                filename: '',
                progress: 0,
                error: ''
            },
            pollingInterval: null,
            sdTotalMb: 0,
            sdUsedMb: 0,
            allowPublicUploads: false,
            isAdmin: false,
            uploadTitle: '',
            uploadAuthor: '',
            isUploading: false,
            uploadError: '',
            uploadSuccess: false,
            isDeleting: null,
            isSleeping: false
        }
    },
    computed: {
        statusClass() {
            if (this.transfer.active) return 'status-transferring';
            if (this.isEReaderConnected) return 'status-connected';
            return 'status-idle';
        },
        statusText() {
            if (this.transfer.active) return 'Transferring';
            if (this.isEReaderConnected) return 'E-Reader Connected';
            return 'System Idle';
        }
    },
    methods: {
        async fetchData() {
            try {
                const response = await fetch('/status');
                const data = await response.json();
                this.isEReaderConnected = data.reader_connected;
                const errors = [];
                if (!data.sd_mounted) errors.push("Warning: SD Card not mounted.");
                if (!data.usb_mounted) errors.push("Warning: USB initialization failed.");
                if (!data.lora_initialized) errors.push("Warning: LoRaWAN initialization failed.");
                this.systemErrors = errors;

                const status = [];
                if (data.catalog_updating) status.push("Updating Library Catalog...");
                if (data.lora_scanning) status.push("Scanning for other Library-Lamps...");
                this.systemStatus = status;

                if (data.sd_total_mb !== undefined) this.sdTotalMb = data.sd_total_mb;
                if (data.sd_used_mb !== undefined) this.sdUsedMb = data.sd_used_mb;
                if (data.allow_public_uploads !== undefined) this.allowPublicUploads = data.allow_public_uploads;
                if (data.is_admin !== undefined) this.isAdmin = data.is_admin;

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
                // ⚡ Bolt: Fetch consolidated endpoint to avoid exhausting ESP-IDF connection pool
                // and to avoid sequential fetch waterfall latency
                const res = await fetch('/list-files?type=all');
                const data = await res.json();
                this.localFiles = data.sd || [];
                this.ereaderFiles = data.usb || [];
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
                if (source === 'sd') {
            alert('A transfer has been initiated. Please press the physical Eject/Sleep button on the E-Book Librarian device to confirm the transfer.');
        }

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

        async deleteFile(filename, source) {
            if (!confirm(`Are you sure you want to delete ${filename}?`)) return;

            this.isDeleting = filename;
            let url = '/delete-file';
            const savedKey = localStorage.getItem('adminKey');
            if (savedKey) {
                url += `?key=${encodeURIComponent(savedKey)}`;
            }

            try {
                const response = await fetch(url, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ filename, source })
                });

                if (response.ok) {
                    this.fetchFileLists();
                } else {
                    const err = await response.text();
                    alert(`Delete failed: ${err}`);
                }
            } catch (error) {
                console.error('Delete error:', error);
                alert('An error occurred while deleting the file.');
            } finally {
                this.isDeleting = null;
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
        async uploadBook() {
            const fileInput = this.$refs.fileInput;
            if (!fileInput.files.length) return;

            const file = fileInput.files[0];
            const title = this.uploadTitle || 'Unknown Title';
            const author = this.uploadAuthor || 'Unknown Author';
            const filename = file.name;

            this.isUploading = true;
            this.uploadError = '';
            this.uploadSuccess = false;

            try {
                // Construct query URL
                let url = `/upload?title=${encodeURIComponent(title)}&author=${encodeURIComponent(author)}&filename=${encodeURIComponent(filename)}`;
                const savedKey = localStorage.getItem('adminKey');
                if (savedKey) {
                    url += `&key=${encodeURIComponent(savedKey)}`;
                }

                const response = await fetch(url, {
                    method: 'POST',
                    body: file // Send file directly as body
                });

                if (response.ok) {
                    this.uploadTitle = '';
                    this.uploadAuthor = '';
                    fileInput.value = '';
                    this.fetchFileLists();
                    this.fetchData(); // to update SD size
                    this.uploadSuccess = true;
                    setTimeout(() => {
                        this.uploadSuccess = false;
                    }, 4000);
                } else {
                    const txt = await response.text();
                    this.uploadError = `Upload failed: ${txt || response.statusText}`;
                }
            } catch (err) {
                console.error(err);
                this.uploadError = 'Network error during upload.';
            } finally {
                this.isUploading = false;
            }
        },
        async enterSleepMode() {
            if (confirm('Are you sure you want to put the device to sleep? You will need to press the RESET button on the board to wake it up.')) {
                this.isSleeping = true;
                try {
                    await fetch('/enter-sleep', { method: 'POST' });
                    // If the request succeeds, the device will go to sleep and will stop responding.
                    this.transfer.error = 'Device is going to sleep. Press RESET to wake.';
                    this.isEReaderConnected = false; // Assume disconnection
                } catch (error) {
                    console.error('Error entering sleep mode:', error);
                    this.transfer.error = 'Failed to send sleep command.';
                    this.isSleeping = false;
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
