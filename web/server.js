// server.js
const express = require('express');
const bodyParser = require('body-parser');
const path = require('path');
const db = require('./database'); // Import database.js

const app = express();
const PORT = 8000;


app.use(bodyParser.json());
app.use(express.static(path.join(__dirname, 'public'))); // serve frontend


// POST endpoint untuk menerima HL7 yg sudah di-parse ESP32
app.post('/hl7/upload', (req, res) => {
  const parsedData = req.body; // sudah dalam format JSON dari ESP32

  if (!parsedData || !parsedData.metadata || !Array.isArray(parsedData.parameters)) {
  console.error('Format data tidak valid:', parsedData);
  return res.status(400).json({ message: 'Format data tidak valid' });
}

  console.log('Data diterima:', parsedData);

  // Simpan data ke dalam database
  const idPasien = parsedData.metadata.id;
  const waktu = parsedData.metadata.waktu;

  db.run (
    `INSERT INTO hasil (idPasien, waktu) VALUES (?, ?)`,
    [idPasien, waktu],
    function (err) {
      if (err) {
        console.error('Error inserting into hasil:', err);
        return res.status(500).json({ message: 'Error inserting into hasil' });
      }

      const hasilId = this.lastID; // ID dari hasil yang baru saja dimasukkan

      const insertParams = parsedData.parameters.map(p => {
        return new Promise((resolve, reject) => {
          db.run(
            `INSERT INTO parameter (hasil_id, nama, nilai) VALUES (?, ?, ?)`,
            [hasilId, p.name, p.value],
            function (err) {
              if (err) {
                console.error('Error inserting into parameter:', err);
                reject(err);
              } else {
                resolve();
              }
            }
          );
        });
      });

      Promise.all(insertParams)
        .then(() => {
          res.status(200).json({ message: 'Data berhasil disimpan' });
        })
        .catch(err => {
          res.status(500).json({ message: 'Error inserting into parameter' });
        });
    }
  )
});

// GET untuk semua data summary
app.get('/api/data', (req, res) => {
  db.all('SELECT * FROM hasil', [], (err, rows) => {
    if (err) {
      console.error('Error fetching data:', err);
      return res.status(500).json({ message: 'Error fetching data' });
    }

    const summary = rows.map((item, index) => ({
      no: index + 1,
      idPasien: item.idPasien,
      waktu: item.waktu,
    }));

    res.json(summary);
  });
});

// GET untuk detail data by idPasien
app.get('/api/data/:id', (req, res) => {
  const idPasien = req.params.id;
  
  db.get('SELECT * FROM hasil WHERE idPasien = ?', [idPasien], (err, row) => {
    if (err) {
      console.error('Error fetching data:', err);
      return res.status(500).json({ message: 'Error fetching data' });
    }

    if (!row) {
      return res.status(404).json({ message: 'Data tidak ditemukan' });
    }

    db.all('SELECT * FROM parameter WHERE hasil_id = ?', [row.id], (err, params) => {
      if (err) {
        console.error('Error fetching parameters:', err);
        return res.status(500).json({ message: 'Error fetching parameters' });
      }

      const detail = {
        idPasien: row.idPasien,
        waktu: row.waktu,
        parameters: params.map(p => ({
          nama: p.nama,
          nilai: p.nilai
        }))
      };

      res.json(detail);
    });
  });
});

app.listen(PORT, () => {
  console.log(`Server berjalan di http://localhost:${PORT}`);
});
