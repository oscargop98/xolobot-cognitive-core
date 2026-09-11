# Xolobot Cognitive Core
> **⚠️ Prerrequisito del Sistema (Tier 1)**
> Este proyecto está diseñado para ejecutarse en **Ubuntu Linux - Noble (24.04) 64-bit**. 
> Antes de proceder con la instalación del workspace, es obligatorio tener **ROS 2 Jazzy Jalisco** instalado en tu equipo. Las instrucciones oficiales de instalación no se incluyen en este documento, pero puedes seguirlas desde el sitio oficial de ROS:
> 🔗 [Ubuntu Development Setup - ROS 2 Jazzy](https://docs.ros.org/en/jazzy/Installation/Alternatives/Ubuntu-Development-Setup.html)

Repositorio principal del programa **Apoyo para implementar la Arquitectura Cognitiva Inspirada en Neuronas Espejo**. Contiene la migración y modernización del entorno de simulación del manipulador antropomórfico Xolobot hacia **ROS 2 Jazzy Jalisco** y **Gazebo Harmonic**.

Incluye la implementación de los módulos cognitivos del robot simulado:
- **Corteza Premotora** — nodo `SimulationController` en C++
- **Corteza Motora Primaria** — `JointTrajectoryController` vía `ros2_control`
- **Cortezas Somatosensorial y Parietal** — 7 sensores de contacto (bumpers) en Gazebo

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

Para cualquier equipo que no cuente con ROS 2 Jazzy instalado nativamente. Los contenedores `sim` y `brain` comparten red e IPC para garantizar la comunicación entre el simulador y el nodo cognitivo.

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