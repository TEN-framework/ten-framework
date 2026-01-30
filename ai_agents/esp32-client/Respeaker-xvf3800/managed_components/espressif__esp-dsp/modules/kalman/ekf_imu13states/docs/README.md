# Extended Kalman Filter for IMU sensor with 13 calculated states 

The extended Kalman filter (EKF) with 13 states designed to be used with data from common 
IMU sensor, like MPU9250 or MPU6050. These sensors provide gyroscope, accelerometer 
and magnetometer data. The sensors have measurement errors, and it's not possible
to use them for calculation system attitude.
For that the EKF should be used.
Current implementation of EKF calculates 13 value to define the system state:
    1. System attitude [0..3]: quaternion that describes absolute value of system attitude (w, x,y,z)
    2. Gyroscope sensor errors[4..6]: bias values (x,y,z)
    3. Magnetometer vector value[7..9]: absolute value of magnetometer sensor (x,y,z)
    4. Magnetometer vector offset[10..12]: absolute value of magnetometer sensor offset (x,y,z)
These values calculated by the EKF and stored to the X vector.
Magnetometer value from the sensor has some value and offset. This information needed to calculate system attitude.
Magnetometer sensor value = R*Magn_Amplitude + Magn_offset. 
Where  R - rotation matrix from system attitude,
Magn_Amplitude - real magnetometer value calculated by the EKF
Magn_offset - magnetometer offset value (called deviation)


## How to use EKF
The EKF has two main methods: state calculation (Process(...)) and state correction (UpdateRefMeasurement() in our case).
The current EKF has one main input value - is a gyroscope (gyro) angular speed. If the gyro has no bias error, then it's enough to call Process(...) method, and EKF 
will calculate system attitude. But, the gyro has a bias error and that's why, the UpdateRefMeasurement(...) must be called, when reference accelerometer and magnetometer values are available.
After the firs start, the EKF will need some time to calculate correct gyro bias and magnetometer deviation. This is a calibration phase. 
To avoid this phase every time after filter started, it's better to store X vector and P matrix to the non-volatile memory, and restore them after system started.
The sequence in this case will be:
Init()-> restore X and P values (if exist) -> go to normal process 


## Adjustable parameters
There two lists of parameters that could be adjusted: R - measurement noise covariance values, and Q - model noise matrix.
The R is an array with values for diagonal matrix. Then smaller value R for reference value, then more EKF will trust for this value.
In normal case, the R values should be between 0.001 and 1.
The Q - is a diagonal (only values in main diagonal are important) noise matrix of state vector. This matrix define how good value in state vector X should be.



