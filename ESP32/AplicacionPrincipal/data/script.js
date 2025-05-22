let chartInstance = null;

async function fetchData() {
    try {
        const response = await fetch('http://192.168.1.82/datos');
        if (!response.ok) throw new Error('Error en la respuesta del servidor');
        return await response.json();
    } catch (error) {
        showError('Error al cargar los datos. Intentando nuevamente...');
        throw error;
    }
}

function renderSensors(data) {
    const container = document.getElementById('sensorGrid');
    container.innerHTML = '';

    const sensors = [
        { type: 'temperature', icon: 'fa-thermometer-half', label: 'Temperatura', unit: '°C' },
        { type: 'humidity', icon: 'fa-tint', label: 'Humedad', unit: '%' },
        { type: 'illuminance', icon: 'fa-sun', label: 'Iluminación', unit: 'lux' }
    ];

    sensors.forEach(sensor => {
        const values = data[sensor.type];
        const card = document.createElement('div');
        card.className = 'sensor-card';
        card.innerHTML = `
                    <div class="sensor-header">
                        <div class="sensor-icon ${sensor.type}">
                            <i class="fas ${sensor.icon}"></i>
                        </div>
                        <h2 class="sensor-title">${sensor.label}</h2>
                    </div>
                    
                    <div class="sensor-values">
                        <div class="value-box">
                            <div class="value-label">Mínimo</div>
                            <div class="value">${values.minima}${sensor.unit}</div>
                        </div>
                        <div class="value-box">
                            <div class="value-label">Máximo</div>
                            <div class="value">${values.maxima}${sensor.unit}</div>
                        </div>
                        <div class="value-box">
                            <div class="value-label">Actual</div>
                            <div class="value">${values.actual}${sensor.unit}</div>
                        </div>
                    </div>

                    <div class="progress-container">
                        <div class="progress-bar" style="width: ${calculateProgress(values.actual, values.minima, values.maxima)}%; 
                             background: ${getProgressColor(values.actual, values.minima, values.maxima)}">
                        </div>
                    </div>
                `;
        container.appendChild(card);
    });

    document.getElementById('lastUpdate').textContent = `Última actualización: ${data.fecha}`;
}

function calculateProgress(current, min, max) {
    const range = max - min;
    const position = current - min;
    return Math.min(Math.max((position / range) * 100, 0), 100);
}

function getProgressColor(current, min, max) {
    const threshold = min + ((max - min) * 0.8);
    return current > threshold ? '#F59E0B' : '#10B981';
}

function updateChart(data) {
    const ctx = document.getElementById('historyChart').getContext('2d');

    if (chartInstance) {
        chartInstance.destroy();
    }

    chartInstance = new Chart(ctx, {
        type: 'line',
        data: {
            labels: data.historical.labels,
            datasets: [{
                label: 'Histórico de Temperatura',
                data: data.historical.values,
                borderColor: '#DC2626',
                tension: 0.4,
                fill: false
            }]
        },
        options: {
            responsive: true,
            plugins: {
                legend: { position: 'top' }
            },
            scales: {
                y: { beginAtZero: false }
            }
        }
    });
}

function showError(message) {
    const errorDiv = document.getElementById('errorMessage');
    errorDiv.textContent = message;
    errorDiv.style.display = 'block';
    setTimeout(() => errorDiv.style.display = 'none', 5000);
}

async function updateData() {
    try {
        const data = await fetchData();
        renderSensors(data);
        updateChart(data);
    } catch (error) {
        console.error('Error:', error);
    }
}

updateData();

setInterval(updateData, 10000);