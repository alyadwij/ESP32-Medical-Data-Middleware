const sqlite3 = require('sqlite3').verbose();
const db = new sqlite3.Database('./data.db');

// Buat tabel jika belum ada
db.serialize(() => {
  db.run(`
    CREATE TABLE IF NOT EXISTS hasil (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      idPasien TEXT,
      waktu TEXT
    )
  `);

  db.run(`
    CREATE TABLE IF NOT EXISTS parameter (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      hasil_id INTEGER,
      nama TEXT,
      nilai TEXT,
      FOREIGN KEY (hasil_id) REFERENCES hasil(id)
    )
  `);
});

module.exports = db;
