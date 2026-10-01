# Guía de Programación — Xolobot Cognitive Core

Documento para quien quiera modificar, extender o depurar el sistema — no solo ejecutarlo. Cubre desde cómo funciona el ciclo completo hasta recetas concretas para los cambios más frecuentes.

---

## 1. El ciclo completo — qué pasa cuando ejecutas `xolo_sim` + `xolo_brain`

```
[xolo_sim]
  1. colcon build --packages-select xolobot_arm_server  →  compila el nodo C++
  2. ros2 launch xolobot_arm xolobot_arm_control.launch.py
       ├── Lanza Gazebo Harmonic con la escena (brazo + soporte + lata)
       ├── Lanza robot_state_publisher (modelo URDF/SDF → TF)
       ├── Lanza controller_manager + JointTrajectoryController (Corteza Motora)
       └── Lanza ros_gz_bridge  →  traduce tópicos Gazebo ↔ ROS 2

[xolo_brain]   (15 segundos después de xolo_sim para que /clock fluya)
  3. ros2 run xolobot_arm_server xolobot_arm_server --ros-args -p use_sim_time:=true
       └── SimulationController inicia su timer (cada 2.5s)

[Ciclo de control — SimulationController]
  t=0–10s   warm-up: 4 ticks sin hacer nada (espera estabilización del reloj)
  t=10s     Corteza Premotora: publica trayectoria de 2 puntos al JTC
                punto 1 (t+1.5s): waypoint alto — codo elevado sobre el pedestal
                punto 2 (t+3.0s): pose de agarre — brazo desciende sobre la lata
  t=13s     JTC ejecuta la trayectoria articulación por articulación
  t=13.5s   Bumper de palma o dedo detecta contacto con la lata
                → colisionDetectada = true
                → se cierra la mano (trayectoria de dedos)
                → se activa el imán magnético (gz topic magnet_on)
  t=18s     Timer de elevación: Corteza Premotora publica pose de elevación
                → el brazo sube con la lata sujeta
```

---

## 2. Mapa de archivos — qué tocar para cada tipo de cambio

| Qué quieres cambiar | Archivo |
|---|---|
| Geometría/posición de la lata | `src/xolobot_arm/models/utileria/objeto.sdf` |
| Geometría/posición del pedestal | `src/xolobot_arm/models/utileria/soporte.sdf` |
| Pose de agarre del brazo | `src/xolobot_arm_server/src/SimulationController.cpp` → función `generaAleatorios()` |
| Pose de elevación post-agarre | `src/xolobot_arm_server/src/SimulationController.cpp` → función `moverHombro()` |
| Velocidad del timer de control | `src/xolobot_arm_server/src/SimulationController.cpp` → `create_timer(milliseconds(2500), ...)` |
| Sensores de contacto (bumpers) | `src/xolobot_arm/config/bridge.yaml` + suscriptores en `SimulationController.cpp` |
| Tolerancias del controlador de joints | `src/xolobot_control/config/xolobot_control.yaml` |
| Qué nodos se lanzan y con qué parámetros | `src/xolobot_arm/launch/xolobot_arm_control.launch.py` |
| Modelo físico del brazo (joints, colisiones) | `src/xolobot_arm/models/xolobot_arm/xolobot_arm.sdf` |

---

## 3. Modificar el mundo físico — archivos SDF

Los archivos SDF (Simulation Description Format) definen los objetos de la escena: su geometría, masa, fricción y posición en el mundo.

### Estructura de un archivo SDF de objeto

```xml
<sdf version='1.7'>
  <model name='nombre_del_modelo'>

    <!-- Pose en el mundo: X Y Z Roll Pitch Yaw (en metros y radianes) -->
    <pose>X Y Z 0 0 0</pose>

    <static>false</static>  <!-- false = se mueve con física; true = fijo en el mundo -->

    <link name='link'>

      <!-- Propiedades de masa e inercia -->
      <inertial>
        <mass>MASA_EN_KG</mass>
        <inertia>
          <ixx>Ixx</ixx> <ixy>0</ixy> <ixz>0</ixz>
          <iyy>Iyy</iyy> <iyz>0</iyz>
          <izz>Izz</izz>
        </inertia>
      </inertial>

      <!-- Geometría de COLISIÓN: la usa el motor de física para detectar contacto -->
      <collision name='collision'>
        <geometry> ... </geometry>
      </collision>

      <!-- Geometría VISUAL: solo lo que ve la cámara, puede diferir de la colisión -->
      <visual name='visual'>
        <geometry> ... </geometry>
        <material> ... </material>
      </visual>

    </link>
  </model>
</sdf>
```

### Geometrías disponibles

```xml
<!-- Caja -->
<box><size>ANCHO LARGO ALTO</size></box>

<!-- Cilindro — eje Z es el eje longitudinal del cilindro -->
<cylinder>
  <radius>RADIO</radius>
  <length>ALTURA</length>
</cylinder>

<!-- Esfera -->
<sphere><radius>RADIO</radius></sphere>

<!-- Malla 3D (archivo .dae o .obj) -->
<mesh><uri>model://nombre_modelo/meshes/archivo.dae</uri></mesh>
```

### Cómo calcular el tensor de inercia

El motor de física necesita valores correctos de inercia para simular el movimiento realista del objeto.

**Para una caja** (masa m, dimensiones x, y, z):
```
Ixx = m*(y² + z²)/12
Iyy = m*(x² + z²)/12
Izz = m*(x² + y²)/12
```

**Para un cilindro** (masa m, radio r, altura h):
```
Ixx = Iyy = m*(3r² + h²)/12
Izz = m*r²/2
```

### Posición Z — cómo calcularla para que el objeto quede sobre el pedestal

El pedestal (`soporte.sdf`) tiene:
- Centro en Z = `0.816173`
- Altura = `0.06 m`
- Superficie superior en Z = `0.816173 + 0.03 = 0.846173`

Para que un objeto quede apoyado sobre el pedestal, su centro debe estar en:
```
Z_objeto = Z_superficie_pedestal + (altura_objeto / 2)
Z_objeto = 0.846173 + (altura_objeto / 2)
```

---

## 4. Modificar el comportamiento del brazo

Toda la lógica está en `SimulationController.cpp`. El flujo principal está en dos funciones:

### `generaAleatorios()` — fase de aproximación y agarre

Esta función se llama cada 2.5 segundos. Controla la fase de aproximación.

```cpp
// Las 6 articulaciones del brazo (índices 0-5 del vector de 21 joints)
// Índice 0: jnt_pecho_hombro   — rotación horizontal de la base
// Índice 1: jnt_hombro_hombro  — abducción lateral
// Índice 2: jnt_hombro_biceps  — flexión del hombro (controla altura)
// Índice 3: jnt_biceps_codo    — flexión del codo
// Índice 4: jnt_codo_antebrazo — pronación/supinación
// Índice 5: jnt_antebrazo_palma — flexión de muñeca

point.positions = {
    0.159,    // [0] rotación hacia la lata
    0.0,      // [1] sin abducción lateral
   -1.2874,   // [2] flexión del hombro (pose final de agarre)
    1.5010,   // [3] codo extendido
   -1.068,    // [4] pronación del antebrazo
   -1.1309,   // [5] muñeca flexionada hacia abajo
    // índices 6-20: dedos (ver moverHombro para valores de cierre)
};

// El waypoint es el punto INTERMEDIO (vuelo sobre el pedestal)
// Solo el índice [2] cambia — el codo sube para no chocar
waypoint.positions[2] = -0.5;  // codo elevado
```

**Para cambiar la pose de agarre:** modifica los valores del vector `point.positions`. Usa `xolo_jtc` (Teach Pendant) para encontrar los valores correctos de forma visual.

### `moverHombro()` — fase de elevación post-agarre

Se ejecuta una vez después de que se detecta contacto. Define la pose final con la lata en la mano.

```cpp
point.positions = {
    0.159, 0.0, -0.5, 1.5010, -1.068, -1.1309,  // brazo levantado
    1.5708, 0.2094, 0.5236,   // pulgar cerrado
    0.80,   0.6109, 0.6981,   // índice cerrado
    1.1345, 0.6109, 0.6109,   // cordial cerrado
    1.1345, 0.6109, 0.6109,   // anular cerrado
    0.80,   0.6109, 0.6109    // meñique cerrado
};
```

### Agregar una nueva fase después de la elevación

Después de `levantando = true` en `moverHombro()`, puedes agregar un nuevo timer para una tercera fase:

```cpp
// Al final de moverHombro(), agregar:
temporizadorDeposito = this->create_timer(
    std::chrono::seconds(6),  // 6s después de iniciar la elevación
    std::bind(&SimulationController::depositarObjeto, this));

// Nueva función:
void SimulationController::depositarObjeto() {
    temporizadorDeposito->cancel();
    trajectory_msgs::msg::JointTrajectory msg;
    msg.header.stamp = this->now() + rclcpp::Duration::from_seconds(0.5);
    // ... define la pose de depósito
    jointTrajectoryPub->publish(msg);
}
```

---

## 5. Agregar un nuevo sensor de contacto

### Paso 1 — Agregar el sensor en el SDF del brazo

Abre `src/xolobot_arm/models/xolobot_arm/xolobot_arm.sdf` y dentro del link que quieres instrumentar agrega:

```xml
<sensor name="nuevo_sensor" type="contact">
  <contact>
    <collision>nombre_del_link::collision</collision>
  </contact>
</sensor>
```

### Paso 2 — Registrar el bridge en `bridge.yaml`

Abre `src/xolobot_arm/config/bridge.yaml` y agrega:

```yaml
- ros_topic_name: /bumper_states_nuevo
  gz_topic_name: /world/default/model/xolobot_arm/link/nombre_del_link/sensor/nuevo_sensor/contact
  ros_type_name: ros_gz_interfaces/msg/Contacts
  gz_type_name: gz.msgs.Contacts
  direction: GZ_TO_ROS
```

### Paso 3 — Suscribirse en `SimulationController.cpp`

En el constructor, agrega el suscriptor:

```cpp
suscriptorNuevo = this->create_subscription<ros_gz_interfaces::msg::Contacts>(
    "/bumper_states_nuevo", rclcpp::SensorDataQoS(),
    std::bind(&SimulationController::deteccionColision, this, std::placeholders::_1));
```

Y declara el miembro en `SimulationController.h`:

```cpp
rclcpp::Subscription<ros_gz_interfaces::msg::Contacts>::SharedPtr suscriptorNuevo;
```

---

## 6. Recetas comunes

### Cambiar la lata de caja rectangular a cilindro

**Archivo:** `src/xolobot_arm/models/utileria/objeto.sdf`

Reemplaza ambos bloques `<geometry>` (el de `<collision>` y el de `<visual>`):

```xml
<!-- ANTES -->
<box><size>0.05 0.05 0.12</size></box>

<!-- DESPUÉS — dimensiones de una lata de refresco estándar -->
<cylinder>
  <radius>0.033</radius>   <!-- 6.6 cm de diámetro -->
  <length>0.122</length>   <!-- 12.2 cm de altura -->
</cylinder>
```

Actualiza también la inercia en `<inertial>`. La masa usada es 2.0kg (no la real de ~0.4kg) para evitar que el cilindro ruede sobre el soporte plano:

```xml
<!-- Cilindro m=2.0kg (estabilidad), r=0.033m, h=0.122m -->
<mass>2.0</mass>
<ixx>0.003</ixx>
<iyy>0.003</iyy>
<izz>0.001</izz>
```

**Nota sobre el agarre:** el cilindro tiene 6.6 cm de diámetro vs. los 5 cm de la caja anterior. Los dedos necesitan abrirse ligeramente más. Usa `xolo_jtc` para recalibrar la apertura de los dedos en la pose de agarre.

---

### Cambiar la posición del objeto objetivo

Modifica la etiqueta `<pose>` en `objeto.sdf` y la misma coordenada en `soporte.sdf`:

```xml
<!-- objeto.sdf -->
<pose>NUEVO_X NUEVO_Y 0.846173 0 0 0</pose>

<!-- soporte.sdf — mismo X/Y que el objeto -->
<pose>NUEVO_X NUEVO_Y 0.816173 0 0 0</pose>
```

Después de mover el objeto, recalibra la pose de agarre en `SimulationController.cpp` con el Teach Pendant (`xolo_jtc`).

---

### Calibrar una nueva pose de agarre con el Teach Pendant

1. Lanza `xolo_sim` (sin `xolo_brain` para que el brazo no se mueva solo).
2. Ejecuta `xolo_jtc` — abre el controlador manual.
3. Mueve cada articulación hasta la pose deseada usando los sliders.
4. Lee los valores de posición de cada joint.
5. Copia esos valores al vector `point.positions` en `generaAleatorios()`.

---

## 7. Reglas que no deben romperse

| Regla | Por qué |
|---|---|
| Siempre `this->now() + 0.5s` en el header de la trayectoria | El JTC rechaza silenciosamente cualquier trayectoria con timestamp en el pasado |
| Usa `create_timer()`, nunca `create_wall_timer()` para lógica de trayectoria | Los timers deben sincronizarse con `/clock` (sim time), no con el reloj del sistema |
| `use_sim_time:=true` es obligatorio en el nodo brain | Sin él, el tiempo del nodo y el de Gazebo divergen |
| Los 21 joints deben listarse en orden exacto en `joint_names` | Un joint en posición equivocada aplica el movimiento al joint incorrecto |
| No uses `static>true</static>` en objetos que el brazo debe mover | Un modelo estático no responde a fuerzas ni colisiones dinámicas |
