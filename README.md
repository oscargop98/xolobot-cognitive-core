# Xolobot Cognitive Core

> **⚠️ Prerrequisito del Sistema (Tier 1)**
> Este proyecto está diseñado para ejecutarse en **Ubuntu Linux - Noble (24.04) 64-bit**.
> Antes de proceder con la instalación del workspace, es obligatorio tener **ROS 2 Jazzy Jalisco instalado estrictamente vía apt** (`sudo apt install ros-jazzy-desktop`), siguiendo las instrucciones oficiales:
> 🔗 [Ubuntu Development Setup - ROS 2 Jazzy](https://docs.ros.org/en/jazzy/Installation/Alternatives/Ubuntu-Development-Setup.html)
>
> **🛑 NO compiles ROS 2 desde código fuente en esta máquina.** Un ROS 2 Jazzy compilado desde fuente conviviendo con el paquete `ros-jazzy-desktop` de apt produce dos instalaciones con `rclcpp` binariamente incompatibles entre sí. Si ambas llegan a sourcearse en algún momento (aunque sea en contextos distintos), el resultado son builds que enlazan contra una y corren contra otra — símbolos indefinidos, `ros2` roto, fallos intermitentes difíciles de diagnosticar. Esto ya causó una ruptura de entorno real en este proyecto; ver el detalle en `docs/handover/DEV_CONTEXT.md` → "Instalación de Jazzy — apt obligatorio".

Repositorio principal del programa **Apoyo para implementar la Arquitectura Cognitiva Inspirada en Neuronas Espejo**. Contiene la migración y modernización del entorno de simulación del manipulador antropomórfico Xolobot hacia **ROS 2 Jazzy Jalisco** y **Gazebo Harmonic**.

Incluye la implementación de los módulos cognitivos del robot simulado:
- **Corteza Premotora** — nodo `SimulationController` en C++
- **Corteza Motora Primaria** — `JointTrajectoryController` vía `ros2_control`
- **Cortezas Somatosensorial y Parietal** — 7 sensores de contacto (bumpers) en Gazebo

---

## 📚 Documentación de Referencia y Bitácoras

Durante el desarrollo y migración de este proyecto, se documentó el proceso de investigación y configuración de forma paralela. Estos documentos sirven para entender el contexto histórico de la migración y las decisiones técnicas adoptadas:

* 📝 **[Bitácora Raíz y Borradores del Proyecto](https://app.notion.com/p/Servicio-Social-ROS-2-2390a9b6c48c8071a7b2f5323f16512e)**: Notas de partida, planeación y estructuración inicial del Servicio Social.
* ⚙️ **[Configuración de Entorno y `.bashrc` (ROS 2 Jazzy)](https://app.notion.com/p/Archivo-bashrc-ROS-2-Jazzi-Jalisco-3680a9b6c48c808f9b66f4022fd2933b)**: Documentación detallada con resultados precisos sobre la inyección de comandos, creación de alias y despliegue del entorno nativo.
---

## 🧠 Guía para entender el código

Esta sección es para quien quiera estudiar, modificar o extender el sistema — no solo ejecutarlo. El proyecto tiene tres paquetes ROS 2, cada uno con una responsabilidad distinta.

### Los tres paquetes y qué hace cada uno

| Paquete | Tipo | Qué contiene |
|---|---|---|
| `xolobot_arm` | Recursos | Modelo SDF del robot y la escena (`.sdf`), archivo de lanzamiento principal (`.launch.py`), configuración del bridge Gazebo↔ROS 2 (`bridge.yaml`) |
| `xolobot_arm_server` | Nodo C++ | Toda la lógica cognitiva: trayectorias, detección de colisión, agarre. **Aquí vive la inteligencia del robot.** |
| `xolobot_control` | Configuración | Parámetros del `JointTrajectoryController` en YAML — tolerancias, nombres de joints, tipo de controlador |

### Dónde está cada módulo cognitivo

Si quieres modificar un módulo específico de la arquitectura, este es el archivo donde buscar:

| Módulo cognitivo | Archivo | Qué modificar |
|---|---|---|
| **Corteza Premotora** — planificación y trayectorias | `xolobot_arm_server/src/SimulationController.cpp` | Función `generaAleatorios()`: define los puntos de la trayectoria, los tiempos y la secuencia de aproximación |
| **Corteza Motora Primaria** — ejecución articular | `xolobot_control/config/xolobot_control.yaml` | Tolerancias del JTC, frecuencia de muestreo, nombres de joints |
| **Cortezas Somatosensorial y Parietal** — tacto | `xolobot_arm/config/bridge.yaml` | Mapeo de sensores Gazebo→ROS 2; agregar un nuevo bumper significa agregar un bloque aquí |
| **Memoria Procedural** — plan motor almacenado | `xolobot_arm_server/src/SimulationController.cpp` | Función `moverHombro()`: define la pose de elevación post-agarre |
| **Modelo físico del robot** | `xolobot_arm/models/xolobot_arm/xolobot_arm.sdf` | Geometría, masa, joints, colisiones y plugins del simulador |
| **Escena y objetos** | `xolobot_arm/models/` (`soporte.sdf`, `objeto.sdf`) | Posición de la mesa y la lata; modificar coordenadas X/Y aquí si cambias la pose objetivo |

### Flujo de datos (de lo más simple a lo más complejo)

```
[Gazebo Harmonic]
   → publica contacto en /world/.../sensor/.../contact
   → ros_gz_bridge lo traduce a /bumper_states_*   ← bridge.yaml define este mapeo
   → SimulationController.cpp lo recibe como Subscriber

[SimulationController]  ← este es el nodo "cerebro"
   → genera JointTrajectory con 21 posiciones articulares
   → lo publica en /joint_trajectory_controller/joint_trajectory

[JointTrajectoryController]  ← dentro de Gazebo, vía ros2_control
   → interpola la trayectoria y mueve el robot articulación por articulación
```

### Reglas que no deben romperse al modificar el código

1. **Siempre usa `this->now()` para el timestamp de la trayectoria**, nunca `rclcpp::Time(0)`. El JTC rechaza silenciosamente cualquier trayectoria con timestamp en el pasado.
2. **Usa `create_timer()`, nunca `create_wall_timer()`**. Los timers deben sincronizarse con el reloj de simulación (`/clock`), no con el reloj del sistema.
3. **El parámetro `use_sim_time:=true` es obligatorio** en el nodo `xolobot_arm_server`. Sin él, el tiempo de simulación y el del nodo divergen y las trayectorias son rechazadas.
4. **Los 21 joints deben nombrarse en orden exacto** en cada `JointTrajectory`. Un joint omitido o en distinto orden hace que el controlador aplique el movimiento al joint equivocado.

### Cómo agregar un nuevo comportamiento

El punto de extensión natural es `SimulationController.cpp`. La máquina de estados actual tiene estas fases:

```
warm-up (15s) → aproximación → contacto detectado → agarre → elevación
```

Para agregar una nueva fase (por ejemplo, depositar el objeto en otra posición), añade un nuevo timer o estado después de `levantando = true` en `moverHombro()`, y publica una nueva `JointTrajectory` con la pose objetivo.

Para consultar el estado detallado de la arquitectura, los joints calibrados y las decisiones de diseño del sistema, revisa el documento técnico `docs/handover/DEV_CONTEXT.md`.

---

## 🛠️ Instalación desde cero y Resolución de Problemas

Sigue estos pasos si es la primera vez que configuras el workspace, o si necesitas limpiar una compilación rota.

### 1. Clonar el repositorio dentro del workspace de ROS 2

```bash
mkdir -p ~/migration_ws/src
cd ~/migration_ws/src
git clone https://github.com/oscargop98/xolobot-cognitive-core.git
```

### 2. Inyectar el entorno de ROS 2 Jazzy

```bash
source /opt/ros/jazzy/setup.bash
```

> **Importante:** Nunca sources ROS 2 Iron antes de Jazzy en la misma sesión. Si lo hiciste, abre una terminal nueva antes de continuar.

### 3. Limpiar caché de compilaciones anteriores (si aplica)

Si estás resolviendo un build roto o migraste de una ruta de workspace anterior, elimina los directorios generados antes de recompilar:

```bash
cd ~/migration_ws
rm -rf build/ install/ log/
```

### 4. Instalar dependencias y compilar

```bash
cd ~/migration_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build
```

### 5. Activar el workspace compilado

```bash
source ~/migration_ws/install/setup.bash
```

### 6. (Opcional) Inyectar los alias de desarrollo en tu shell

```bash
bash ~/migration_ws/src/xolobot-cognitive-core/setup_aliases.sh
source ~/.bashrc
```

### 7. Mantener el workspace actualizado

Cuando el repositorio recibe nuevos cambios (código, configuración, modelos SDF), sigue este flujo para actualizar tu instalación local:

```bash
cd ~/migration_ws/src/xolobot-cognitive-core
git pull origin main
```

Si los cambios solo afectan archivos de lanzamiento (`.launch.py`), configuración YAML o documentación, **no es necesario recompilar**. Solo vuelve a sourcear:

```bash
source ~/migration_ws/install/setup.bash
```

Si los cambios tocan código C++ (`src/SimulationController.cpp` u otros `.cpp`/`.hpp`) o `CMakeLists.txt`, recompila el paquete afectado:

```bash
cd ~/migration_ws
colcon build --packages-select xolobot_arm_server
source install/setup.bash
```

> **Importante:** Si después de un `git pull` también ejecutaste `sudo apt upgrade`, realiza un rebuild limpio completo para evitar conflictos de versiones de librerías:
> ```bash
> cd ~/migration_ws
> rm -rf build/ install/ log/
> colcon build
> source install/setup.bash
> ```

---

## 💾 Requerimientos de almacenamiento

El stack tecnológico de este proyecto (ROS 2 + Gazebo Harmonic + Docker) tiene un consumo de disco inherentemente alto.

> **Se recomienda asignar al menos 300 GB a la partición de Linux al instalar Ubuntu.** Esta cifra contempla el stack completo de ROS 2 + Gazebo Harmonic, imágenes Docker, caché de compilación activa, logs de ejecución, y margen operativo para el desarrollo sostenido. Una partición de 200 GB puede llenarse al llegar a la fase de pruebas intensivas.

### Por qué el proyecto ocupa tanto espacio

| Componente | Espacio aproximado | Descripción |
|---|---|---|
| ROS 2 Jazzy Desktop (`/opt/ros/jazzy/`) | 3–4 GB | Framework DDS, `rviz2`, `rqt`, herramientas CLI y dependencias |
| Gazebo Harmonic (`gz-harmonic`) | 4–6 GB | Motor de física (Bullet/DART), renderizado Ogre2, modelos 3D base |
| Caché de compilación (`build/`) | 1–2 GB | Archivos objeto `.o`, `CMakeCache.txt`, artefactos de Ninja — regenerables |
| Imágenes Docker (`/var/lib/docker/`) | 8–12 GB por imagen | Contiene todo lo anterior pre-compilado; varias versiones acumulan más |
| Logs de ejecución (`~/.ros/log/`) | 0.5–2 GB | Crece con cada sesión de trabajo; se puede limpiar con `rm -rf ~/.ros/log/` |

### Recomendaciones

- **Partición recomendada al instalar Ubuntu:** **300 GB** (nativo + Docker + margen de trabajo).
- Si ya tienes una instalación con espacio limitado y cuentas con una partición Windows o NTFS disponible, puedes montarla como volumen auxiliar sin riesgo:
  ```bash
  # Verificar UUID de la partición NTFS
  sudo blkid /dev/sdXY

  # Montar permanentemente (agregar a /etc/fstab)
  UUID=<tu-UUID>  /mnt/datos  ntfs-3g  defaults,uid=1000,gid=1000,umask=022,nofail  0  0
  ```
- Limpia el caché de compilación cuando termines una sesión de desarrollo intensivo: `rm -rf ~/migration_ws/build/ ~/migration_ws/log/`.

---

## 🚀 Ejecución del Proyecto

### Opción A: Ejecución Nativa

Requiere **Ubuntu 24.04** y **ROS 2 Jazzy Jalisco** instalados en el host.

El proyecto incluye un panel de alias diseñados para operar y depurar la simulación rápidamente desde la terminal.

#### Panel de Control — Alias disponibles

| Alias | Función |
|---|---|
| `xolo_sim` | Compila el servidor y lanza Gazebo Harmonic (Corteza Motora) |
| `xolo_brain` | Lanza el nodo de control autónomo (Corteza Premotora) |
| `xolo_kill` | Termina todos los procesos de ROS 2 y Gazebo |
| `xolo_cam` | Muestra las coordenadas actuales de la cámara en Gazebo |
| `xolo_view` | Mueve la cámara a la pose ideal de observación del agarre |
| `xolo_jtc` | Abre el Teach Pendant (`rqt_joint_trajectory_controller`) para calibración manual |
| `xolo_kill_jtc` | Cierra el controlador manual |
| `xolo_float` | Depura la lata flotante publicando en el tópico `magnet_off` |

#### Flujo de ejecución

```bash
# Terminal 1 — Simulador físico + JointTrajectoryController
xolo_sim

# Terminal 2 — Nodo cognitivo (espera ~15s de warm-up antes de enviar trayectorias)
xolo_brain
```

> Si necesitas matar todos los procesos en cualquier momento: `xolo_kill`

---

### Opción B: Ejecución con Docker

Diseñada para entornos donde **no es viable o conveniente instalar el stack completo de ROS 2 + Gazebo directamente en el sistema operativo del host**. Casos de uso principales:

- **Servidores de laboratorio compartidos** — donde instalar ROS 2 Jazzy afectaría a otros usuarios o proyectos que usen versiones distintas del framework.
- **Máquinas con Ubuntu 22.04 u otra versión no compatible** — Docker provee internamente Ubuntu 24.04 sin necesidad de reinstalar el sistema.
- **Entornos de integración o despliegue remoto** — donde se necesita reproducibilidad exacta del entorno sin depender de la configuración del host.
- **Desarrollo paralelo de múltiples versiones** — cada contenedor es un entorno aislado; cambiar de versión es tan simple como cambiar la imagen.

> **⚠️ Caso de uso restringido.** La Opción B está pensada exclusivamente para situaciones donde no es posible instalar ROS 2 Jazzy de forma nativa: servidores de laboratorio compartidos, máquinas con Ubuntu 22.04 u otra versión incompatible, o entornos de despliegue remoto. En la práctica, **casi siempre se ejecutará de forma local** sobre el equipo de desarrollo — no es una opción para producción en red ni para correr el simulador en la nube, ya que Gazebo Harmonic requiere acceso a GPU y display local. Para cualquier integrante del laboratorio con Ubuntu 24.04, la **Opción A (Nativa)** es la correcta.

Los contenedores `sim` y `brain` comparten red e IPC para garantizar la comunicación entre el simulador y el nodo cognitivo.

#### Prerrequisito: habilitar el forwarding gráfico (una vez por sesión)

```bash
xhost +local:docker
```

#### Construir la imagen y levantar el entorno completo

```bash
cd ~/migration_ws/src/xolobot-cognitive-core
docker compose up --build
```

La imagen se construye en ~10–15 minutos la primera vez. Las ejecuciones posteriores son instantáneas (el workspace ya está pre-compilado en la imagen).

#### Arranque manual con dos terminales separadas

```bash
# Terminal 1 — Gazebo + controladores
docker compose up sim

# Terminal 2 — Nodo cognitivo
docker compose run brain
```