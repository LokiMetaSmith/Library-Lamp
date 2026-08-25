// Vanilla JS replacement for Vue.js to optimize for ESP32 resources

const state = {
    localFiles: [],
    ereaderFiles: [],
    isEReaderConnected: false,
    systemErrors: [],
    systemStatus: [],
    transfer: {
        active: false,
        filename: '',
        progress: 0,
        error: ''
    },
    sdTotalMb: 0,
    sdUsedMb: 0,
    allowPublicUploads: false,
    isAdmin: false,
    isUploading: false,
    uploadError: '',
    uploadSuccess: false,
    isDeleting: null,
    isSleeping: false,
    pollingInterval: null
};

// Helper: Escaping HTML strings to prevent XSS
function esc(str) {
    if (!str) return '';
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;');
}

async function fetchData() {
    try {
        const response = await fetch('/status');
        const data = await response.json();
        state.isEReaderConnected = data.reader_connected;

        const errors = [];
        if (!data.sd_mounted) errors.push("Warning: SD Card not mounted.");
        if (!data.usb_mounted) errors.push("Warning: USB initialization failed.");
        if (!data.lora_initialized) errors.push("Warning: LoRaWAN initialization failed.");
        state.systemErrors = errors;

        const status = [];
        if (data.catalog_updating) status.push("Updating Library Catalog...");
        if (data.lora_scanning) status.push("Scanning for other Library-Lamps...");
        state.systemStatus = status;

        if (data.sd_total_mb !== undefined) state.sdTotalMb = data.sd_total_mb;
        if (data.sd_used_mb !== undefined) state.sdUsedMb = data.sd_used_mb;
        if (data.allow_public_uploads !== undefined) state.allowPublicUploads = data.allow_public_uploads;
        if (data.is_admin !== undefined) state.isAdmin = data.is_admin;

        const prevActive = state.transfer.active;
        if (data.transfer_active) {
            state.transfer.active = true;
            state.transfer.filename = data.filename;
            state.transfer.progress = data.total_bytes > 0 ? (data.bytes_transferred / data.total_bytes) * 100 : 0;
        } else {
            if (prevActive) {
                // Transfer just finished, refresh file lists
                fetchFileLists();
            }
            state.transfer.active = false;
        }

        renderUI();
    } catch (error) {
        console.error('Error fetching status:', error);
    }
}

async function fetchFileLists() {
    try {
        const res = await fetch('/list-files?type=all');
        const data = await res.json();
        state.localFiles = data.sd || [];
        state.ereaderFiles = data.usb || [];
        renderUI();
    } catch (error) {
        console.error('Error fetching file lists:', error);
    }
}

function renderUI() {
    // 1. Status Indicator
    const statusInd = document.getElementById('statusIndicator');
    if (statusInd) {
        statusInd.className = 'status-indicator ' + (state.transfer.active ? 'status-transferring' : (state.isEReaderConnected ? 'status-connected' : 'status-idle'));
        const statusText = state.transfer.active ? 'Transferring' : (state.isEReaderConnected ? 'E-Reader Connected' : 'System Idle');
        statusInd.title = statusText;
        statusInd.setAttribute('aria-label', statusText);
    }

    // 2. System Banners
    const errBanner = document.getElementById('systemErrorsBanner');
    if (errBanner) {
        if (state.systemErrors.length > 0) {
            errBanner.innerHTML = state.systemErrors.map(err => `<p>⚠️ ${esc(err)}</p>`).join('');
            errBanner.style.display = 'block';
        } else {
            errBanner.style.display = 'none';
        }
    }

    const statBanner = document.getElementById('systemStatusBanner');
    if (statBanner) {
        if (state.systemStatus.length > 0) {
            statBanner.innerHTML = state.systemStatus.map(msg => `<p>🔄 ${esc(msg)}</p>`).join('');
            statBanner.style.display = 'block';
        } else {
            statBanner.style.display = 'none';
        }
    }

    // 3. Storage Info
    const storageInfo = document.getElementById('storageInfo');
    if (storageInfo) {
        if (state.sdTotalMb > 0) {
            storageInfo.textContent = `${state.sdUsedMb} MB used / ${state.sdTotalMb} MB total`;
            storageInfo.style.display = 'block';
        } else {
            storageInfo.style.display = 'none';
        }
    }

    // 4. Upload Section & CTA hint
    const uploadSection = document.getElementById('uploadSection');
    const canUpload = state.allowPublicUploads || state.isAdmin;
    if (uploadSection) {
        uploadSection.style.display = canUpload ? 'block' : 'none';
    }

    const uploadHint = document.getElementById('uploadHint');
    if (uploadHint) {
        uploadHint.style.display = canUpload ? 'inline-block' : 'none';
    }

    // 5. Render SD Card File List
    const localList = document.getElementById('localFilesList');
    if (localList) {
        if (state.localFiles.length === 0) {
            localList.innerHTML = `
                <li class="empty-state">
                    No books found in the library. <br>
                    <span id="uploadHint" style="font-size: 0.9em; margin-top: 5px; display: ${canUpload ? 'inline-block' : 'none'};">Upload a book above to get started!</span>
                </li>`;
        } else {
            localList.innerHTML = state.localFiles.map(file => {
                const isTransferring = state.transfer.active && state.transfer.filename === file.name;
                const isDeleting = state.isDeleting === file.name;
                const progressPct = Math.round(state.transfer.progress);
                const isEpub = file.name.toLowerCase().endsWith('.epub');

                return `
                <li>
                    <div class="file-info">
                        <span class="file-title">${esc(file.title || file.name)}</span>
                        <span class="file-author">${esc(file.author || '')}</span>
                        ${file.notes ? `<span class="file-notes">${esc(file.notes)}</span>` : ''}
                    </div>
                    <div class="transfer-controls">
                        ${isTransferring ? `
                            <div class="progress-container" role="progressbar" aria-valuemin="0" aria-valuemax="100" aria-valuenow="${progressPct}" aria-label="Transfer progress for ${esc(file.title || file.name)}" title="${progressPct}%">
                                <div class="progress-bar" style="width: ${progressPct}%"></div>
                            </div>
                        ` : ''}
                        <button onclick="transferToEReader('${esc(file.name)}')" ${(!state.isEReaderConnected || state.transfer.active) ? 'disabled' : ''} title="${state.transfer.active ? 'A transfer is currently in progress' : (!state.isEReaderConnected ? 'Connect an E-Reader to transfer books' : 'Transfer to E-Reader')}" aria-label="Transfer ${esc(file.title || file.name)} to E-Reader">Transfer to E-Reader</button>
                        ${isEpub ? `<a class="sleep-btn" style="text-decoration: none;" href="/viewer.html?file=${encodeURIComponent(file.name)}&source=sd" aria-label="Read ${esc(file.title || file.name)}">Read</a>` : ''}
                        ${state.isAdmin ? `<button onclick="deleteFile('${esc(file.name)}', 'sd')" class="btn danger" style="background-color: #c94b4b; color: white;" aria-label="Delete ${esc(file.title || file.name)}" ${(isDeleting || state.transfer.active) ? 'disabled' : ''} title="${isDeleting ? 'Deleting file...' : (state.transfer.active ? 'Action unavailable during transfer' : 'Delete file')}">${isDeleting ? 'Deleting...' : 'Delete'}</button>` : ''}
                        ${isTransferring ? `<button onclick="cancelTransfer()" class="cancel-btn" aria-label="Cancel transfer for ${esc(file.title || file.name)}">Cancel</button>` : ''}
                    </div>
                </li>`;
            }).join('');
        }
    }

    // 6. Render E-Reader Section
    const ereaderContainer = document.getElementById('ereaderContainer');
    if (ereaderContainer) {
        if (!state.isEReaderConnected) {
            ereaderContainer.innerHTML = `
                <div class="empty-state">
                    No e-reader connected.<br><span style="font-size: 0.9em; margin-top: 5px; display: inline-block;">Connect your device via USB to view and transfer books.</span>
                </div>`;
        } else if (state.ereaderFiles.length === 0) {
            ereaderContainer.innerHTML = `
                <ul class="file-list">
                    <li class="empty-state">
                        No books found on the e-reader. <br>
                        <span style="font-size: 0.9em; margin-top: 5px; display: inline-block;">Transfer a book from your local library to start reading!</span>
                    </li>
                </ul>`;
        } else {
            ereaderContainer.innerHTML = `
                <ul class="file-list">
                    ${state.ereaderFiles.map(file => {
                        const isTransferring = state.transfer.active && state.transfer.filename === file.name;
                        const isDeleting = state.isDeleting === file.name;
                        const progressPct = Math.round(state.transfer.progress);

                        return `
                        <li>
                            <div class="file-info">
                                <span class="file-title">${esc(file.title || file.name)}</span>
                                <span class="file-author">${esc(file.author || '')}</span>
                                ${file.notes ? `<span class="file-notes">${esc(file.notes)}</span>` : ''}
                            </div>
                            <div class="transfer-controls">
                                ${isTransferring ? `
                                    <div class="progress-container" role="progressbar" aria-valuemin="0" aria-valuemax="100" aria-valuenow="${progressPct}" aria-label="Transfer progress for ${esc(file.title || file.name)}" title="${progressPct}%">
                                        <div class="progress-bar" style="width: ${progressPct}%"></div>
                                    </div>
                                ` : ''}
                                <button onclick="transferToLibrary('${esc(file.name)}')" ${state.transfer.active ? 'disabled' : ''} title="${state.transfer.active ? 'A transfer is currently in progress' : 'Transfer to Library'}" aria-label="Transfer ${esc(file.title || file.name)} to Library">Transfer to Library</button>
                                ${state.isAdmin ? `<button onclick="deleteFile('${esc(file.name)}', 'usb')" class="btn danger" style="background-color: #c94b4b; color: white;" aria-label="Delete ${esc(file.title || file.name)}" ${(isDeleting || state.transfer.active) ? 'disabled' : ''} title="${isDeleting ? 'Deleting file...' : (state.transfer.active ? 'Action unavailable during transfer' : 'Delete file')}">${isDeleting ? 'Deleting...' : 'Delete'}</button>` : ''}
                                ${isTransferring ? `<button onclick="cancelTransfer()" class="cancel-btn" aria-label="Cancel transfer for ${esc(file.title || file.name)}">Cancel</button>` : ''}
                            </div>
                        </li>`;
                    }).join('')}
                </ul>`;
        }
    }

    // 7. Footer error message
    const transferErr = document.getElementById('transferError');
    if (transferErr) {
        if (state.transfer.error) {
            transferErr.textContent = state.transfer.error;
            transferErr.style.display = 'block';
        } else {
            transferErr.style.display = 'none';
        }
    }
}

async function performTransfer(source, destination, filename) {
    if (state.transfer.active) return;

    state.transfer.active = true;
    state.transfer.filename = filename;
    state.transfer.progress = 0;
    state.transfer.error = '';
    renderUI();

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
            state.transfer.error = result.message;
        }
    } catch (error) {
        console.error('Transfer error:', error);
        state.transfer.error = 'A network error occurred during the transfer.';
    } finally {
        state.transfer.active = false;
        renderUI();
    }
}

function transferToEReader(filename) {
    performTransfer('sd', 'usb', filename);
}

function transferToLibrary(filename) {
    performTransfer('usb', 'sd', filename);
}

async function cancelTransfer() {
    if (!state.transfer.active) return;
    try {
        await fetch('/transfer-cancel', { method: 'POST' });
    } catch (error) {
        console.error('Error cancelling transfer:', error);
        state.transfer.error = 'Failed to send cancel request.';
        renderUI();
    }
}

async function deleteFile(filename, source) {
    if (!confirm(`Are you sure you want to delete ${filename}?`)) return;

    state.isDeleting = filename;
    renderUI();

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
            fetchFileLists();
        } else {
            const err = await response.text();
            alert(`Delete failed: ${err}`);
        }
    } catch (error) {
        console.error('Delete error:', error);
        alert('An error occurred while deleting the file.');
    } finally {
        state.isDeleting = null;
        renderUI();
    }
}

async function uploadBook(event) {
    if (event) event.preventDefault();
    const fileInput = document.getElementById('uploadFile');
    const titleInput = document.getElementById('uploadTitle');
    const authorInput = document.getElementById('uploadAuthor');
    const uploadBtn = document.getElementById('uploadBtn');

    if (!fileInput || !fileInput.files.length) return;

    const file = fileInput.files[0];
    const title = titleInput ? titleInput.value : 'Unknown Title';
    const author = authorInput ? authorInput.value : 'Unknown Author';
    const filename = file.name;

    state.isUploading = true;
    const errEl = document.getElementById('uploadError');
    const succEl = document.getElementById('uploadSuccess');
    if (errEl) { errEl.style.display = 'none'; errEl.textContent = ''; }
    if (succEl) { succEl.style.display = 'none'; }

    if (uploadBtn) {
        uploadBtn.disabled = true;
        uploadBtn.textContent = 'Uploading...';
        uploadBtn.title = 'Upload in progress...';
    }

    try {
        let url = `/upload?title=${encodeURIComponent(title)}&author=${encodeURIComponent(author)}&filename=${encodeURIComponent(filename)}`;
        const savedKey = localStorage.getItem('adminKey');
        if (savedKey) {
            url += `&key=${encodeURIComponent(savedKey)}`;
        }

        const response = await fetch(url, {
            method: 'POST',
            body: file
        });

        if (response.ok) {
            if (titleInput) titleInput.value = '';
            if (authorInput) authorInput.value = '';
            fileInput.value = '';
            if (succEl) {
                succEl.style.display = 'block';
                setTimeout(() => {
                    succEl.style.display = 'none';
                }, 4000);
            }
            fetchFileLists();
            fetchData();
        } else {
            const txt = await response.text();
            if (errEl) {
                errEl.textContent = `Upload failed: ${txt || response.statusText}`;
                errEl.style.display = 'block';
            }
        }
    } catch (err) {
        console.error(err);
        if (errEl) {
            errEl.textContent = 'Network error during upload.';
            errEl.style.display = 'block';
        }
    } finally {
        state.isUploading = false;
        if (uploadBtn) {
            uploadBtn.disabled = false;
            uploadBtn.textContent = 'Upload';
            uploadBtn.title = 'Upload book';
        }
    }
}

async function enterSleepMode(btn) {
    if (confirm('Are you sure you want to put the device to sleep? You will need to press the RESET button on the board to wake it up.')) {
        state.isSleeping = true;
        if (btn) {
            btn.disabled = true;
            btn.textContent = 'Entering Sleep...';
            btn.title = 'Entering sleep mode...';
        }
        try {
            await fetch('/enter-sleep', { method: 'POST' });
            state.transfer.error = 'Device is going to sleep. Press RESET to wake.';
            state.isEReaderConnected = false;
            renderUI();
        } catch (error) {
            console.error('Error entering sleep mode:', error);
            state.transfer.error = 'Failed to send sleep command.';
            state.isSleeping = false;
            if (btn) {
                btn.disabled = false;
                btn.textContent = 'Enter Sleep Mode';
                btn.title = 'Enter sleep mode';
            }
            renderUI();
        }
    }
}

// Initialize on DOM load
document.addEventListener('DOMContentLoaded', () => {
    fetchData();
    fetchFileLists();
    // ⚡ Bolt: Reduced status polling from 1s to 2s to minimize CPU load on ESP32
    state.pollingInterval = setInterval(fetchData, 2000);
});
