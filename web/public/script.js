let currentPage = 0;
let rowsPerPage = 10;
let data = [];
let filteredData = [];

function renderTable() {
    const tableBody = document.getElementById('data-table-body');
    tableBody.innerHTML = " ";
    
    const start = currentPage * rowsPerPage;
    const end = start + rowsPerPage;
    const paginatedData = filteredData.slice(start, end);
    
    paginatedData.forEach((item, index) => {
        const row = document.createElement("tr");
        row.innerHTML = `
        <td>${start + index + 1}</td>
        <td>${item.idPasien}</td>
        <td>${item.waktu}</td> 
        `;

        row.addEventListener("click", () => {
            window.location.href = `detail.html?id=${item.idPasien}`;
        });
        tableBody.appendChild(row);
    });
}

function updatePaginationButton() {
    document.getElementById("prev-btn").disabled = currentPage === 0;
    document.getElementById("next-btn").disabled = (currentPage + 1) * rowsPerPage >= filteredData.length;
}

function updateTable() {
    fetch("/api/data")
        .then((res) => res.json())
        .then((json) => {
            data = json;
            filteredData = data;
            renderTable();
            updatePaginationButton();
        })

    .catch((error) =>
        console.error("Error fetching data:", error)
    );
}

setInterval(updateTable, 1000); // Update every 5 seconds
updateTable(); // Initial fetch

document.getElementById("prev-btn").addEventListener("click", () => {
    if (currentPage > 0) {
        currentPage--;
        renderTable();
        updatePaginationButton();
    }
});

document.getElementById("next-btn").addEventListener("click", () => {
    if ((currentPage + 1) * rowsPerPage < data.length) {
        currentPage++;
        renderTable();
        updatePaginationButton();
    }
});

document.getElementById("rows-per-page").addEventListener("change", function () {
    const val = this.value;
    rowsPerPage = val === "all" ? data.length : parseInt(val);
    currentPage = 0;
    renderTable();
    updatePaginationButton();
});

document.getElementById("search-input").addEventListener("input", function () {
    const searchTerm = this.value.toLowerCase();
    filteredData = data.filter(item => {
        return item.idPasien.toLowerCase().includes(searchTerm) || item.waktu.toLowerCase().includes(searchTerm);
    });
    currentPage = 0;
    renderTable();
    updatePaginationButton();
});