
### Ketentuan Pengujian 1

Pengujian dilakukan dalam kondisi diusahakan semaksimal mungkin kadar asap dalam **range (0.02% - 0.2%)<sup>(*)</sup>** yang sama setiap pengujiannya sehingga hasil yang didapat akurat.

### Ketentuan Pengujian 2

Pengujian dilakukan dimana sensor MQ-2 dan sumber asap berada dalam satu garis lurus.

| Ketinggian | Waktu hingga terdeteksi |
| ---------- | ------------------------|
| 5 cm       | ~1 detik                |
| 10 cm      | 2 detik                 |
| 20 cm      | ~5 detik                |
| 30 cm      | 7 detik                 |

### <sup>(*)</sup>Kalkulasi PPM

Referensi: https://thestempedia.com/tutorials/interfacing-mq-2-gas-sensor-with-evive/#main

![MQ-2 Gas Sensor Sensitivity Characteristics](https://thestempedia.com/wp-content/uploads/2019/03/Gas-Sensor-MQ2.png "MQ-2 Characteristics")

RS merupakan resistansi dari sensor yang berubah bergantung pada konsentrasi gas.

`RS = (Vin - Vout) / Vout`

R0 merupakan resistansi dari sensor pada konsentrasi yang diketahui tanpa ada kehadiran gas, atau dengan kata lain pada udara bersih.

`R0 = RS / 9.8`

Kode program untuk mengukur R0: https://raw.githubusercontent.com/dimasarna/sistem-keamanan-gudang/main/mencari_nilai_r0_mq2.ino

Untuk mengetahui nilai konsentrasi gas dalam ppm maka digunakan persamaan:

`x = 10 ^ {[log(y) - b] / m}`

Pada tabel dibawah dapat ditemukan nilai **m** dan **b** untuk gas yang berbeda-beda.

| Gas | Value at 200 | Value at 10000 | Value at 5000 | m | b |
| -   | -            | -              | -             | - | - |
| H2      | 2.1 | 0.33 | 0.46  | -0.47305447  | 1.412572126 |
| LPG     | 1.6	| 0.27 | 0.37  | -0.454838059 | 1.25063406  |
| Methane | 3   | 0.7  | 0.94  | -0.372003751 | 1.349158571 |
| CO      | 5.1 | 1.35 | 1.8   | -0.33975668  | 1.512022272 |
| Alcohol | 2.8 | 0.65 | 0.85  | -0.373311285 | 1.310286169 |
| Smoke   | 3.4 | 0.6  | 0.95  | -0.44340257  | 1.617856412 |
| Propane | 1.7 | 0.28 | 0.385 | -0.461038681 | 1.290828982 |
