1. Configure the USER Config parameter in the motor. h file before using the stepper motor driver
2. Partial parameter description:
    Drive three entry parameters, with default values as follows:
    Maxspeed=MOTORDEF_SPEED;//The maximum speed of the motor, which is the square wave frequency of the driving motor
    maxstep[n] = MOTORn_MAX_STEP; // Maximum step range of motor n motion
    The concept of "step count, coordinates, position, and angle" in the drive motor corresponds to the concept of "step" in the stepper motor.
    The coordinate range of motor motion is [0, maxstep], and the value of maxstep is determined based on actual product testing.
    The default gear for motor rotation is 1, which means the maximum speed is maxspeed. The actual speed of the motor is maxspeed/gear [n]
    Motor gear range: [1, MOTOR_GEAR]
3. Explanation of Drive Control Commands
    MOTOR-STOP//Stop motor rotation
    MOTOR-STEPS//Rotating motor, relative coordinates, the positive and negative values of the parameters represent the direction (relative to the current actual coordinates)
    MOTORO-COORDINATE//Rotating motor, absolute coordinates
    MOTOROVNet VNet//Get the reset status, current coordinates, target coordinates, gear, and speed of the motor
    MOTOR_S_SPEED//Set motor speed and gear
    MOTOR RESET//Motor reset calibration
    MOTOROD_RESET//Skip the reset step and directly specify the current coordinates
4. Driver compilation and loading
    Modify the ISVP-INV_KERNEL_DIR kernel path in the Makefile
    make
    insmod motor.ko maxstep=4000,700
    Note: The maxstep parameter here will override the default parameters configured by the MOTORn_MAX-STEP macro.
5. Test application compilation
    make step_test
6. Suggestions
    The higher the maxspeed value, the higher the timer interrupt frequency.
    If there is no need for two motors with different speeds, it is recommended to set the gears MOTOR_GEAR and gear [n] to 1 and use the stMotorSpeed.speed parameter to control the speed, which can reduce the frequency of timer interruptions.
7. MOTOR-REET command resets calibration logic:
    The motor rotates towards the end limit direction (positive direction) until the end limit switch is triggered, and the coordinates at this time are recorded as (maxstep [0], maxstep [1]).
    Reset completed.
    After the reset is completed, the motor will rotate towards the (maxstep [0]/2, maxstep [1]/2) coordinates. At this point, the motor can already be controlled.
8. Use the step_test program to debug the MOTORn_MAX-STEP parameter and determine the starting and ending limits:
    1) The drive specifies two directions for the rotation of the motor: the "positive direction" and the "negative direction". The first time I used it, I didn't know the corresponding relationship between the positive/negative direction and the actual rotation direction of the device.
    2) Turn off the power, first screw the two motors to the center position, use the MOTOR-REET command to make the motors rotate, and after obtaining the direction of rotation of the motors, use the MOTOR-STOP command to stop the motors.
    3) The rotation direction obtained in step 2) is the "positive direction", and the limit switch corresponding to the direction is the end limit.
    4) Turn off the power and rotate the two motors in the "negative direction" to the starting limit, which is the (0,0) coordinate.
    5) First, provide a set of smaller maxstep parameters and use the MOTOR-REET command to rotate the motor. Wait until the motor reaches the (maxstep [0], maxstep [1]) coordinates and observe whether it reaches the actual endpoint limit.
    6) If there is no transition, increase MOTORn_MAX-STEP; If there is a blockage caused by early rotation, reduce MOTORn_MAX-STEP. Repeat this step.


1.步进电机驱动使用前先配置 motor.h 文件中USER Config参数

2.部分参数说明：
  驱动三个入口参数，默认值如下：
   maxspeed = MOTOR_DEF_SPEED; //电机最大速度，即驱动电机方波频率
   maxstep[n] = MOTORn_MAX_STEP; //电机n运动最大步数范围

  驱动中电机“步数、坐标、位置、角度”是同一概念，对应步进电机中“步”的概念。
  电机运动的坐标范围：[0,maxstep]，maxstep 的值是根据实际产品测试出的。
  默认电机转动档位(gear)为1，即最大速度 maxspeed，电机实际速度 speed=maxspeed/gear[n]
  电机档位范围：[1,MOTOR_GEAR]

3.驱动控制命令说明
  MOTOR_STOP       //停止电机转动
  MOTOR_STEPS      //转动电机，相对坐标，参数的正负代表方向（相对与当前实际坐标）
  MOTOR_COORDINATE //转动电机，绝对坐标
  MOTOR_GET_STATUS //获取电机复位状态、当前坐标、目标坐标、档位、速度
  MOTOR_SET_SPEED  //设置电机速度和档位
  MOTOR_RESET      //电机复位校准
  MOTOR_NO_RESET   //跳过复位步骤，直接指定当前坐标

4.驱动编译加载
  修改Makefile中 TASSADAR_ENV_KERNEL_DIR kernel路径
  make

  insmod motor.ko maxstep=4000,700

  注意：这里的 maxstep 参数会覆盖 MOTORn_MAX_STEP 宏配置的默认参数。

5.测试应用程序编译
  make step_test


6.建议
  速度(maxspeed)值越大，定时器中断频率越高。
  如果没有两个电机不同速度的需求，建议档位 MOTOR_GEAR 和gear[n]都设为1，使用 stMotorSpeed.speed 参数控制速度，这样可以减少定时器中断的频率。

7.MOTOR_RESET 命令复位校准逻辑：
    电机向终点限位方向（正方向）转动，直至触发终点限位开关，记录此时坐标为(maxstep[0]，maxstep[1])。
    复位结束。
    复位结束后电机会向(maxstep[0]/2，maxstep[1]/2)坐标转动。此时电机已经可以被控制。

8. 使用step_test程序调试 MOTORn_MAX_STEP 参数以及确定起点限位和终点限位：
  1）驱动对电机旋转的方向规定了“正方向”和“负方向”两个方向。第一次使用是不知道正/负方向和实际设备旋转方向的对应关系。
  2）断电，先将两电机拧到中心点位置，使用 MOTOR_RESET 命令让电机转动，得到电机旋转方向后使用 MOTOR_STOP 命令让电机停转。
  3）第2)步得到的旋转方向即是“正方向”，对应方向的限位开关既是终点限位。
  4）断电将两电机向“负方向”拧到起点限位，即(0,0)坐标。
  5）先给一组较小的 maxstep 参数，使用 MOTOR_RESET 命令让电机转动，等到电机转到(maxstep[0],maxstep[1])坐标，观察是否转到实际终点限位。
  6）若没有转到则增大 MOTORn_MAX_STEP；若提前转到，发生堵转，则减小MOTORn_MAX_STEP。重复此步骤。

