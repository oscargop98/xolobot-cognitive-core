# Xolobot Cognitive Core

Repositorio principal del programa **Apoyo para implementar la Arquitectura Cognitiva Inspirada en Neuronas Espejo**. Este proyecto contiene la migración y modernización del entorno de simulación del manipulador antropomórfico Xolobot hacia **ROS 2 Jazzy Jalisco** y **Gazebo Harmonic**.

Incluye la implementación de la Corteza Premotora (nodo `SimulationController`), Corteza Motora Primaria (`JointTrajectoryController`) y Cortezas Somatosensorial/Parietal (sensores táctiles).

## Requisitos Previos
El proyecto está diseñado para ejecutarse de dos maneras, dependiendo de la configuración de tu equipo.

---

## Opción A: Ejecución Nativa (Requiere Ubuntu 24.04 y ROS 2 Jazzy)

El proyecto incluye un set de comandos especiales (alias) diseñados por Oscar Gonzalez para operar y depurar la simulación rápidamente. 

### Panel de Control (Comandos Disponibles):
* `xolo_sim`: Compila el servidor y lanza el entorno físico de Gazebo (Corteza Motora).
* `xolo_brain`: Lanza el cerebro/controlador lógico (Corteza Premotora).
* `xolo_kill`: 💀 Botón de pánico. Aniquila todos los procesos de ROS 2 y Gazebo.
* `xolo_cam`: Muestra las coordenadas actuales de la cámara.
* `xolo_view`: Mueve la cámara automáticamente a la pose ideal de observación.
* `xolo_jtc`: Abre el Teach Pendant (`rqt_joint_trajectory_controller`) para calibración manual.
* `xolo_kill_jtc`: Cierra el controlador manual.
* `xolo_float`: Depura la lata flotante publicando en el tópico magnet_off.

### Flujo de ejecución nativa:
1. Asegúrate de tener los alias inyectados en tu `~/.bashrc`.
2. En la Terminal 1, ejecuta: `xolo_sim`
3. En la Terminal 2, ejecuta: `xolo_brain`

---

## Opción B: Ejecución con Docker (Para cualquier otro equipo)

Si tu equipo no cuenta con ROS 2 Jazzy, puedes levantar la arquitectura completa utilizando contenedores. Esto garantiza que el cerebro y el simulador coexistan en el mismo espacio de red e IPC.

1. Construir y levantar el entorno cognitivo completo:
   ```bash
   docker compose up --build


---
# Brazo robótico antropomórfico 🦾
## src
Carpeta principal de este repositorio.
## Configurar .bash
```
# --- Configuración de ROS 2 y Gazebo ---
source /opt/ros/iron/setup.bash
source ~/ros2_ws/install/setup.bash
export PATH=${PATH}:${HOME}/bin

# Rutas de Plugins y Modelos de Gazebo
export GAZEBO_PLUGIN_PATH=/opt/ros/iron/lib:$GAZEBO_PLUGIN_PATH
export GAZEBO_MODEL_PATH=/home/oscarss2/ros2_ws/src/xolobot_arm/models:$HOME/.gazebo/models:$GAZEBO_MODEL_PATH # Cambiar oscarss2 si es necesario por el usuario que lo ejecute

```

---
# INICAR EL PROYECTO

## TERMINAL 1 - PROYECTO CON LAUNCH
```
cd ~/ros2_ws/
colcon build
source install/setup.bash
ros2 launch xolobot_arm xolobot_arm_control.launch.py # Inicia la simulación. Este comando mantendrá esta terminal ocupada
```
## TERMINAL 2 - EL SERIVIDOR
```
cd ~/ros2_ws/
colcon build
source install/setup.bash
ros2 run xolobot_arm_server xolobot_arm_server # Como la simulación ya está corriendo, el controller_manager estará activo y te responderá
```

## Paso 3
```
cd ~/ros2_ws/
colcon build
source install/setup.bash
ros2 control list_controllers
```
## Nuevos comandos
```
ros2 control list_controllers

ros2 pkg list | grep ros2_control
    #gazebo_ros2_control
    #ros2_control
    #ros2_control_test_assets
    #ros2_controllers'' 
ros2 pkg list | grep libgazebo_ros2_control.so 

ros2 pkg list | grep joint_trajectory_controller
    # joint_trajectory_controller

# Matar los procesos
pkill -9 gzserver
pkill -9 gzclient
pkill -9 gazebo
```
---
## Comandos Anteriores
### Para lanzar el mundo - ejecutar 
1. colcon build
2. source install/setup.bash
3. ros2 launch xolobot_arm xolobot_arm_control.launch.py
4. ros2 run xolobot_arm_server xolobot_arm_server

### Para escuchar tópicos
1. ros2 topic echo /bumper_states

### Para ver los controladores disponibles
1. ros2 control list_controllers

### Para matar procesos
1. pkill -9 gzserver
2. pkill -9 gzclient
3. pkill -9 gazebo

### Paquete de Python
ros2 pkg create --build-type ament_python --license Apache-2.0 modulos

## src_lata
Esta es una carpeta que puede servir de prueba. Tiene el proyecto del brazo robótico antropomórfico que carga la lata y simula el agarre.
