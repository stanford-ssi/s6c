const net = require('net');

const UPLINK_NAME = 'uplink.fifo';
const DOWNLINK_NAME = 'downlink.fifo';

function main() {
    // const uplinkServer = net.createServer((stream) => {
    //     setInterval(() => {
    //         console.log('Sending uplink message');
    //         stream.write('test uplink');
    //     }, 1000);
    // });
    //
    // uplinkServer.listen('\\\\.\\' + UPLINK_NAME, () => {
    //     console.log('Uplink created');
    // });

    const downlinkClient = net.connect('\\\\.\\' + DOWNLINK_NAME, function() {
        console.log('Downlink created')
    });

    downlinkClient.on('error', console.log);

    downlinkClient.on('data', function(data) {
        console.log('Downlink data:', data, data.toString());
    });
}

main();
