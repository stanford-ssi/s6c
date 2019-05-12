const net = require('net');
const HOST = '127.0.0.1';
const PORT = 8700;

const client = new net.Socket();
let uplinkInterval;

client.connect(PORT, HOST, function() {
    console.log('Client connected');

    uplinkInterval = setInterval(() => {
        console.log('Sending uplink');
        client.write('Example uplink\n');
    }, 1000);
});

client.on('data', function(data) {
    console.log('Downlink', data);
});

client.on('close', function() {
    console.log('Connection closed');
    clearInterval(uplinkInterval);
});
