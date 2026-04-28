# Rover Mobility Comparison Project

![Status](https://img.shields.io/badge/Status-Work%20in%20Progress-yellow)
![License](https://img.shields.io/badge/license-MIT-blue)
![Python](https://img.shields.io/badge/Python-3.8%2B-blue)

**A comparative study of mobility and performance between 6-wheeled (Rocker-Bogie) and 4-wheeled (Single-Rocker) rovers.**

## 📑 Table of Contents

- [Overview](#-overview)
- - [Project Status](#-project-status)
  - - [Key Features](#-key-features)
    - - [Objectives](#-objectives)
      - - [Methodology](#-methodology)
        - - [Key Results](#-key-results)
          - - [Hardware & Components](#-hardware--components)
            - - [File Structure](#-file-structure)
              - - [Installation](#-installation)
                - - [Usage](#-usage)
                  - - [Results & Analysis](#-results--analysis)
                    - - [Future Work](#-future-work)
                      - - [Contributing](#-contributing)
                        - - [License](#license)
                          - - [Author](#author)
                           
                            - ---

                            ## 🎯 Overview

                            โครงงานนี้มีวัตถุประสงค์เพื่อศึกษาและเปรียบเทียบสมรรถนะของระบบขับเคลื่อนแบบ 4 ล้อ (Single-Rocker) และ 6 ล้อ (Rocker-Bogie) ภายใต้สภาวะทางลาดชัน เพื่อประเมินความเหมาะสมในการประยุกต์ใช้กับยานสำรวจดาวเคราะห์

                            This project conducts a detailed comparative analysis of two rover wheel configurations:
                            - **4-Wheel Single-Rocker Configuration**: Lighter, more energy-efficient
                            - - **6-Wheel Rocker-Bogie Configuration**: More stable, better terrain adaptability
                             
                              - ---

                              ## 🚧 Project Status

                              **Current Phase:** Preliminary Results (Work in Progress)

                              - ✅ Hardware prototypes completed (both 4-wheel and 6-wheel platforms)
                              - - ✅ Testing conducted on 15° and 20° inclined surfaces
                                - - ✅ Data collection and initial analysis complete
                                  - - 🔄 Additional testing rounds planned
                                    - - 📊 Results validation in progress
                                     
                                      - ---

                                      ## ⭐ Key Features

                                      - **Dual Platform Testing**: Direct comparison between two wheel configurations
                                      - - **Standardized Components**: Same motors, wheels, and control systems (controlled variables)
                                        - - **Real-time Data Collection**: Voltage, current, and orientation measurements
                                          - - **Statistical Analysis**: Mean values and standard deviation calculations
                                            - - **Multiple Terrain Tests**: Testing on 15° and 20° inclines
                                              - - **Comprehensive Logging**: CSV format data logging via SD card
                                               
                                                - ---

                                                ## 🎯 Objectives

                                                1. **Stability Comparison**: Compare Pitch/Roll stability between 4-wheel and 6-wheel systems
                                                2. 2. **Energy Efficiency Analysis**: Analyze power consumption (Power = V × I) for both configurations
                                                   3. 3. **Trade-off Study**: Investigate the balance between "Stability" and "Energy Efficiency"
                                                      4. 4. **Terrain Adaptability**: Assess performance on varied incline angles
                                                        
                                                         5. ---
                                                        
                                                         6. ## ⚙️ Methodology
                                                        
                                                         7. ### Development
                                                         8. - Developed two robotic prototypes (4-wheel and 6-wheel platforms)
                                                            - - Used identical motors, wheels, and control systems for fair comparison
                                                              - - Controlled variable approach to ensure accuracy
                                                               
                                                                - ### Testing
                                                                - - **Test Surfaces**: 15° and 20° inclined ramps
                                                                  - - **Data Measurement Points**:
                                                                    -   - Electric current (ACS712 sensor)
                                                                        -   - Voltage
                                                                            -   - Orientation angles (MPU6050: Pitch, Roll)
                                                                                - - **Data Logging**: CSV format via SD Card module
                                                                                  - - **Analysis**: Mean values and standard deviation calculations
                                                                                   
                                                                                    - ---

                                                                                    ## 📊 Key Results (Preliminary)

                                                                                    | Metric | 4-Wheel | 6-Wheel | Notes |
                                                                                    |--------|---------|---------|-------|
                                                                                    | **Energy Consumption** | Baseline | +5-8% | 6-wheel uses slightly more energy |
                                                                                    | **Stability (Pitch/Roll)** | Higher variance | Lower variance | 6-wheel more stable |
                                                                                    | **Wheel Slip** | Occurs on steep inclines | Better grip | 6-wheel has superior traction |
                                                                                    | **Weight Distribution** | Uneven | More balanced | 6-wheel better load distribution |

                                                                                    ### Key Findings:
                                                                                    - 🔋 **6-wheel system** uses approximately **5-8% more power**
                                                                                    - - ⚖️ **6-wheel system** demonstrates **higher stability** (lower Pitch/Roll variance)
                                                                                      - - ⚠️ **4-wheel system** experiences **wheel slip** on steep inclines
                                                                                        - - 📈 **Clear trade-off** observed between **Stability vs Energy Efficiency**
                                                                                         
                                                                                          - ---

                                                                                          ## 🔧 Hardware & Components

                                                                                          ### Main Components
                                                                                          - **Microcontroller**: Arduino/STM32 (specify your MCU)
                                                                                          - - **Motors**: [Your Motor Spec]
                                                                                            - - **Wheels**: [Your Wheel Type]
                                                                                              - - **Current Sensor**: ACS712 (±5A/±20A variant)
                                                                                                - - **IMU Sensor**: MPU6050 (Accelerometer + Gyroscope)
                                                                                                  - - **SD Card Module**: For data logging
                                                                                                    - - **Power Supply**: [Your Battery/Power Specs]
                                                                                                     
                                                                                                      - ### Mechanical Design
                                                                                                      - - Single-Rocker mechanism (4-wheel configuration)
                                                                                                        - - Rocker-Bogie mechanism (6-wheel configuration)
                                                                                                          - - Custom-designed chassis for test platforms
                                                                                                           
                                                                                                            - For detailed hardware specifications, see [HARDWARE.md](./HARDWARE.md)
                                                                                                           
                                                                                                            - ---
                                                                                                            
                                                                                                            ## 📂 File Structure
                                                                                                            
                                                                                                            ```
                                                                                                            rover-wheel-compariso/
                                                                                                            ├── README.md                   # This file
                                                                                                            ├── LICENSE                     # MIT License
                                                                                                            ├── CONTRIBUTING.md             # Contribution guidelines
                                                                                                            ├── HARDWARE.md                 # Hardware specifications & assembly
                                                                                                            ├── RESULTS.md                  # Detailed results & analysis
                                                                                                            ├── requirements.txt            # Python dependencies
                                                                                                            ├── .gitignore                  # Git ignore patterns
                                                                                                            │
                                                                                                            ├── hardware/                   # Hardware designs & schematics
                                                                                                            │   ├── schematics/            # Electrical schematics
                                                                                                            │   ├── CAD_files/             # 3D design files
                                                                                                            │   └── assembly_guide/        # Assembly instructions
                                                                                                            │
                                                                                                            ├── src/                        # Source code
                                                                                                            │   ├── firmware/              # Microcontroller firmware
                                                                                                            │   ├── analysis/              # Data analysis scripts
                                                                                                            │   └── utils/                 # Utility functions
                                                                                                            │
                                                                                                            ├── data/                       # Experimental data
                                                                                                            │   ├── raw_data/              # Original CSV files
                                                                                                            │   └── processed_data/        # Analyzed/cleaned data
                                                                                                            │
                                                                                                            └── results/                    # Results & visualizations
                                                                                                                ├── plots/                 # Generated graphs
                                                                                                                └── reports/               # Analysis reports
                                                                                                            ```
                                                                                                            
                                                                                                            ---
                                                                                                            
                                                                                                            ## 📥 Installation
                                                                                                            
                                                                                                            ### Prerequisites
                                                                                                            - Python 3.8 or higher
                                                                                                            - - Arduino IDE (or PlatformIO) for firmware
                                                                                                              - - Git
                                                                                                               
                                                                                                                - ### Hardware Setup
                                                                                                                - 1. Assemble both rover platforms following [HARDWARE.md](./HARDWARE.md)
                                                                                                                  2. 2. Flash the firmware to your microcontroller
                                                                                                                     3. 3. Connect sensors and data logging components
                                                                                                                        4. 4. Calibrate MPU6050 and ACS712 sensors
                                                                                                                          
                                                                                                                           5. ### Software Setup
                                                                                                                          
                                                                                                                           6. ```bash
                                                                                                                              # Clone the repository
                                                                                                                              git clone https://github.com/EKKLESIA123/rover-wheel-compariso.git
                                                                                                                              cd rover-wheel-compariso

                                                                                                                              # Install Python dependencies
                                                                                                                              pip install -r requirements.txt
                                                                                                                              ```
                                                                                                                              
                                                                                                                              ---
                                                                                                                              
                                                                                                                              ## 🚀 Usage
                                                                                                                              
                                                                                                                              ### Data Collection
                                                                                                                              1. Place rover on test surface (15° or 20° incline)
                                                                                                                              2. 2. Start data logging on microcontroller
                                                                                                                                 3. 3. Run rover for specified duration
                                                                                                                                    4. 4. Download CSV files from SD card
                                                                                                                                      
                                                                                                                                       5. ### Data Analysis
                                                                                                                                       6. ```bash
                                                                                                                                          # Run analysis scripts
                                                                                                                                          python src/analysis/data_analysis.py

                                                                                                                                          # Generate plots
                                                                                                                                          python src/analysis/plot_results.py
                                                                                                                                          ```
                                                                                                                                          
                                                                                                                                          ---
                                                                                                                                          
                                                                                                                                          ## 📊 Results & Analysis
                                                                                                                                          
                                                                                                                                          Detailed results, graphs, and statistical analysis are available in [RESULTS.md](./RESULTS.md)
                                                                                                                                          
                                                                                                                                          ### Current Findings
                                                                                                                                          - Preliminary data shows clear trade-offs between configurations
                                                                                                                                          - - 6-wheel system superior for stability-critical applications
                                                                                                                                            - - 4-wheel system better for energy-constrained missions
                                                                                                                                              - - Terrain type significantly affects performance differences
                                                                                                                                               
                                                                                                                                                - ---
                                                                                                                                                
                                                                                                                                                ## 🔮 Future Work
                                                                                                                                                
                                                                                                                                                - ✔️ Increase number of test iterations for statistical confidence
                                                                                                                                                - - ✔️ Test on more complex terrain (gravel, sand, rocky surfaces)
                                                                                                                                                  - - ✔️ Improve suspension system to reduce structural twisting
                                                                                                                                                    - - ✔️ Optimize weight distribution in both configurations
                                                                                                                                                      - - ✔️ Implement machine learning for terrain prediction
                                                                                                                                                        - - ✔️ Conduct long-duration endurance tests
                                                                                                                                                          - - ✔️ Test hybrid configurations (8-wheel alternatives)
                                                                                                                                                           
                                                                                                                                                            - ---
                                                                                                                                                            
                                                                                                                                                            ## 🤝 Contributing
                                                                                                                                                            
                                                                                                                                                            We welcome contributions! Please see [CONTRIBUTING.md](./CONTRIBUTING.md) for guidelines on:
                                                                                                                                                            - Reporting bugs
                                                                                                                                                            - - Proposing features
                                                                                                                                                              - - Submitting pull requests
                                                                                                                                                               
                                                                                                                                                                - ---
                                                                                                                                                                
                                                                                                                                                                ## 📜 License
                                                                                                                                                                
                                                                                                                                                                This project is licensed under the **MIT License** - see the [LICENSE](./LICENSE) file for details.
                                                                                                                                                                
                                                                                                                                                                ---
                                                                                                                                                                
                                                                                                                                                                ## 👤 Author
                                                                                                                                                                
                                                                                                                                                                **Phatharawin**
                                                                                                                                                                
                                                                                                                                                                - GitHub: [@EKKLESIA123](https://github.com/EKKLESIA123)
                                                                                                                                                                - - Project Started: April 2026
                                                                                                                                                                 
                                                                                                                                                                  - ---
                                                                                                                                                                  
                                                                                                                                                                  ## 📞 Questions or Support?
                                                                                                                                                                  
                                                                                                                                                                  If you have questions or need support:
                                                                                                                                                                  - Open an [Issue](../../issues)
                                                                                                                                                                  - - Check [HARDWARE.md](./HARDWARE.md) for hardware questions
                                                                                                                                                                    - - See [RESULTS.md](./RESULTS.md) for analysis details
                                                                                                                                                                     
                                                                                                                                                                      - ---
                                                                                                                                                                      
                                                                                                                                                                      **Last Updated**: April 2026
                                                                                                                                                                      
