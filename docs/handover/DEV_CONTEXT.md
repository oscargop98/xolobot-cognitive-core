# DEV_CONTEXT.md — Xolobot Cognitive Core
**Documento técnico de contexto y traspaso del proyecto**
Última actualización: 2026-09-24

---

## 1. Resumen del Proyecto

**Xolobot** es un brazo robótico antropomórfico de 21 grados de libertad con una mano de cinco dedos, desarrollado en el marco del programa de servicio social:

> *"Apoyo para implementar la Arquitectura Cognitiva Inspirada en Neuronas Espejo"*
> UAM Cuajimalpa — Responsable: Dra. Alicia Montserrat Alvarado González

El objetivo es implementar una arquitectura cognitiva inspirada en neuronas espejo que permita al robot ejecutar agarres autónomos reactivos. Cada módulo del sistema simula una región cortical:

| Módulo cognitivo | Implementación técnica |
|---|---|
| Corteza Premotora | Nodo `SimulationController` en C++ |
| Corteza Motora Primaria | `JointTrajectoryController` (ros2_control) |
| Corteza Somatosensorial/Parietal | 7 sensores bumper en Gazebo + bridge YAML |
| Memoria Procedural | Algoritmo de waypoint cinemático ajustado por retroalimentación |

**Estado actual:** la simulación ejecuta agarre completo de una lata de refresco — aproximación con waypoint evasivo, detección de contacto, cierre de dedos, activación magnética y elevación.

---

## 2. Arquitectura y Paquetes

### Workspace

```
~/migration_ws/
├── src/
│   ├── xolobot-cognitive-core/          ← REPOSITORIO ACTIVO
│   │   ├── src/
│   │   │   ├── xolobot_arm/             ← Modelo, mundo Gazebo, launch, bridge
│   │   │   ├── xolobot_arm_server/      ← Nodo C++ de control autónomo
│   │   │   └── xolobot_control/         ← Config JointTrajectoryController
│   │   ├── Dockerfile
│   │   ├── docker-compose.yml
│   │   ├── setup_aliases.sh
│   │   └── docs/                        ← Reporte trimestral 26-P + este archivo
│   └── mi-brazo-robot-ros2-iron/        ← LEGACY — tiene COLCON_IGNORE, NO tocar
├── build/
├── install/
└── log/
```

> **Crítico:** `mi-brazo-robot-ros2-iron` tiene `COLCON_IGNORE` y fue desconectado del remote git. Colcon lo ignora completamente. El código activo vive exclusivamente en `xolobot-cognitive-core`.

### Paquetes ROS 2

| Paquete | Tipo | Rol |
|---|---|---|
| `xolobot_arm` | ament_cmake (recursos) | SDF/URDF del robot, mundo Gazebo, launch principal, bridge config |
| `xolobot_arm_server` | ament_cmake (C++) | Toda la lógica de control: trayectorias, detección de colisión, agarre |
| `xolobot_control` | ament_cmake (config) | YAML del `JointTrajectoryController`, launch auxiliar |

### Flujo de datos

```
Gazebo Harmonic (gz-sim8)
  ↓  ros_gz_bridge  [nodo `bridge` inline en xolobot_arm_control.launch.py]
/clock                          → SimulationController (sincronización temporal)
/bumper_states_palma            ↘
/bumper_states_pulgar_3          → SimulationController: detección de contacto
/bumper_states_indice_3          ↗ (5 dedos + palma, el antebrazo está excluido)
/bumper_states_{cordial,anular,menique}_3
  ↓
SimulationController.cpp  [xolobot_arm_server/src/]
  ↓  publica
/joint_trajectory_controller/joint_trajectory  →  JTC  →  Gazebo (mueve 21 joints)
/xolobot_arm/attach (std_msgs/Empty)           →  bridge  →  DetachableJoint plugin (DESACTIVADO, ver "Bugs ya resueltos")
```

### Nodo de control: `SimulationController.cpp`

**El control está en C++, NO en Python.** Archivo principal: `src/xolobot_arm_server/src/SimulationController.cpp`.

Máquina de estados:
1. **Warm-up** — espera 6 ticks del timer (~15s) para que `/clock` fluya desde Gazebo
2. **Planificación** — genera trayectoria de 2 puntos: waypoint alto (t=2.5s) + pose final (t=5s)
3. **Ejecución** — JTC interpola las 21 articulaciones
4. **Contacto** — bumper detecta colisión palma/dedo con el objeto
5. **Agarre** — cierra dedos + publica `attach_pub_` → `/xolobot_arm/attach` (actualmente sin efecto real: el plugin `DetachableJoint` está desactivado, ver "Bugs ya resueltos")
6. **Elevación** — `moverHombro()` publica trayectoria de elevación suave (5s)

Flags estáticos importantes:
```cpp
static int contador_inicio = 0;       // warm-up counter
static bool pose_acercamiento_enviada = false;  // one-shot lock
bool levantando = false;              // bloquea re-envío de trayectorias
bool colisionDetectada = false;       // trigger de agarre
```

---

## 3. Simulación y Modelo del Robot

### Gazebo Harmonic — NO Gazebo Classic

El proyecto usa **Gazebo Harmonic (gz-sim8)**. Comandos de Gazebo Classic (`gzserver`, `gzclient`, `gazebo`) no existen aquí.

### Mundo y objetos

- **Mundo:** `src/xolobot_arm/worlds/coca_levitando.world`
- **Pedestal/soporte:** `soporte.sdf` — mesa estática en X: 0.2709, Y: 0.2567
- **Lata:** `objeto.sdf` (coke_can) — posicionada sobre el pedestal

### Cámara

- **Config GUI:** `src/xolobot_arm/config/view_front.config`
- **Pose de cámara:** `0.212 1.133 0.890 0.0 0.2260 -1.4149` (roll-pitch-yaw)
- Se pasa al launch con `--gui-config` porque el bloque `<gui>` del SDF es ignorado por Gazebo Harmonic.

### Bridge de comunicación

**No existe un `bridge.yaml`.** El bridge (`ros_gz_bridge`/`parameter_bridge`) está definido inline como el nodo `bridge` dentro de `src/xolobot_arm/launch/xolobot_arm_control.launch.py`. Bridgea, con el mismo nombre a ambos lados (Gazebo ↔ ROS 2):
```
/clock                     @ rosgraph_msgs/msg/Clock
/bumper_states_palma       @ ros_gz_interfaces/msg/Contacts
/bumper_states_antebrazo   @ ros_gz_interfaces/msg/Contacts
/bumper_states_pulgar_3    @ ros_gz_interfaces/msg/Contacts
/bumper_states_indice_3    @ ros_gz_interfaces/msg/Contacts
/bumper_states_cordial_3   @ ros_gz_interfaces/msg/Contacts
/bumper_states_anular_3    @ ros_gz_interfaces/msg/Contacts
/bumper_states_menique_3   @ ros_gz_interfaces/msg/Contacts
/xolobot_arm/attach        @ std_msgs/msg/Empty  (ROS → Gazebo, unidireccional)
```
`/clock` debe ir siempre como primera entrada para sincronización temporal.

### Articulaciones (21 DOF)

| Índice | Joint | Valor de agarre (rad) |
|---|---|---|
| 0 | `jnt_pecho_hombro` | 0.159 |
| 1 | `jnt_hombro_hombro` | 0.0 |
| 2 | `jnt_hombro_biceps` | -1.2874 (waypoint: -0.5) |
| 3 | `jnt_biceps_codo` | 1.5010 |
| 4 | `jnt_codo_antebrazo` | -1.068 |
| 5 | `jnt_antebrazo_palma` | -1.1309 |
| 6–20 | Dedos (5 × 3 falanges) | Valores de cierre calibrados |

---

## 4. Estado de la Migración (Iron → Jazzy)

### Qué cambió y por qué

| Área | Iron / Gazebo Classic | Jazzy / Gazebo Harmonic |
|---|---|---|
| Plugins SDF | `libgazebo_ros_control.so` | Plugins nativos `gz::sim::systems::*` |
| Bridge | Auto-discovery | `parameter_bridge` explícito, definido inline en `xolobot_arm_control.launch.py` (no hay `bridge.yaml`) |
| Rutas de tópicos | Cortas (`/bumper_states`) | Largas (`/world/.../contact`) |
| DetachableJoint | Acoplado por defecto | Plugin comentado/desactivado en el SDF — el agarre magnético no está realmente activo, ver "Bugs ya resueltos" |
| JTC timestamps | Permisivo | Rechaza `stamp=0` y cualquier timestamp en el pasado |
| JTC payload | Acepta `velocities` vacío | **Requiere** `velocities` y `accelerations` del mismo tamaño que `positions` |

### Bugs ya resueltos — no reabrir

- **Phantom Empty del bridge → plugin `DetachableJoint` desactivado:** al iniciar, el bridge disparaba un `Empty` fantasma en `/xolobot_arm/attach` que provocaba un acople/desacople falso en t=0. **Solución realmente aplicada:** el bloque completo `<plugin name="gz::sim::systems::DetachableJoint">` quedó **comentado** en `src/xolobot_arm/models/xolobot_arm.sdf` (no se carga en Gazebo). El nodo C++ conserva únicamente `attach_pub_` (publica `Empty` en `/xolobot_arm/attach`, bridgeado en el `launch.py`); no existe ningún tópico `detach`/`magnet_off` ni ninguna llamada `system("gz topic ...")` en el código actual. **Reactivar el plugin (SDF + bridge) sigue pendiente — no está resuelto, solo desactivado.**
- **Constructor bloqueante:** `sleep_for()` en el constructor bloqueaba `spin()` y `/clock` nunca llegaba. Solución: `create_wall_timer` con lambda de un solo disparo.
- **Antebrazo en filtro de colisión:** el filtro incluía `link_antebrazo_izq` que generaba falsos positivos. Solución: filtro estricto — solo palma y las 5 yemas de los dedos.
- **Arc interpolation + pedestal:** JTC interpola en curva cúbica causando que el antebrazo choque con el pedestal. Solución definitiva: trayectoria de 2 puntos (waypoint alto en t=2.5s, descenso en t=5.0s).

### Instalación de Jazzy — apt obligatorio (NO compilar desde fuente)

**ROS 2 Jazzy debe instalarse exclusivamente vía apt.** Nunca compilar el core de ROS 2 desde fuente en una máquina donde vaya a correr este proyecto.

> **🛑 Conflicto letal ya observado en este proyecto:** en la migración de esta máquina (sesión 2026-09-24/25) se encontró un ROS 2 Jazzy compilado desde fuente en `~/ros2_jazzy/install/`, sourceado globalmente en `~/.bashrc`. Este proyecto necesita `ros_gz_interfaces`/`ros_gz_bridge`/`gz_ros2_control`, que solo existen como paquetes apt (`ros-jazzy-*`). Instalarlos sin más habría creado **dos `rclcpp` distintos y binariamente incompatibles** conviviendo en la misma máquina (uno compilado a mano en `~/ros2_jazzy`, otro de apt en `/opt/ros/jazzy`). Si ambos llegan a sourcearse — aunque sea en contextos distintos, uno en `.bashrc` global y otro dentro de un alias — el resultado es compilar contra un `rclcpp` y ejecutar contra otro: símbolos indefinidos, `ros2` roto, builds que fallan de forma intermitente y difícil de diagnosticar. **Esta es, con alta probabilidad, la causa real de las rupturas de entorno reportadas en intentos previos de instalación de este proyecto — no el mecanismo de inyección de alias en sí.**
>
> **Solución aplicada:** se instaló el stack completo vía apt (`ros-jazzy-desktop` + paquetes de Gazebo/control de abajo), se dejó de sourcear `~/ros2_jazzy` en `.bashrc` (reemplazado por `/opt/ros/jazzy/setup.bash`, con guarda `[ -f ... ]`), y se recompiló el workspace desde cero contra el apt de Jazzy. `~/ros2_jazzy` se dejó en disco sin usarse (huérfano, inofensivo) en vez de borrarlo.
>
> **Regla para el futuro:** si esta máquina (o cualquier clon nuevo) tiene un ROS 2 compilado desde fuente, **no lo actives globalmente**. Instala todo el stack de este proyecto vía apt; si necesitas conservar el build desde fuente para otro propósito, mantenlo completamente aislado — nunca sourceado en la misma shell donde se compila o ejecuta este workspace.

```bash
sudo apt update
sudo apt install -y \
  ros-jazzy-desktop \
  ros-jazzy-ros2-control \
  ros-jazzy-ros2-controllers \
  ros-jazzy-gz-ros2-control \
  ros-jazzy-ros-gz-sim \
  ros-jazzy-ros-gz-bridge \
  ros-jazzy-ros-gz-interfaces \
  ros-jazzy-controller-manager \
  ros-jazzy-rqt-joint-trajectory-controller
```

---

## 5. Reglas y Convenciones

### Reglas críticas de desarrollo

1. **Siempre usar simulation time.** Pasar `use_sim_time:=true` a todos los nodos. Sin esto, el JTC rechaza trayectorias por desincronización de reloj.

2. **Timers de trayectoria: `create_timer()`, nunca `create_wall_timer()`.** Los timers deben sincronizarse con `/clock`. Excepción: la inicialización del imán (`magnet_off` en constructor) sí puede usar `create_wall_timer` porque ocurre antes de que el bridge esté activo.

3. **No valores en cero hardcodeados en trayectorias.** Las articulaciones requieren offsets de compensación gravitacional. El joint `jnt_hombro_hombro` tiene un offset trigonométrico de **0.758 rad (43.4°)** para compensación lateral al aproximarse al objeto.

4. **Header stamp obligatorio con offset.** Siempre usar:
   ```cpp
   msg.header.stamp = this->now() + rclcpp::Duration::from_seconds(0.5);
   ```
   El JTC rechaza cualquier trayectoria cuyo stamp sea ≤ al tiempo actual del controlador.

5. **Velocities y accelerations deben tener el mismo tamaño que positions:**
   ```cpp
   point.velocities.assign(TOTAL_JOINTS, 0.0);
   point.accelerations.assign(TOTAL_JOINTS, 0.0);
   ```

### Alias de desarrollo

Definidos en `setup_aliases.sh` e inyectados en `~/.bashrc`:

| Alias | Función |
|---|---|
| `xolo_sim` | Compila `xolobot_arm_server` y lanza Gazebo |
| `xolo_brain` | Lanza `SimulationController` con `use_sim_time:=true` |
| `xolo_kill` | Mata todos los procesos de ROS 2 y Gazebo |
| `xolo_cam` | Muestra coordenadas actuales de la cámara |
| `xolo_view` | Mueve la cámara a la pose de observación ideal |
| `xolo_jtc` | Abre el Teach Pendant RQT para calibración manual |
| `xolo_kill_jtc` | Cierra el controlador manual |
| `xolo_float` | Publica `magnet_off` para depurar la lata flotante |

### Git — Autoría

Sin Co-Authored-By en commits. La autoría es exclusivamente del usuario.

---

## 6. Punto de Partida — Última Sesión (2026-09-24)

### Lo que se hizo en esta sesión

1. **Reorganización del repositorio:** se creó `xolobot-cognitive-core` en GitHub, se renombró la rama a `main`, y se copió el código desde `mi-brazo-robot-ros2-iron`. La carpeta vieja recibió `COLCON_IGNORE` y fue desconectada del remote.

2. **Limpieza y rebuild limpio:** se eliminaron `build/`, `install/`, `log/` (que tenían 126+ referencias a paths obsoletos) y se recompiló en limpio. Los 3 paquetes compilaron sin errores.

3. **README.md reestructurado:** el archivo limpio con alias, instrucciones de instalación y opciones de ejecución (nativa + Docker) fue commiteado al repo.

4. **Reporte de servicio social:** `docs/Trimestre 26-P Reporte.md` fue generado y commiteado. Cubre los Meses 1–3 del programa, mapeando el trabajo técnico a los módulos cognitivos del proyecto.

5. **Auditoría de infraestructura completada:** workspace saneado, sin contaminación de entorno Iron, sin paths obsoletos en el caché de compilación.

### Estado actual del sistema

```
~/migration_ws/
  colcon build → 3 packages finished [OK]
  install/setup.bash → activo

~/migration_ws/src/xolobot-cognitive-core/
  git branch → main
  git remote → https://github.com/oscargop98/xolobot-cognitive-core.git
  último commit → 3446653 "docs: agregar enlaces a bitacoras de investigacion"

~/migration_ws/src/mi-brazo-robot-ros2-iron/
  COLCON_IGNORE → presente
  git remote → eliminado (desconectado de GitHub)
```

### Por dónde continuar

El sistema de agarre es funcional. Las siguientes líneas de trabajo posibles son:

- **Calibración cinemática adicional:** el offset de 0.758 rad en `jnt_hombro_hombro` puede requerir ajuste fino según la posición exacta del objeto en el mundo.
- **Integración de módulos cognitivos superiores:** el proyecto espera la implementación de Corteza Prefrontal (planificación de secuencias) e Hipocampo (memoria episódica) como nodos ROS 2 adicionales que se conecten al `SimulationController`.
- **Pruebas de Docker en más equipos:** los parches de VM (`~/.Xauthority`, `ROS_LOCALHOST_ONLY=1`, `ipc: host`) están documentados en `DOCKER.md` pero no han sido probados en equipos del laboratorio distintos al de Oscar.
- **Bitácoras:** revisar `src/xolobot-cognitive-core/Bitacora/` para contexto de decisiones técnicas anteriores antes de modificar timing, trayectorias o configuración del bridge.
