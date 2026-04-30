# Hardware Specifications & Assembly Guide

## ภาพรวม Hardware

โปรเจกต์นี้ใช้หุ่นยนต์ต้นแบบ 2 ตัวที่ใช้ชิ้นส่วนเดียวกัน เพื่อควบคุมตัวแปรในการทดลอง

---

## รายการอุปกรณ์หลัก

### Microcontroller
| อุปกรณ์ | รุ่น | จำนวน | หมายเหตุ |
|---------|------|--------|----------|
| Arduino | Mega 2560 | 1 ชิ้น | ตัวควบการบันทึกข้อมูล |
| Arduino | UNO | 2 ชิ้น | ตัวควบการขับเคลื่อนและการเลี้ยว |

### Motors & Wheels
| อุปกรณ์ | สเปค | จำนวน | หมายเหตุ |
|---------|------|--------|----------|
| DC Motor | 12V / 30RPM | 10 ชิ้น (4WD: 4, 6WD: 6) | พร้อม gearbox |
| ล้อหุ่นยนต์ | เส้นผ่าศูนย์กลาง 65mm | 10 ชิ้น | ยาง rubber grip |
| L298N Motor Driver | Dual H-Bridge | 2 ชิ้น | ต่อหัวอยู่ แต่ละตัว |

### Sensors
| Sensor | Model | จำนวน | วัด |
|--------|-------|--------|-----|
| Current Sensor | ACS712-5A | 1 ชิ้น | กระแสไฟฟ้า (A) |
| IMU | MPU6050 | 2 ชิ้น | Pitch, Roll (°) |
| Voltage Divider | R1=10kΩ, R2=20kΩ | 2 ชุด | แรงดันไฟฟ้า (V) |

### Data Logging
| อุปกรณ์ | รุ่น | จำนวน | หมายเหตุ |
|---------|------|--------|----------|
| SD Card Module | SPI interface | 1 ชิ้น | บันทึก CSV |
| SD Card | 8GB Class 10 | 2 ชิ้น | เก็บข้อมูล |

### Power Supply
| อุปกรณ์ | สเปค | จำนวน | หมายเหตุ |
|---------|------|--------|----------|
| LiPo Battery | 12V 2200mAh 3S | 2 ก้อน | หนึ่งก้อนต่อตัว |
| Step-Down Module | LM2596 (12V→6V) | 4 ชิ้น | จ่ายไฟ motor |

---

## โครงสร้างเครื่องกล

### 4-Wheel Single-Rocker
- ระบบ Single-Rocker ช่วยรับแรงกระแทก
- น้ำหนักรวม: ~1.2 kg
- ขนาด: 30 × 20 × 12 cm
- วัสดุ: อลูมิเนียม + พลาสติก 3D Print

### 6-Wheel Rocker-Bogie
- ระบบ Rocker-Bogie แบบ NASA Mars Rover
- น้ำหนักรวม: ~1.6 kg
- ขนาด: 35 × 22 × 14 cm
- วัสดุ: อลูมิเนียม + พลาสติก 3D Print

---

## การเดินสาย (Wiring)

### ACS712 → Arduino
```
VCC  → 5V
GND  → GND
OUT  → A0 (4WD) / A1 (6WD)
```

### MPU6050 → Arduino
```
VCC  → 5V
GND  → GND
SCL  → SCL (Pin 21)
SDA  → SDA (Pin 20)
```

### SD Card Module → Arduino
```
VCC  → 5V
GND  → GND
MOSI → Pin 51
MISO → Pin 50
SCK  → Pin 52
CS   → Pin 53
```

### L298N Motor Driver → Arduino
```
IN1  → Pin 2
IN2  → Pin 3
IN3  → Pin 4
IN4  → Pin 5
ENA  → Pin 9 (PWM)
ENB  → Pin 10 (PWM)
```

---

## วิธีการ Calibrate Sensors

### MPU6050
1. วาง rover บนพื้นราบ 100%
2. รัน calibration sketch
3. บันทึกค่า offset ลงใน firmware

### ACS712
1. ไม่ต่อโหลดใดๆ
2. อ่านค่า ADC ที่ "zero current"
3. บันทึกค่า baseline (ควรอยู่ที่ ~512 สำหรับ 5V reference)

---

## การประกอบ (Assembly)

1. **ประกอบโครงรถ** ตามแบบ CAD ใน `hardware/CAD_files/`
2. **ติดมอเตอร์** เข้ากับโครง
3. **ต่อวงจร** ตาม wiring diagram ใน `hardware/schematics/`
4. **ติดตั้ง sensors** (ACS712, MPU6050, SD Card)
5. **ต่อแบตเตอรี่** และ step-down modules
6. **Upload firmware** จาก `src/firmware/`
7. **Calibrate sensors** ตามขั้นตอนด้านบน
8. **ทดสอบ** บนพื้นราบก่อน

---

## ปัญหาที่พบและวิธีแก้ไข

| ปัญหา | สาเหตุ | วิธีแก้ไข |
|-------|--------|----------|
| ข้อมูล IMU กระโดด | Vibration จาก motor | เพิ่ม Low-pass filter ใน firmware |
| SD Card เขียนช้า | Buffer เต็ม | เพิ่มขนาด buffer / ลด sampling rate |
| โครงบิดตัว | Material อ่อน | เปลี่ยนเป็น aluminum alloy |

---

*ดูรายละเอียดเพิ่มเติมที่ [RESULTS.md](./RESULTS.md)*
