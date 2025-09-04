let selectedSdFile = null;
let selectedUsbFile = null;

const sdList = document.getElementById('sd-file-list');
const usbList = document.getElementById('usb-file-list');
const toReaderBtn = document.getElementById('to-reader-btn');
const toLibraryBtn = document.getElementById('to-library-btn');
const statusDiv = document.getElementById('status');

function selectFile(listElement, fileName, type) {
    Array.from(document.querySelectorAll('.file-name.selected')).forEach(el => el.classList.remove('selected'));
    const clickedLi = event.target.closest('li');
    if (clickedLi) {
        clickedLi.firstElementChild.classList.add('selected');
    }

    if (type === 'sd') {
        selectedSdFile = fileName;
        selectedUsbFile = null;
    } else {
        selectedUsbFile = fileName;
        selectedSdFile = null;
    }
    updateButtons();
}

function createListItem(fileName, type) {
    const li = document.createElement('li');
    const span = document.createElement('span');
    span.className = 'file-name';
    span.textContent = fileName;
    li.appendChild(span);
    const listElement = type === 'sd' ? sdList : usbList;
    li.onclick = () => selectFile(listElement, fileName, type);
    return li;
}

async function fetchFiles(type) {
    try {
        const response = await fetch(`/list-files?type=${type}`);
        const files = await response.json();
        const listElement = type === 'sd' ? sdList : usbList;
        listElement.innerHTML = '';
        if (files && files.length > 0) {
          files.forEach(file => {
            listElement.appendChild(createListItem(file.name, type));
          });
        } else {
           listElement.innerHTML = '<li>No books found.</li>';
        }
    } catch (e) {
        console.error(`Error fetching ${type} files:`, e);
        const listElement = type === 'sd' ? sdList : usbList;
        listElement.innerHTML = `<li>Error loading files. Check console.</li>`;
    }
}

async function transferFile(source, destination, fileName) {
    statusDiv.textContent = `Copying ${fileName}... This may take a moment.`;
    toReaderBtn.disabled = true;
    toLibraryBtn.disabled = true;
    try {
        const response = await fetch('/transfer-file', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ source, destination, filename: fileName })
        });
        const result = await response.json();
        statusDiv.textContent = result.message;
    } catch (e) {
        statusDiv.textContent = 'Error during transfer.';
    }
    // Refresh file lists after transfer
    setTimeout(() => {
      fetchFiles('sd');
      fetchFiles('usb');
      updateButtons();
    }, 1500);
}

function updateButtons() {
    toReaderBtn.disabled = !selectedSdFile;
    toLibraryBtn.disabled = !selectedUsbFile;
}

function checkStatus() {
    fetch('/status')
        .then(response => response.json())
        .then(data => {
            const isConnected = data.reader_connected;
            const wasConnected = statusDiv.classList.contains('connected');

            if (isConnected) {
                statusDiv.textContent = 'E-Reader Connected!';
                statusDiv.className = 'status connected';
                if (!wasConnected) { // Fetch files only on new connection
                    fetchFiles('usb');
                }
            } else {
                statusDiv.textContent = 'E-Reader Disconnected. Please plug in your device.';
                statusDiv.className = 'status disconnected';
                usbList.innerHTML = '<li>Please connect e-reader.</li>';
            }
        });
}

toReaderBtn.onclick = () => {
    if (selectedSdFile) transferFile('sd', 'usb', selectedSdFile);
};

toLibraryBtn.onclick = () => {
    if (selectedUsbFile) transferFile('usb', 'sd', selectedUsbFile);
};

window.onload = () => {
    fetchFiles('sd');
    setInterval(checkStatus, 3000);
    checkStatus();
};
