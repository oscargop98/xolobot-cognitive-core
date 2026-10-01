# DEV_CONTEXT.md — Xolobot Cognitive Core
**Documento técnico de contexto y traspaso del proyecto**
Última actualización: 2026-10-01

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
1. **Warm-up** — espera 4 ticks del timer (~10s) para que `/clock` fluya desde Gazebo
2. **Planificación** — genera trayectoria de 2 puntos: waypoint alto (t=1.5s) + pose final (t=3s)
3. **Ejecución** — JTC interpola las 21 articulaciones
4. **Contacto** — bumper detecta colisión palma/dedo con el objeto
5. **Agarre** — cierra dedos + publica `attach_pub_` → `/xolobot_arm/attach` (actualmente sin efecto real: el plugin `DetachableJoint` está desactivado, ver "Bugs ya resueltos")
6. **Elevación** — `moverHombro()` publica trayectoria de elevación suave; timer de 5s

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
- **Lata:** `objeto.sdf` (coke_can) — geometría **cilindro** (radio=0.033m, largo=0.122m, masa=2.0kg), posicionada sobre el pedestal. Nota: la masa elevada (2.0kg vs real ~0.4kg) es intencional para evitar que el cilindro ruede en la superficie plana del soporte.

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

- **Phantom Empty del bridge → plugin `DetachableJoint` desactivado; agarre vía `gz topic`:** al iniciar, el bridge disparaba un `Empty` fantasma que provocaba un acople/desacople falso en t=0. **Solución aplicada (2026-10-01):** el bloque `DetachableJoint` quedó **comentado** en `xolobot_arm.sdf` y el agarre se reemplazó por llamadas directas vía `system()`:
  - Constructor: `create_wall_timer(3s)` → `system("gz topic -t /xolobot_arm/magnet_off ...")` (suelta cualquier objeto adherido al inicio)
  - `agarre_objeto()`: `system("gz topic -t /xolobot_arm/magnet_on ...")` (activa el imán al detectar contacto)
  - El tópico `/xolobot_arm/attach` y `attach_pub_` fueron eliminados del código.
  - **Nota:** el tópico `magnet_on`/`magnet_off` en Gazebo Harmonic requiere un plugin adicional en el SDF. Si no está configurado, `system("gz topic ...")` no produce efecto real. El agarre es funcional por la pose cinemática; la sujeción magnética es trabajo pendiente.
- **Constructor bloqueante:** `sleep_for()` en el constructor bloqueaba `spin()` y `/clock` nunca llegaba. Solución: `create_wall_timer` con lambda de un solo disparo.
- **Antebrazo en filtro de colisión:** el filtro incluía `link_antebrazo_izq` que generaba falsos positivos. Solución: filtro estricto — solo palma y las 5 yemas de los dedos.
- **Arc interpolation + pedestal:** JTC interpola en curva cúbica causando que el antebrazo choque con el pedestal. Solución definitiva: trayectoria de 2 puntos (waypoint alto en t=1.5s, descenso en t=3.0s).

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

## 6. Punto de Partida — Historial de sesiones

### Sesión 2026-09-24 (reorganización y puesta en marcha)

1. **Reorganización del repositorio:** se creó `xolobot-cognitive-core` en GitHub, se renombró la rama a `main`, y se copió el código desde `mi-brazo-robot-ros2-iron`. La carpeta vieja recibió `COLCON_IGNORE` y fue desconectada del remote.
2. **Limpieza y rebuild limpio:** se eliminaron `build/`, `install/`, `log/` y se recompiló en limpio. Los 3 paquetes compilaron sin errores.
3. **README.md reestructurado:** alias, instrucciones de instalación, opciones de ejecución (nativa + Docker).
4. **Reporte de servicio social Trimestre 1:** `docs/Trimestre 26-P Reporte.md` commiteado (Meses 1–3).
5. **Auditoría de infraestructura:** workspace saneado, sin contaminación de entorno Iron.

### Sesión 2026-10-01 (forma del objeto, timing, documentación)

1. **`objeto.sdf` → cilindro:** la lata cambió de `<box>` a `<cylinder>` con dimensiones reales de lata de refresco (radio=0.033m, largo=0.122m). La masa quedó en 2.0kg para estabilidad física sobre el soporte plano.
2. **Optimización de timing en `SimulationController.cpp`:**
   - Warm-up: 6 ticks → 4 ticks (~10s)
   - Waypoint: t=2.5s → t=1.5s
   - Pose final: t=5s → t=3s
   - Timer de elevación: 8s → 5s (tanto en `deteccionColision` como en `deteccionColisionPalma`)
3. **`AI_CONTEXT.md` renombrado a `DEV_CONTEXT.md`** — sin lenguaje relacionado con IA.
4. **`docs/PROGRAMMING_GUIDE.md` creado** — manual técnico completo para quien quiera extender el sistema: ciclo de simulación, mapa de archivos, recetas SDF, guía de modificación de `SimulationController.cpp`, cómo agregar sensores, reglas no negociables.
5. **README.md actualizado:** flujo de actualización del workspace (Sección 7), recomendación de 300 GB de almacenamiento con desglose por capa, guía de código, advertencia Docker como caso especial.
6. **Reporte Trimestre 2:** `docs/Trimestre 26-P Reporte 2.md` commiteado (Meses 4–6, julio–septiembre 2026).

### Estado actual del sistema

```
~/migration_ws/
  colcon build → 3 packages finished [OK]
  install/setup.bash → activo

~/migration_ws/src/xolobot-cognitive-core/
  git branch → main
  git remote → https://github.com/oscargop98/xolobot-cognitive-core.git
  commits pendientes de push: 7ac7144, 9f5cb09, 357f7b8 (documentación oct-2026)

~/migration_ws/src/mi-brazo-robot-ros2-iron/
  COLCON_IGNORE → presente
  git remote → eliminado (desconectado de GitHub)
```

> **Nota:** después de aplicar los cambios de timing y geometría, se requiere:
> ```bash
> cd ~/migration_ws
> colcon build --packages-select xolobot_arm xolobot_arm_server
> source install/setup.bash
> ```

### Por dónde continuar

El sistema de agarre es funcional. Las siguientes líneas de trabajo posibles son:

- **Documentación de referencia:** `docs/PROGRAMMING_GUIDE.md` explica el ciclo completo, recetas de modificación y reglas a no romper — leerlo antes de modificar el código.
- **Calibración post-cilindro:** el cilindro tiene 6.6 cm de diámetro (vs. 5 cm de la caja anterior). Los dedos pueden necesitar recalibración de apertura. Usar `xolo_jtc` (Teach Pendant) para ajustar los valores de `point.positions` en `generaAleatorios()`.
- **Integración de módulos cognitivos superiores:** el proyecto espera la implementación de Corteza Prefrontal (planificación de secuencias) e Hipocampo (memoria episódica) como nodos ROS 2 adicionales que se conecten al `SimulationController`.
- **Reactivar `DetachableJoint`:** el plugin magnético está desactivado (comentado en el SDF). El nodo publica el tópico `/xolobot_arm/attach` pero no tiene efecto real. Reactivar requiere descomentar el bloque del plugin en `xolobot_arm.sdf` y agregar el tópico en el bridge.
- **Bitácoras:** revisar `src/xolobot-cognitive-core/Bitacora/` para contexto de decisiones técnicas anteriores antes de modificar timing, trayectorias o configuración del bridge.
