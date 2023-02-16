const express = require('express');
const dmrLib = require('dmr-lib');


const TOKEN = '5fdg43w221q3125';

const app = express();


app.get('/battery_data', (req, res) => {
    let data = req.query.data;

    if(req.query.token!==TOKEN || data===undefined || data==='') {
        res.status(400).end();
        return;
    }

    data = data.toUpperCase();

    let parts = data.split('_');

    if(parts.length!==3) {
        res.status(400).end();
        return;
    }

    let chipIds = parts[0].split('/').filter((i) => {
        return i.substr(0, 2) === 'A3';
    });

    if(chipIds.length!==1) {
        res.status(400).end();
        return;
    }

    let rawSerial = chipIds[0].substr(2,  chipIds[0].length-4); //Remove chip prefix and CRC
    let batteryData = dmrLib.Protocols.BMS.fromBatteryChip(Buffer.from(parts[2], 'hex'), Buffer.from(parts[1], 'hex'), rawSerial);

    console.log('Battery data:');
    console.log(batteryData);

    res.send('ok');
});


app.listen(80);