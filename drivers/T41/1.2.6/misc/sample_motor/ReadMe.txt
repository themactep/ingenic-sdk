驱动目前只适配了T31 T40 T41

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
  修改Makefile中 ISVP_ENV_KERNEL_DIR kernel路径
  make

  insmod motor.ko maxstep=4000,700

  注意：这里的 maxstep 参数会覆盖 MOTORn_MAX_STEP 宏配置的默认参数。

5.测试应用程序编译
  make step_test


6.建议
  速度(maxspeed)值越大，定时器中断频率越高。
  如果没有两个电机不同速度的需求，建议档位 MOTOR_GEAR 和gear[n]都设为1，使用 stMotorSpeed.speed 参数控制速度，这样可以减少定时器中断的频率。

7.MOTOR_RESET 命令复位校准逻辑：
    未校准前默认当前电机处于(0,0)坐标，然后向正方向转到(maxstep[0],maxstep[1])坐标，这样无论复位前电机处于什么坐标，都可以使电机软件上(maxstep[0]，maxstep[1])坐标与实际终点坐标对应。
    （电机若提前到达终点坐标，机械结构上的限位会让电机堵转）
    电机转到(maxstep[0],maxstep[1])坐标后复位结束，然后电机会向(maxstep[0]/2，maxstep[1]/2)坐标转动。此时电机已经可以被控制。

8. 使用step_test程序调试 MOTORn_MAX_STEP 参数：
  1）驱动对电机旋转的方向规定了“正方向”和“负方向”两个方向。第一次使用是不知道正/负方向和实际设备旋转方向的对应关系的。
  2）断电，先将两电机拧到中心点位置，使用 MOTOR_RESET 命令让电机转动，得到电机旋转方向后使用 MOTOR_STOP 命令让电机停转。
  3）第2)步得到的旋转方向即是“正方向”。
  4）断电将两电机向“负方向”拧到起点，即(0,0)坐标。
  5）先给一组较小的 maxstep 参数，使用 MOTOR_RESET 命令让电机转动，等到电机转到(maxstep[0],maxstep[1])坐标，观察是否转到实际终点坐标。
  6）若没有转到则增大 MOTORn_MAX_STEP；若提前转到，发生堵转，则减小MOTORn_MAX_STEP。重复此步骤。

